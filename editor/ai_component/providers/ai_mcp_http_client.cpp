/**************************************************************************/
/*  ai_mcp_http_client.cpp                                                */
/**************************************************************************/

#include "ai_mcp_http_client.h"

#include "core/io/json.h"
#include "core/crypto/crypto.h"
#include "core/os/os.h"

void AIMCPHTTPClient::_bind_methods() {
}

String AIMCPHTTPClient::_normalize_connection_host(const String &p_host) {
	const String host = p_host.strip_edges();
	if (host == "0.0.0.0") {
		return "127.0.0.1";
	}
	if (host == "::" || host == "[::]") {
		return "::1";
	}
	return host;
}

String AIMCPHTTPClient::normalize_connection_host_for_test(const String &p_host) {
	return _normalize_connection_host(p_host);
}

bool AIMCPHTTPClient::_parse_endpoint(const String &p_url, Endpoint &r_endpoint, String &r_error) {
	r_endpoint = Endpoint();
	String scheme;
	String host;
	String path;
	String fragment;
	int port = 0;
	Error err = p_url.strip_edges().parse_url(scheme, host, port, path, fragment);
	if (err != OK || (scheme != "https://" && scheme != "http://") || host.is_empty()) {
		r_error = "MCP HTTP server URL is invalid.";
		return false;
	}

	r_endpoint.scheme = scheme;
	r_endpoint.host = host;
	r_endpoint.path = path.is_empty() ? String("/") : path;
	r_endpoint.use_tls = scheme == "https://";
	r_endpoint.port = port == 0 ? (r_endpoint.use_tls ? 443 : 80) : port;
	return true;
}

Vector<String> AIMCPHTTPClient::_build_headers(const String &p_extra_headers, const String &p_session_id, bool p_accept_sse) {
	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back(String("Accept: application/json") + (p_accept_sse ? ", text/event-stream" : ""));
	headers.push_back("MCP-Protocol-Version: 2025-06-18");
	if (!p_session_id.is_empty()) {
		headers.push_back("Mcp-Session-Id: " + p_session_id);
	}

	Vector<String> lines = p_extra_headers.split("\n", false);
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (line.is_empty() || line.begins_with("#")) {
			continue;
		}
		if (line.contains(":")) {
			headers.push_back(line);
			continue;
		}
		const int separator = line.find("=");
		if (separator <= 0) {
			continue;
		}
		const String key = line.substr(0, separator).strip_edges();
		const String value = line.substr(separator + 1).strip_edges();
		if (!key.is_empty()) {
			headers.push_back(key + ": " + value);
		}
	}
	return headers;
}

String AIMCPHTTPClient::_join_paths(const String &p_base_path, const String &p_endpoint_path) {
	if (p_endpoint_path.is_empty()) {
		return p_base_path.is_empty() ? String("/") : p_base_path;
	}
	if (p_endpoint_path.begins_with("http://") || p_endpoint_path.begins_with("https://")) {
		String scheme;
		String host;
		String path;
		String fragment;
		int port = 0;
		if (p_endpoint_path.parse_url(scheme, host, port, path, fragment) == OK && !path.is_empty()) {
			return path;
		}
		return p_endpoint_path;
	}
	if (p_endpoint_path.begins_with("/")) {
		return p_endpoint_path;
	}
	if (p_endpoint_path.begins_with("?")) {
		String base_path = p_base_path.is_empty() ? String("/") : p_base_path;
		return base_path + p_endpoint_path;
	}

	String base_path = p_base_path.is_empty() ? String("/") : p_base_path;
	const int slash_index = base_path.rfind("/");
	if (slash_index >= 0) {
		base_path = base_path.substr(0, slash_index + 1);
	}
	return (base_path + p_endpoint_path).simplify_path();
}

bool AIMCPHTTPClient::_split_sse_data_events(const String &p_text, bool p_require_complete_events, Vector<String> &r_events) {
	r_events.clear();
	const String normalized = p_text.replace("\r\n", "\n");
	Vector<String> raw_events = normalized.split("\n\n", false);
	for (int i = 0; i < raw_events.size(); i++) {
		if (p_require_complete_events && i == raw_events.size() - 1 && !normalized.ends_with("\n\n")) {
			break;
		}
		Vector<String> lines = raw_events[i].split("\n", false);
		String data;
		for (int j = 0; j < lines.size(); j++) {
			String line = lines[j].strip_edges();
			if (!line.begins_with("data:")) {
				continue;
			}
			if (!data.is_empty()) {
				data += "\n";
			}
			data += line.substr(5).strip_edges();
		}
		if (!data.is_empty()) {
			r_events.push_back(data);
		}
	}
	return !r_events.is_empty();
}

bool AIMCPHTTPClient::_extract_sse_data_events(const String &p_text, Vector<String> &r_events) {
	return _split_sse_data_events(p_text, false, r_events);
}

int AIMCPHTTPClient::_find_sse_event_separator(const PackedByteArray &p_buffer, int &r_separator_size) {
	for (int i = 0; i < p_buffer.size() - 1; i++) {
		if (i < p_buffer.size() - 3 && p_buffer[i] == '\r' && p_buffer[i + 1] == '\n' && p_buffer[i + 2] == '\r' && p_buffer[i + 3] == '\n') {
			r_separator_size = 4;
			return i;
		}
		if (p_buffer[i] == '\n' && p_buffer[i + 1] == '\n') {
			r_separator_size = 2;
			return i;
		}
	}

	r_separator_size = 0;
	return -1;
}

bool AIMCPHTTPClient::_consume_sse_data_events(PackedByteArray &r_buffer, Vector<String> &r_events) {
	r_events.clear();
	while (true) {
		int separator_size = 0;
		const int separator_index = _find_sse_event_separator(r_buffer, separator_size);
		if (separator_index < 0) {
			break;
		}

		const PackedByteArray raw_event_bytes = r_buffer.slice(0, separator_index);
		r_buffer = r_buffer.slice(separator_index + separator_size);
		const String raw_event = raw_event_bytes.is_empty() ? String() : String::utf8(reinterpret_cast<const char *>(raw_event_bytes.ptr()), raw_event_bytes.size());
		Vector<String> event_data;
		_extract_sse_data_events(raw_event, event_data);
		for (int i = 0; i < event_data.size(); i++) {
			r_events.push_back(event_data[i]);
		}
	}

	return !r_events.is_empty();
}

bool AIMCPHTTPClient::_parse_json_or_sse_response(const String &p_response_text, int p_expected_id, Dictionary &r_result, String &r_error) {
	String response_text = p_response_text.strip_edges();
	if (response_text.is_empty()) {
		r_error = "MCP HTTP server returned an empty response.";
		return false;
	}

	if (response_text.begins_with("data:") || response_text.contains("\ndata:")) {
		Vector<String> events;
		_extract_sse_data_events(response_text, events);
		for (int i = 0; i < events.size(); i++) {
			Dictionary candidate;
			const AIMCPResponseParseStatus status = AIMCPProtocol::parse_response_line(events[i], p_expected_id, candidate, r_error);
			if (status == AI_MCP_RESPONSE_MATCHED) {
				r_result = candidate;
				return true;
			}
			if (status == AI_MCP_RESPONSE_FAILED) {
				return false;
			}
		}
		r_error = "MCP HTTP SSE response did not contain the expected JSON-RPC response.";
		return false;
	}

	return AIMCPProtocol::parse_response(response_text, p_expected_id, r_result, r_error);
}

bool AIMCPHTTPClient::_capture_response_metadata(const Ref<HTTPClient> &p_client, HTTPClient::Status p_status, ResponseMetadata &r_metadata) {
	if (r_metadata.headers_received) {
		return true;
	}
	if (p_status != HTTPClient::STATUS_BODY && !(p_status == HTTPClient::STATUS_CONNECTED && (p_client->has_response() || p_client->get_response_code() != 0))) {
		return false;
	}

	r_metadata.response_code = p_client->get_response_code();
	List<String> response_headers;
	if (p_client->get_response_headers(&response_headers) == OK) {
		for (const List<String>::Element *E = response_headers.front(); E; E = E->next()) {
			const String &header = E->get();
			Vector<String> parts = header.split(":", true, 1);
			if (parts.size() == 2 && parts[0].strip_edges().to_lower() == "mcp-session-id") {
				r_metadata.session_id = parts[1].strip_edges();
			}
		}
	}

	r_metadata.headers_received = true;
	return true;
}

bool AIMCPHTTPClient::_connect(const Endpoint &p_endpoint, Ref<HTTPClient> &r_client, String &r_error) const {
	r_client = HTTPClient::create();
	r_client->set_blocking_mode(true);
	const String connection_host = _normalize_connection_host(p_endpoint.host);
	Error err = r_client->connect_to_host(connection_host, p_endpoint.port, p_endpoint.use_tls ? Ref<TLSOptions>(TLSOptions::client()) : Ref<TLSOptions>());
	if (err != OK) {
		r_error = vformat("Failed to connect to MCP HTTP server: %s:%d", connection_host, p_endpoint.port);
		return false;
	}

	const uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	while (r_client->get_status() == HTTPClient::STATUS_CONNECTING || r_client->get_status() == HTTPClient::STATUS_RESOLVING) {
		if (_is_cancel_requested()) {
			r_error = "Tool execution cancelled.";
			return false;
		}

		r_client->poll();
		if (OS::get_singleton()->get_ticks_msec() - start_time > (uint64_t)timeout_msec) {
			r_error = connection_host == p_endpoint.host ? vformat("MCP HTTP connection timed out: %s:%d", connection_host, p_endpoint.port) : vformat("MCP HTTP connection timed out: %s:%d (configured host %s)", connection_host, p_endpoint.port, p_endpoint.host);
			return false;
		}
		OS::get_singleton()->delay_usec(1000);
	}

	if (r_client->get_status() != HTTPClient::STATUS_CONNECTED) {
		r_error = connection_host == p_endpoint.host ? vformat("MCP HTTP connection failed: status=%d host=%s port=%d", (int)r_client->get_status(), connection_host, p_endpoint.port) : vformat("MCP HTTP connection failed: status=%d host=%s port=%d configured_host=%s", (int)r_client->get_status(), connection_host, p_endpoint.port, p_endpoint.host);
		return false;
	}
	return true;
}

bool AIMCPHTTPClient::_wait_for_response_headers(const Ref<HTTPClient> &p_client, ResponseMetadata &r_metadata, String &r_error) const {
	const uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	while (true) {
		if (_is_cancel_requested()) {
			r_error = "Tool execution cancelled.";
			return false;
		}

		p_client->poll();
		HTTPClient::Status status = p_client->get_status();
		if (_capture_response_metadata(p_client, status, r_metadata)) {
			return true;
		}
		if (status == HTTPClient::STATUS_DISCONNECTED) {
			r_error = "MCP HTTP server disconnected before returning response headers.";
			return false;
		}
		if (status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
			r_error = "MCP HTTP TLS handshake failed.";
			return false;
		}
		if (status >= HTTPClient::STATUS_CONNECTION_ERROR) {
			r_error = vformat("MCP HTTP connection error: status=%d", (int)status);
			return false;
		}

		if (OS::get_singleton()->get_ticks_msec() - start_time > (uint64_t)timeout_msec) {
			r_error = "MCP HTTP request timed out while waiting for response headers.";
			return false;
		}
		OS::get_singleton()->delay_usec(1000);
	}
}

bool AIMCPHTTPClient::_read_response_body(const Ref<HTTPClient> &p_client, PackedByteArray &r_body, String &r_error, bool p_allow_open_stream, ResponseMetadata *r_metadata) const {
	r_body.clear();
	const uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	bool received_response = r_metadata && r_metadata->headers_received;
	bool received_body = false;
	while (true) {
		if (_is_cancel_requested()) {
			r_error = "Tool execution cancelled.";
			return false;
		}

		p_client->poll();
		HTTPClient::Status status = p_client->get_status();
		if (r_metadata) {
			_capture_response_metadata(p_client, status, *r_metadata);
		}
		if (status == HTTPClient::STATUS_BODY) {
			received_response = true;
			received_body = true;
			PackedByteArray chunk = p_client->read_response_body_chunk();
			if (!chunk.is_empty()) {
				r_body.append_array(chunk);
			}
		} else if (status == HTTPClient::STATUS_CONNECTED && p_client->has_response() && !received_response) {
			received_response = true;
			if (!p_allow_open_stream) {
				break;
			}
		} else if (status == HTTPClient::STATUS_CONNECTED && received_response && !received_body) {
			break;
		} else if (status == HTTPClient::STATUS_CONNECTED && received_body) {
			break;
		} else if (status == HTTPClient::STATUS_DISCONNECTED) {
			break;
		} else if (status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
			r_error = "MCP HTTP TLS handshake failed.";
			return false;
		} else if (status >= HTTPClient::STATUS_CONNECTION_ERROR) {
			r_error = vformat("MCP HTTP connection error: status=%d", (int)status);
			return false;
		}

		if (p_allow_open_stream && !r_body.is_empty()) {
			return true;
		}
		if (OS::get_singleton()->get_ticks_msec() - start_time > (uint64_t)timeout_msec) {
			r_error = "MCP HTTP request timed out.";
			return false;
		}
		OS::get_singleton()->delay_usec(1000);
	}
	return true;
}

bool AIMCPHTTPClient::_read_streamable_http_response(const Ref<HTTPClient> &p_client, int p_expected_id, ResponseMetadata &r_metadata, Dictionary &r_result, String &r_error) const {
	PackedByteArray response_body;
	const uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	bool received_response = r_metadata.headers_received;
	while (true) {
		if (_is_cancel_requested()) {
			r_error = "Tool execution cancelled.";
			return false;
		}

		p_client->poll();
		HTTPClient::Status status = p_client->get_status();
		_capture_response_metadata(p_client, status, r_metadata);
		if (status == HTTPClient::STATUS_BODY) {
			received_response = true;
			PackedByteArray chunk = p_client->read_response_body_chunk();
			if (!chunk.is_empty()) {
				response_body.append_array(chunk);
				const String response_text = String::utf8(reinterpret_cast<const char *>(response_body.ptr()), response_body.size());
				if (response_text.begins_with("data:") || response_text.contains("\ndata:")) {
					Vector<String> events;
					_consume_sse_data_events(response_body, events);
					for (int i = 0; i < events.size(); i++) {
						Dictionary candidate;
						const AIMCPResponseParseStatus parse_status = AIMCPProtocol::parse_response_line(events[i], p_expected_id, candidate, r_error);
						if (parse_status == AI_MCP_RESPONSE_MATCHED) {
							r_result = candidate;
							return true;
						}
						if (parse_status == AI_MCP_RESPONSE_FAILED) {
							return false;
						}
					}
				}
			}
		} else if (status == HTTPClient::STATUS_CONNECTED && p_client->has_response() && !received_response) {
			received_response = true;
		} else if (status == HTTPClient::STATUS_CONNECTED && received_response) {
			break;
		} else if (status == HTTPClient::STATUS_DISCONNECTED) {
			break;
		} else if (status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
			r_error = "MCP HTTP TLS handshake failed.";
			return false;
		} else if (status >= HTTPClient::STATUS_CONNECTION_ERROR) {
			r_error = vformat("MCP HTTP connection error: status=%d", (int)status);
			return false;
		}

		if (OS::get_singleton()->get_ticks_msec() - start_time > (uint64_t)timeout_msec) {
			r_error = "MCP HTTP request timed out.";
			return false;
		}
		OS::get_singleton()->delay_usec(1000);
	}

	const String response_text = response_body.is_empty() ? String() : String::utf8(reinterpret_cast<const char *>(response_body.ptr()), response_body.size());
	return _parse_json_or_sse_response(response_text, p_expected_id, r_result, r_error);
}

bool AIMCPHTTPClient::_send_streamable_http_message(const String &p_request_json, int p_request_id, Dictionary &r_result, String &r_error) {
	Endpoint endpoint;
	if (!_parse_endpoint(server.url, endpoint, r_error)) {
		return false;
	}

	Ref<HTTPClient> client;
	if (!_connect(endpoint, client, r_error)) {
		return false;
	}

	Vector<String> headers = _build_headers(server.headers, session_id, true);
	PackedByteArray body = p_request_json.to_utf8_buffer();
	Error err = client->request(HTTPClient::METHOD_POST, endpoint.path, headers, body.ptr(), body.size());
	if (err != OK) {
		r_error = "Failed to send MCP HTTP request.";
		return false;
	}

	ResponseMetadata metadata;
	if (!_wait_for_response_headers(client, metadata, r_error)) {
		return false;
	}
	if (!metadata.session_id.is_empty()) {
		session_id = metadata.session_id;
	}

	const int response_code = metadata.response_code;
	if (p_request_id <= 0 && (response_code == HTTPClient::RESPONSE_ACCEPTED || response_code == HTTPClient::RESPONSE_NO_CONTENT)) {
		r_result.clear();
		return true;
	}
	if (response_code < 200 || response_code >= 300) {
		PackedByteArray response_body;
		(void)_read_response_body(client, response_body, r_error, false, &metadata);
		const String response_text = response_body.is_empty() ? String() : String::utf8(reinterpret_cast<const char *>(response_body.ptr()), response_body.size());
		r_error = vformat("MCP HTTP server returned HTTP %d: %s", response_code, response_text.substr(0, 512));
		return false;
	}
	if (p_request_id <= 0) {
		r_result.clear();
		return true;
	}
	return _read_streamable_http_response(client, p_request_id, metadata, r_result, r_error);
}

bool AIMCPHTTPClient::_extract_sse_endpoint_path(const String &p_event_data, String &r_endpoint_path) const {
	Ref<JSON> parser;
	parser.instantiate();
	if (parser->parse(p_event_data) == OK && parser->get_data().get_type() == Variant::DICTIONARY) {
		Dictionary event = parser->get_data();
		r_endpoint_path = String(event.get("uri", event.get("url", event.get("endpoint", String())))).strip_edges();
	} else if (parser->get_data().get_type() == Variant::STRING) {
		r_endpoint_path = String(parser->get_data()).strip_edges();
	} else {
		r_endpoint_path = p_event_data.strip_edges();
	}
	if (!r_endpoint_path.begins_with("/") && !r_endpoint_path.begins_with("?") && !r_endpoint_path.begins_with("http://") && !r_endpoint_path.begins_with("https://")) {
		r_endpoint_path.clear();
	}
	return !r_endpoint_path.is_empty();
}

bool AIMCPHTTPClient::_read_sse_endpoint_event(const Ref<HTTPClient> &p_stream_client, ResponseMetadata &r_metadata, String &r_endpoint_path, String &r_error) const {
	PackedByteArray event_buffer;
	const uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	while (true) {
		if (_is_cancel_requested()) {
			r_error = "Tool execution cancelled.";
			return false;
		}

		p_stream_client->poll();
		HTTPClient::Status status = p_stream_client->get_status();
		_capture_response_metadata(p_stream_client, status, r_metadata);
		if (status == HTTPClient::STATUS_BODY) {
			PackedByteArray chunk = p_stream_client->read_response_body_chunk();
			if (!chunk.is_empty()) {
				event_buffer.append_array(chunk);
				Vector<String> events;
				_consume_sse_data_events(event_buffer, events);
				for (int i = 0; i < events.size(); i++) {
					if (_extract_sse_endpoint_path(events[i], r_endpoint_path)) {
						return true;
					}
				}
			}
		} else if (status == HTTPClient::STATUS_DISCONNECTED) {
			r_error = "MCP SSE stream closed before providing a message endpoint.";
			return false;
		} else if (status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
			r_error = "MCP SSE TLS handshake failed.";
			return false;
		} else if (status >= HTTPClient::STATUS_CONNECTION_ERROR) {
			r_error = vformat("MCP SSE stream error: status=%d", (int)status);
			return false;
		}

		if (r_metadata.headers_received && (r_metadata.response_code < 200 || r_metadata.response_code >= 300)) {
			const String response_text = event_buffer.is_empty() ? String() : String::utf8(reinterpret_cast<const char *>(event_buffer.ptr()), event_buffer.size());
			r_error = vformat("MCP SSE stream endpoint returned HTTP %d: %s", r_metadata.response_code, response_text.substr(0, 512));
			return false;
		}
		if (OS::get_singleton()->get_ticks_msec() - start_time > (uint64_t)timeout_msec) {
			r_error = "MCP SSE stream timed out while waiting for a message endpoint.";
			return false;
		}
		OS::get_singleton()->delay_usec(1000);
	}
}

bool AIMCPHTTPClient::_open_sse_channel(Ref<HTTPClient> &r_stream_client, Endpoint &r_post_endpoint, String &r_error) const {
	Endpoint stream_endpoint;
	if (!_parse_endpoint(server.url, stream_endpoint, r_error)) {
		return false;
	}
	if (!_connect(stream_endpoint, r_stream_client, r_error)) {
		return false;
	}

	Vector<String> headers = _build_headers(server.headers, String(), true);
	Error err = r_stream_client->request(HTTPClient::METHOD_GET, stream_endpoint.path, headers, nullptr, 0);
	if (err != OK) {
		r_error = "Failed to open MCP SSE stream.";
		return false;
	}

	ResponseMetadata metadata;
	String endpoint_path;
	if (!_read_sse_endpoint_event(r_stream_client, metadata, endpoint_path, r_error)) {
		return false;
	}

	if (endpoint_path.is_empty()) {
		r_error = "MCP SSE server did not provide a message endpoint.";
		return false;
	}

	if (endpoint_path.begins_with("http://") || endpoint_path.begins_with("https://")) {
		if (!_parse_endpoint(endpoint_path, r_post_endpoint, r_error)) {
			return false;
		}
	} else {
		r_post_endpoint = stream_endpoint;
		r_post_endpoint.path = _join_paths(stream_endpoint.path, endpoint_path);
	}
	return true;
}

bool AIMCPHTTPClient::_send_legacy_sse_post(const Endpoint &p_post_endpoint, const String &p_request_json, String &r_error) const {
	Ref<HTTPClient> client;
	if (!_connect(p_post_endpoint, client, r_error)) {
		return false;
	}

	Vector<String> headers = _build_headers(server.headers, String(), false);
	PackedByteArray body = p_request_json.to_utf8_buffer();
	Error err = client->request(HTTPClient::METHOD_POST, p_post_endpoint.path, headers, body.ptr(), body.size());
	if (err != OK) {
		r_error = "Failed to send MCP SSE message.";
		return false;
	}

	PackedByteArray response_body;
	if (!_read_response_body(client, response_body, r_error)) {
		return false;
	}
	const int response_code = client->get_response_code();
	if (response_code < 200 || response_code >= 300) {
		const String response_text = response_body.is_empty() ? String() : String::utf8(reinterpret_cast<const char *>(response_body.ptr()), response_body.size());
		r_error = vformat("MCP SSE message endpoint returned HTTP %d: %s", response_code, response_text.substr(0, 512));
		return false;
	}
	return true;
}

bool AIMCPHTTPClient::_read_legacy_sse_response(const Ref<HTTPClient> &p_stream_client, int p_expected_id, Dictionary &r_result, String &r_error) const {
	PackedByteArray event_buffer;
	const uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	while (true) {
		if (_is_cancel_requested()) {
			r_error = "Tool execution cancelled.";
			return false;
		}

		p_stream_client->poll();
		HTTPClient::Status status = p_stream_client->get_status();
		if (status == HTTPClient::STATUS_BODY) {
			PackedByteArray chunk = p_stream_client->read_response_body_chunk();
			if (!chunk.is_empty()) {
				event_buffer.append_array(chunk);
				Vector<String> events;
				_consume_sse_data_events(event_buffer, events);
				for (int i = 0; i < events.size(); i++) {
					Dictionary candidate;
					const AIMCPResponseParseStatus parse_status = AIMCPProtocol::parse_response_line(events[i], p_expected_id, candidate, r_error);
					if (parse_status == AI_MCP_RESPONSE_MATCHED) {
						r_result = candidate;
						return true;
					}
					if (parse_status == AI_MCP_RESPONSE_FAILED) {
						return false;
					}
				}
			}
		} else if (status == HTTPClient::STATUS_DISCONNECTED) {
			r_error = "MCP SSE stream closed before returning a response.";
			return false;
		} else if (status >= HTTPClient::STATUS_CONNECTION_ERROR) {
			r_error = vformat("MCP SSE stream error: status=%d", (int)status);
			return false;
		}

		if (OS::get_singleton()->get_ticks_msec() - start_time > (uint64_t)timeout_msec) {
			r_error = "MCP SSE request timed out.";
			return false;
		}
		OS::get_singleton()->delay_usec(1000);
	}
}

bool AIMCPHTTPClient::_send_legacy_sse_message(const String &p_request_json, int p_request_id, Dictionary &r_result, String &r_error) {
	Ref<HTTPClient> stream_client;
	Endpoint post_endpoint;
	if (!_open_sse_channel(stream_client, post_endpoint, r_error)) {
		return false;
	}
	if (!_send_legacy_sse_post(post_endpoint, p_request_json, r_error)) {
		return false;
	}
	if (p_request_id <= 0) {
		r_result.clear();
		return true;
	}
	return _read_legacy_sse_response(stream_client, p_request_id, r_result, r_error);
}

bool AIMCPHTTPClient::_initialize_legacy_sse_channel(const Ref<HTTPClient> &p_stream_client, const Endpoint &p_post_endpoint, String &r_error) {
	Dictionary initialize_result;
	const int initialize_id = next_request_id++;
	if (!_send_legacy_sse_post(p_post_endpoint, AIMCPProtocol::make_initialize_request(initialize_id), r_error)) {
		return false;
	}
	if (!_read_legacy_sse_response(p_stream_client, initialize_id, initialize_result, r_error)) {
		return false;
	}
	Dictionary notification_result;
	return _send_legacy_sse_post(p_post_endpoint, AIMCPProtocol::make_initialized_notification(), r_error);
}

bool AIMCPHTTPClient::_send_message(const String &p_request_json, int p_request_id, Dictionary &r_result, String &r_error) {
	if (server.transport == "sse") {
		return _send_legacy_sse_message(p_request_json, p_request_id, r_result, r_error);
	}
	return _send_streamable_http_message(p_request_json, p_request_id, r_result, r_error);
}

bool AIMCPHTTPClient::_initialize_session(String &r_error) {
	if (server.transport == "sse") {
		Ref<HTTPClient> stream_client;
		Endpoint post_endpoint;
		if (!_open_sse_channel(stream_client, post_endpoint, r_error)) {
			return false;
		}
		return _initialize_legacy_sse_channel(stream_client, post_endpoint, r_error);
	}

	Dictionary initialize_result;
	const int request_id = next_request_id++;
	if (!_send_message(AIMCPProtocol::make_initialize_request(request_id), request_id, initialize_result, r_error)) {
		return false;
	}
	Dictionary notification_result;
	return _send_message(AIMCPProtocol::make_initialized_notification(), 0, notification_result, r_error);
}

bool AIMCPHTTPClient::_list_tools_legacy_sse(Vector<AIMCPToolDescriptor> &r_tools, String &r_error) {
	Ref<HTTPClient> stream_client;
	Endpoint post_endpoint;
	if (!_open_sse_channel(stream_client, post_endpoint, r_error)) {
		return false;
	}
	if (!_initialize_legacy_sse_channel(stream_client, post_endpoint, r_error)) {
		return false;
	}

	Dictionary result;
	const int request_id = next_request_id++;
	if (!_send_legacy_sse_post(post_endpoint, AIMCPProtocol::make_tools_list_request(request_id), r_error)) {
		return false;
	}
	if (!_read_legacy_sse_response(stream_client, request_id, result, r_error)) {
		return false;
	}
	return AIMCPProtocol::parse_tools_list_result(result, server.id, server.display_name, r_tools, r_error);
}

AIMCPToolCallResult AIMCPHTTPClient::_call_tool_legacy_sse(const String &p_tool_name, const Dictionary &p_arguments) {
	AIMCPToolCallResult result;
	String error;
	Ref<HTTPClient> stream_client;
	Endpoint post_endpoint;
	if (!_open_sse_channel(stream_client, post_endpoint, error)) {
		result.error = error;
		return result;
	}
	if (!_initialize_legacy_sse_channel(stream_client, post_endpoint, error)) {
		result.error = error;
		return result;
	}

	Dictionary response;
	const int request_id = next_request_id++;
	if (!_send_legacy_sse_post(post_endpoint, AIMCPProtocol::make_tools_call_request(request_id, p_tool_name, p_arguments), error)) {
		result.error = error;
		return result;
	}
	if (!_read_legacy_sse_response(stream_client, request_id, response, error)) {
		result.error = error;
		return result;
	}
	result = AIMCPProtocol::parse_tool_call_result(response);
	result.metadata["mcp_server_id"] = server.id;
	result.metadata["mcp_server_name"] = server.display_name;
	result.metadata["mcp_tool_name"] = p_tool_name;
	result.metadata["mcp_transport"] = server.transport;
	return result;
}

Vector<String> AIMCPHTTPClient::extract_sse_data_events_for_test(const String &p_text) {
	Vector<String> events;
	_extract_sse_data_events(p_text, events);
	return events;
}

Vector<String> AIMCPHTTPClient::consume_sse_data_events_for_test(PackedByteArray &r_buffer) {
	Vector<String> events;
	_consume_sse_data_events(r_buffer, events);
	return events;
}

bool AIMCPHTTPClient::initialize(String &r_error) {
	session_id.clear();
	return _initialize_session(r_error);
}

bool AIMCPHTTPClient::list_tools(Vector<AIMCPToolDescriptor> &r_tools, String &r_error) {
	if (server.transport == "sse") {
		return _list_tools_legacy_sse(r_tools, r_error);
	}

	session_id.clear();
	if (!_initialize_session(r_error)) {
		return false;
	}

	Dictionary result;
	const int request_id = next_request_id++;
	if (!_send_message(AIMCPProtocol::make_tools_list_request(request_id), request_id, result, r_error)) {
		return false;
	}
	return AIMCPProtocol::parse_tools_list_result(result, server.id, server.display_name, r_tools, r_error);
}

AIMCPToolCallResult AIMCPHTTPClient::call_tool(const String &p_tool_name, const Dictionary &p_arguments) {
	if (server.transport == "sse") {
		return _call_tool_legacy_sse(p_tool_name, p_arguments);
	}

	AIMCPToolCallResult result;
	String error;
	session_id.clear();
	if (!_initialize_session(error)) {
		result.error = error;
		return result;
	}

	Dictionary response;
	const int request_id = next_request_id++;
	if (!_send_message(AIMCPProtocol::make_tools_call_request(request_id, p_tool_name, p_arguments), request_id, response, error)) {
		result.error = error;
		return result;
	}
	result = AIMCPProtocol::parse_tool_call_result(response);
	result.metadata["mcp_server_id"] = server.id;
	result.metadata["mcp_server_name"] = server.display_name;
	result.metadata["mcp_tool_name"] = p_tool_name;
	result.metadata["mcp_transport"] = server.transport;
	return result;
}
