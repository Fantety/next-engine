/**************************************************************************/
/*  ai_http_client.cpp                                                    */
/**************************************************************************/

#include "ai_http_client.h"

#include "core/os/os.h"
#include "core/variant/variant.h"

bool AIHTTPResponse::is_success_status_code() const {
	return response_code >= 200 && response_code < 300;
}

String AIHTTPResponse::get_body_as_text() const {
	if (body.is_empty()) {
		return String();
	}
	return String::utf8(reinterpret_cast<const char *>(body.ptr()), body.size());
}

String AIHTTPResponse::get_header_value(const String &p_name) const {
	const String expected = p_name.strip_edges().to_lower();
	for (int i = 0; i < headers.size(); i++) {
		const Vector<String> parts = headers[i].split(":", true, 1);
		if (parts.size() == 2 && parts[0].strip_edges().to_lower() == expected) {
			return parts[1].strip_edges();
		}
	}
	return String();
}

void AIHTTPClient::_bind_methods() {
}

bool AIHTTPClient::_is_cancel_requested(const Ref<AICancelToken> &p_cancel_token) {
	return p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested();
}

String AIHTTPClient::_get_cancel_message(const Ref<AICancelToken> &p_cancel_token) {
	return p_cancel_token.is_valid() ? p_cancel_token->get_cancel_message("HTTP request cancelled.") : String("HTTP request cancelled.");
}

bool AIHTTPClient::_capture_response_metadata(const Ref<HTTPClient> &p_client, HTTPClient::Status p_status, AIHTTPResponse &r_response) {
	if (r_response.headers_received) {
		return true;
	}
	if (p_status != HTTPClient::STATUS_BODY && !(p_status == HTTPClient::STATUS_CONNECTED && (p_client->has_response() || p_client->get_response_code() != 0))) {
		return false;
	}

	r_response.response_code = p_client->get_response_code();
	r_response.headers.clear();

	List<String> response_headers;
	if (p_client->get_response_headers(&response_headers) == OK) {
		for (const List<String>::Element *E = response_headers.front(); E; E = E->next()) {
			r_response.headers.push_back(E->get());
		}
	}

	r_response.headers_received = true;
	return true;
}

bool AIHTTPClient::_append_body_limited(PackedByteArray &r_body, const PackedByteArray &p_chunk, int p_limit, String &r_error) {
	if (p_chunk.is_empty()) {
		return true;
	}
	if (p_limit >= 0 && r_body.size() + p_chunk.size() > p_limit) {
		r_error = vformat("HTTP response exceeded maximum body size (%d bytes).", p_limit);
		return false;
	}
	r_body.append_array(p_chunk);
	return true;
}

bool AIHTTPClient::_call_event_callback(const Callable &p_callback, const AISSEEvent &p_event, bool &r_stop_requested, String &r_error) {
	r_stop_requested = false;
	if (!p_callback.is_valid()) {
		return true;
	}

	Dictionary event_dict = p_event.to_dictionary();
	Variant event_variant = event_dict;
	const Variant *argptrs[1] = { &event_variant };
	Variant ret;
	Callable::CallError ce;
	p_callback.callp(argptrs, 1, ret, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		r_error = "Failed to dispatch HTTP SSE event: " + Variant::get_callable_error_text(p_callback, argptrs, 1, ce) + ".";
		return false;
	}

	if (ret.get_type() == Variant::BOOL && bool(ret)) {
		r_stop_requested = true;
	}
	return true;
}

String AIHTTPClient::_make_http_error_preview(const AIHTTPResponse &p_response) {
	String preview = p_response.get_body_as_text().strip_edges();
	if (preview.length() > 512) {
		preview = preview.substr(0, 512) + "...";
	}
	return preview;
}

String AIHTTPClient::normalize_connection_host(const String &p_host) {
	const String host = p_host.strip_edges();
	if (host == "0.0.0.0") {
		return "127.0.0.1";
	}
	if (host == "::" || host == "[::]") {
		return "::1";
	}
	return host;
}

bool AIHTTPClient::parse_endpoint(const String &p_url, AIHTTPEndpoint &r_endpoint, String &r_error) {
	r_endpoint = AIHTTPEndpoint();

	String scheme;
	String host;
	String path;
	String fragment;
	int port = 0;
	const Error err = p_url.strip_edges().parse_url(scheme, host, port, path, fragment);
	if (err != OK || (scheme != "https://" && scheme != "http://") || host.is_empty()) {
		r_error = "HTTP URL is invalid.";
		return false;
	}

	r_endpoint.scheme = scheme;
	r_endpoint.host = host;
	r_endpoint.path = path.is_empty() ? String("/") : path;
	r_endpoint.use_tls = scheme == "https://";
	r_endpoint.port = port == 0 ? (r_endpoint.use_tls ? 443 : 80) : port;
	return true;
}

String AIHTTPClient::join_paths(const String &p_base_path, const String &p_endpoint_path) {
	if (p_endpoint_path.is_empty()) {
		return p_base_path.is_empty() ? String("/") : p_base_path;
	}
	if (p_endpoint_path.begins_with("http://") || p_endpoint_path.begins_with("https://")) {
		AIHTTPEndpoint endpoint;
		String error;
		if (parse_endpoint(p_endpoint_path, endpoint, error)) {
			return endpoint.path;
		}
		return p_endpoint_path;
	}
	if (p_endpoint_path.begins_with("/")) {
		return p_endpoint_path;
	}
	if (p_endpoint_path.begins_with("?")) {
		const String base_path = p_base_path.is_empty() ? String("/") : p_base_path;
		return base_path + p_endpoint_path;
	}

	String base_path = p_base_path.is_empty() ? String("/") : p_base_path;
	const int slash_index = base_path.rfind("/");
	if (slash_index >= 0) {
		base_path = base_path.substr(0, slash_index + 1);
	}
	return (base_path + p_endpoint_path).simplify_path();
}

bool AIHTTPClient::_connect(const AIHTTPEndpoint &p_endpoint, const AIHTTPRequest &p_request, Ref<HTTPClient> &r_client, String &r_error) const {
	r_client = HTTPClient::create();
	r_client->set_blocking_mode(true);

	const String connection_host = normalize_connection_host(p_endpoint.host);
	const Error err = r_client->connect_to_host(connection_host, p_endpoint.port, p_endpoint.use_tls ? Ref<TLSOptions>(TLSOptions::client()) : Ref<TLSOptions>());
	if (err != OK) {
		r_error = vformat("%s failed to connect: %s:%d", p_request.label.is_empty() ? String("HTTP request") : p_request.label, connection_host, p_endpoint.port);
		return false;
	}

	const uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	const uint64_t timeout_msec = (uint64_t)MAX(1, p_request.timeout_msec);
	while (r_client->get_status() == HTTPClient::STATUS_CONNECTING || r_client->get_status() == HTTPClient::STATUS_RESOLVING) {
		if (_is_cancel_requested(p_request.cancel_token)) {
			r_error = _get_cancel_message(p_request.cancel_token);
			return false;
		}

		r_client->poll();
		if (OS::get_singleton()->get_ticks_msec() - start_time > timeout_msec) {
			r_error = vformat("%s connection timed out: %s:%d", p_request.label.is_empty() ? String("HTTP request") : p_request.label, connection_host, p_endpoint.port);
			return false;
		}
		OS::get_singleton()->delay_usec(MAX(0, p_request.poll_interval_usec));
	}

	if (r_client->get_status() != HTTPClient::STATUS_CONNECTED) {
		r_error = vformat("%s connection failed: status=%d host=%s port=%d", p_request.label.is_empty() ? String("HTTP request") : p_request.label, (int)r_client->get_status(), connection_host, p_endpoint.port);
		return false;
	}
	return true;
}

bool AIHTTPClient::_send_request(const Ref<HTTPClient> &p_client, const AIHTTPEndpoint &p_endpoint, const AIHTTPRequest &p_request, String &r_error) const {
	const uint8_t *body_ptr = p_request.body.is_empty() ? nullptr : p_request.body.ptr();
	const Error err = p_client->request(p_request.method, p_endpoint.path, p_request.headers, body_ptr, p_request.body.size());
	if (err != OK) {
		r_error = vformat("%s failed to send request (%d): %s", p_request.label.is_empty() ? String("HTTP request") : p_request.label, err, p_endpoint.path);
		return false;
	}
	return true;
}

bool AIHTTPClient::_read_response(const Ref<HTTPClient> &p_client, const AIHTTPRequest &p_request, AIHTTPResponse &r_response, String &r_error) const {
	const uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	const uint64_t timeout_msec = (uint64_t)MAX(1, p_request.timeout_msec);
	bool received_body = false;

	while (true) {
		if (_is_cancel_requested(p_request.cancel_token)) {
			r_error = _get_cancel_message(p_request.cancel_token);
			return false;
		}

		p_client->poll();
		const HTTPClient::Status status = p_client->get_status();
		_capture_response_metadata(p_client, status, r_response);

		if (status == HTTPClient::STATUS_BODY) {
			received_body = true;
			const PackedByteArray chunk = p_client->read_response_body_chunk();
			if (!_append_body_limited(r_response.body, chunk, p_request.max_response_body_bytes, r_error)) {
				return false;
			}
		} else if (status == HTTPClient::STATUS_CONNECTED && p_client->has_response() && !received_body) {
			break;
		} else if (status == HTTPClient::STATUS_CONNECTED && received_body) {
			break;
		} else if (status == HTTPClient::STATUS_DISCONNECTED) {
			break;
		} else if (status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
			r_error = p_request.label.is_empty() ? String("HTTP TLS handshake failed.") : p_request.label + " TLS handshake failed.";
			return false;
		} else if (status >= HTTPClient::STATUS_CONNECTION_ERROR) {
			r_error = vformat("%s connection error: status=%d", p_request.label.is_empty() ? String("HTTP request") : p_request.label, (int)status);
			return false;
		}

		if (OS::get_singleton()->get_ticks_msec() - start_time > timeout_msec) {
			r_error = vformat("%s timed out while reading response.", p_request.label.is_empty() ? String("HTTP request") : p_request.label);
			return false;
		}
		OS::get_singleton()->delay_usec(MAX(0, p_request.poll_interval_usec));
	}

	if (p_request.fail_on_http_error && !r_response.is_success_status_code()) {
		r_error = vformat("%s returned HTTP %d: %s", p_request.label.is_empty() ? String("HTTP request") : p_request.label, r_response.response_code, _make_http_error_preview(r_response));
		return false;
	}
	return true;
}

bool AIHTTPClient::_read_sse_response(const Ref<HTTPClient> &p_client, const AIHTTPRequest &p_request, const Callable &p_event_callback, AIHTTPResponse &r_response, String &r_error) const {
	const uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	const uint64_t timeout_msec = (uint64_t)MAX(1, p_request.timeout_msec);
	AISSEParser parser;
	bool received_body = false;
	bool stop_requested = false;

	while (!stop_requested) {
		if (_is_cancel_requested(p_request.cancel_token)) {
			r_error = _get_cancel_message(p_request.cancel_token);
			return false;
		}

		p_client->poll();
		const HTTPClient::Status status = p_client->get_status();
		_capture_response_metadata(p_client, status, r_response);

		if (status == HTTPClient::STATUS_BODY) {
			received_body = true;
			const PackedByteArray chunk = p_client->read_response_body_chunk();
			if (!chunk.is_empty()) {
				if (p_request.store_stream_body) {
					if (!_append_body_limited(r_response.body, chunk, p_request.max_response_body_bytes, r_error)) {
						return false;
					}
				} else if (r_response.body.size() < STREAM_PREVIEW_BYTES) {
					const int remaining = STREAM_PREVIEW_BYTES - r_response.body.size();
					r_response.body.append_array(chunk.size() > remaining ? chunk.slice(0, remaining) : chunk);
				}

				Vector<AISSEEvent> events;
				if (parser.push_chunk(chunk, events)) {
					for (int i = 0; i < events.size(); i++) {
						if (!_call_event_callback(p_event_callback, events[i], stop_requested, r_error)) {
							return false;
						}
						if (stop_requested) {
							break;
						}
					}
				}
			}
		} else if (status == HTTPClient::STATUS_CONNECTED && p_client->has_response() && !received_body) {
			// Headers are available and the stream may still produce body chunks.
		} else if (status == HTTPClient::STATUS_CONNECTED && received_body) {
			break;
		} else if (status == HTTPClient::STATUS_DISCONNECTED) {
			break;
		} else if (status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
			r_error = p_request.label.is_empty() ? String("HTTP SSE TLS handshake failed.") : p_request.label + " TLS handshake failed.";
			return false;
		} else if (status >= HTTPClient::STATUS_CONNECTION_ERROR) {
			r_error = vformat("%s SSE connection error: status=%d", p_request.label.is_empty() ? String("HTTP request") : p_request.label, (int)status);
			return false;
		}

		if (OS::get_singleton()->get_ticks_msec() - start_time > timeout_msec) {
			r_error = vformat("%s timed out while reading SSE stream.", p_request.label.is_empty() ? String("HTTP request") : p_request.label);
			return false;
		}
		OS::get_singleton()->delay_usec(MAX(0, p_request.poll_interval_usec));
	}

	Vector<AISSEEvent> trailing_events;
	if (!stop_requested && parser.finish(trailing_events)) {
		for (int i = 0; i < trailing_events.size(); i++) {
			if (!_call_event_callback(p_event_callback, trailing_events[i], stop_requested, r_error)) {
				return false;
			}
			if (stop_requested) {
				break;
			}
		}
	}

	if (p_request.fail_on_http_error && !r_response.is_success_status_code()) {
		r_error = vformat("%s returned HTTP %d: %s", p_request.label.is_empty() ? String("HTTP request") : p_request.label, r_response.response_code, _make_http_error_preview(r_response));
		return false;
	}
	return true;
}

bool AIHTTPClient::request(const AIHTTPRequest &p_request, AIHTTPResponse &r_response, String &r_error) const {
	r_response = AIHTTPResponse();
	r_error = String();

	AIHTTPEndpoint endpoint;
	if (!parse_endpoint(p_request.url, endpoint, r_error)) {
		return false;
	}

	Ref<HTTPClient> client;
	if (!_connect(endpoint, p_request, client, r_error)) {
		return false;
	}
	if (!_send_request(client, endpoint, p_request, r_error)) {
		return false;
	}
	return _read_response(client, p_request, r_response, r_error);
}

bool AIHTTPClient::request_sse(const AIHTTPRequest &p_request, const Callable &p_event_callback, AIHTTPResponse &r_response, String &r_error) const {
	r_response = AIHTTPResponse();
	r_error = String();

	AIHTTPEndpoint endpoint;
	if (!parse_endpoint(p_request.url, endpoint, r_error)) {
		return false;
	}

	Ref<HTTPClient> client;
	if (!_connect(endpoint, p_request, client, r_error)) {
		return false;
	}
	if (!_send_request(client, endpoint, p_request, r_error)) {
		return false;
	}
	return _read_sse_response(client, p_request, p_event_callback, r_response, r_error);
}
