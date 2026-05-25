/**************************************************************************/
/*  ai_openai_runtime_client.cpp                                          */
/**************************************************************************/

#include "ai_openai_runtime_client.h"

#include "core/io/json.h"
#include "core/io/http_client.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/variant/variant.h"

#include "editor/ai_component/providers/ai_openai_compatible_codec.h"

class AIOpenAIStreamEventHandler : public RefCounted {
	GDSOFTCLASS(AIOpenAIStreamEventHandler, RefCounted);

	AIOpenAICompatibleStreamAccumulator accumulator;
	AIAgentRuntimeResponse response;
	String error;
	Callable partial_response_callback;

protected:
	static void _bind_methods() {}

public:
	void set_partial_response_callback(const Callable &p_callback) {
		partial_response_callback = p_callback;
	}

	void handle_event(const String &p_event) {
		if (!error.is_empty()) {
			return;
		}

		AIOpenAIStreamParseResult parse_result;
		if (!accumulator.apply_event(p_event, parse_result)) {
			error = parse_result.error.is_empty() ? String("Failed to parse provider stream.") : parse_result.error;
			print_line(vformat("[AI Agent][RuntimeClient] Streaming event parse failed: %s", error));
			return;
		}

		response = parse_result.response;
		if (parse_result.has_delta && partial_response_callback.is_valid() && !parse_result.response.content.is_empty()) {
			Dictionary partial_response;
			partial_response["content"] = parse_result.response.content;
			partial_response["metadata"] = parse_result.response.metadata;
			partial_response_callback.call(partial_response);
		}
	}

	AIAgentRuntimeResponse finish(String &r_error) const {
		if (!error.is_empty()) {
			r_error = error;
			return AIAgentRuntimeResponse();
		}

		String final_error;
		AIAgentRuntimeResponse final_response = accumulator.get_response(final_error);
		if (!final_error.is_empty()) {
			r_error = final_error;
			return AIAgentRuntimeResponse();
		}

		r_error = String();
		return final_response;
	}
};

static bool _process_openai_sse_event(const String &p_raw_event, const Callable &p_stream_event_callback, int &r_stream_event_count) {
	Vector<String> lines = p_raw_event.split("\n", false);
	String data;
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (!line.begins_with("data:")) {
			continue;
		}
		String value = line.substr(5).strip_edges();
		if (!data.is_empty()) {
			data += "\n";
		}
		data += value;
	}
	if (data.is_empty()) {
		return false;
	}

	r_stream_event_count++;
	print_line(vformat("[AI Agent][HTTP] Streaming event received. event=%d chars=%d done=%s", r_stream_event_count, data.length(), data == "[DONE]" ? "yes" : "no"));
	if (p_stream_event_callback.is_valid()) {
		p_stream_event_callback.call(data);
	}
	return data == "[DONE]";
}

static int _find_openai_sse_event_separator(const PackedByteArray &p_buffer, int &r_separator_size) {
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

void AIOpenAIRuntimeTransport::_bind_methods() {
}

bool AIOpenAIRuntimeTransport::request_chat_completion(const AIProviderConfig &p_config, const Array &p_messages, const Array &p_tool_schemas, String &r_response_text, String &r_error) {
	(void)p_config;
	(void)p_messages;
	(void)p_tool_schemas;
	(void)r_response_text;
	r_error = "OpenAI runtime transport is not implemented.";
	return false;
}

bool AIOpenAIRuntimeTransport::request_chat_completion_stream(const AIProviderConfig &p_config, const Array &p_messages, const Array &p_tool_schemas, const Callable &p_stream_event_callback, String &r_error) {
	(void)p_config;
	(void)p_messages;
	(void)p_tool_schemas;
	(void)p_stream_event_callback;
	r_error = "OpenAI streaming runtime transport is not implemented.";
	return false;
}

void AIOpenAIHTTPRuntimeTransport::_bind_methods() {
}

bool AIOpenAIHTTPRuntimeTransport::request_chat_completion(const AIProviderConfig &p_config, const Array &p_messages, const Array &p_tool_schemas, String &r_response_text, String &r_error) {
	r_response_text = String();
	r_error = String();

	print_line(vformat("[AI Agent][HTTP] Starting provider request. provider=%s model=%s messages=%d tools=%d timeout=%ds", p_config.provider_name, p_config.model, p_messages.size(), p_tool_schemas.size(), p_config.timeout_seconds));

	if (p_config.api_key.strip_edges().is_empty()) {
		r_error = "API key is not configured.";
		print_line("[AI Agent][HTTP] Failed before request: API key is not configured.");
		return false;
	}
	if (p_config.base_url.strip_edges().is_empty()) {
		r_error = "Provider base URL is not configured.";
		print_line("[AI Agent][HTTP] Failed before request: Provider base URL is not configured.");
		return false;
	}

	String scheme;
	String host;
	String base_path;
	String fragment;
	int port = 0;
	Error err = p_config.base_url.parse_url(scheme, host, port, base_path, fragment);
	if (err != OK || (scheme != "https://" && scheme != "http://") || host.is_empty()) {
		r_error = "Provider base URL is invalid.";
		print_line(vformat("[AI Agent][HTTP] Invalid provider base URL: %s", p_config.base_url));
		return false;
	}

	const bool use_tls = scheme == "https://";
	if (port == 0) {
		port = use_tls ? 443 : 80;
	}
	String request_path = AIOpenAICompatibleCodec::build_request_path(base_path);
	print_line(vformat("[AI Agent][HTTP] Parsed endpoint. scheme=%s host=%s port=%d path=%s tls=%s", scheme, host, port, request_path, use_tls ? "yes" : "no"));

	Ref<HTTPClient> client = HTTPClient::create();
	client->set_blocking_mode(true);
	print_line(vformat("[AI Agent][HTTP] Connecting to %s:%d...", host, port));
	err = client->connect_to_host(host, port, use_tls ? Ref<TLSOptions>(TLSOptions::client()) : Ref<TLSOptions>());
	if (err != OK) {
		r_error = vformat("Failed to connect to provider: %s:%d", host, port);
		print_line(vformat("[AI Agent][HTTP] connect_to_host failed. error=%d host=%s port=%d", err, host, port));
		return false;
	}

	const uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	const uint64_t timeout_msec = MAX(1, p_config.timeout_seconds) * 1000;
	while (client->get_status() == HTTPClient::STATUS_CONNECTING || client->get_status() == HTTPClient::STATUS_RESOLVING) {
		client->poll();
		if (OS::get_singleton()->get_ticks_msec() - start_time > timeout_msec) {
			r_error = vformat("Provider connection timed out: %s:%d", host, port);
			const int64_t elapsed_ms = (int64_t)(OS::get_singleton()->get_ticks_msec() - start_time);
			print_line(vformat("[AI Agent][HTTP] Connection timed out. status=%d elapsed_ms=%d", (int)client->get_status(), elapsed_ms));
			return false;
		}
		OS::get_singleton()->delay_usec(1000);
	}

	if (client->get_status() != HTTPClient::STATUS_CONNECTED) {
		r_error = vformat("Provider connection failed: status=%d host=%s port=%d", (int)client->get_status(), host, port);
		print_line(r_error);
		return false;
	}
	print_line(vformat("[AI Agent][HTTP] Connected to provider. elapsed_ms=%d", (int64_t)(OS::get_singleton()->get_ticks_msec() - start_time)));

	Vector<String> headers;
	headers.push_back("Authorization: Bearer " + p_config.api_key);
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: application/json");

	PackedByteArray body = AIOpenAICompatibleCodec::build_body(p_messages, p_config.model, p_tool_schemas, false, p_config.max_output_tokens);
	print_line(vformat("[AI Agent][HTTP] Sending POST %s body_bytes=%d", request_path, (int)body.size()));
	err = client->request(HTTPClient::METHOD_POST, request_path, headers, body.ptr(), body.size());
	if (err != OK) {
		r_error = vformat("Failed to send provider request (%d): %s", err, request_path);
		print_line(vformat("[AI Agent][HTTP] Failed to send request. error=%d path=%s", err, request_path));
		return false;
	}
	print_line("[AI Agent][HTTP] Request sent. Waiting for response...");

	PackedByteArray response_body;
	bool received_response = false;
	int last_status = -1;
	while (true) {
		client->poll();
		HTTPClient::Status status = client->get_status();
		if ((int)status != last_status) {
			print_line(vformat("[AI Agent][HTTP] Client status changed: %d", (int)status));
			last_status = (int)status;
		}
		if (status == HTTPClient::STATUS_BODY) {
			received_response = true;
			PackedByteArray chunk = client->read_response_body_chunk();
			if (!chunk.is_empty()) {
				response_body.append_array(chunk);
				print_line(vformat("[AI Agent][HTTP] Read response chunk. chunk_bytes=%d total_bytes=%d", (int)chunk.size(), (int)response_body.size()));
			}
		} else if (status == HTTPClient::STATUS_CONNECTED && client->has_response() && !received_response) {
			received_response = true;
			print_line("[AI Agent][HTTP] Response headers received with no response body.");
			break;
		} else if (status == HTTPClient::STATUS_CONNECTED && received_response) {
			print_line("[AI Agent][HTTP] Response body complete; connection kept alive.");
			break;
		} else if (status == HTTPClient::STATUS_DISCONNECTED) {
			print_line("[AI Agent][HTTP] Provider disconnected; response read loop ended.");
			break;
		} else if (status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
			r_error = vformat("Provider TLS handshake failed: %s:%d%s", host, port, request_path);
			print_line(vformat("[AI Agent][HTTP] TLS handshake failed. host=%s port=%d path=%s", host, port, request_path));
			return false;
		} else if (status >= HTTPClient::STATUS_CONNECTION_ERROR) {
			r_error = vformat("Provider connection error: status=%d host=%s path=%s", (int)status, host, request_path);
			print_line(vformat("[AI Agent][HTTP] Connection error while waiting response. status=%d host=%s path=%s received_bytes=%d", (int)status, host, request_path, (int)response_body.size()));
			return false;
		}

		if (OS::get_singleton()->get_ticks_msec() - start_time > timeout_msec) {
			const int64_t elapsed_ms = (int64_t)(OS::get_singleton()->get_ticks_msec() - start_time);
			r_error = vformat("Provider request timed out: host=%s path=%s elapsed_ms=%d received_bytes=%d", host, request_path, elapsed_ms, (int)response_body.size());
			print_line(vformat("[AI Agent][HTTP] Request timed out. elapsed_ms=%d received_bytes=%d last_status=%d", elapsed_ms, (int)response_body.size(), last_status));
			return false;
		}
		OS::get_singleton()->delay_usec(1000);
	}

	if (response_body.is_empty()) {
		r_response_text = String();
	} else {
		r_response_text = String::utf8(reinterpret_cast<const char *>(response_body.ptr()), response_body.size());
	}
	const int response_code = client->get_response_code();
	print_line(vformat("[AI Agent][HTTP] Provider response complete. http_code=%d body_bytes=%d elapsed_ms=%d", response_code, (int)response_body.size(), (int64_t)(OS::get_singleton()->get_ticks_msec() - start_time)));
	if (response_code < 200 || response_code >= 300) {
		String response_preview = r_response_text.strip_edges();
		if (response_preview.length() > 512) {
			response_preview = response_preview.substr(0, 512) + "...";
		}
		r_error = vformat("Provider returned HTTP %d from %s%s: %s", response_code, host, request_path, response_preview);
		print_line(vformat("[AI Agent][HTTP] Provider returned non-success HTTP code. code=%d preview=%s", response_code, response_preview));
		return false;
	}
	print_line("[AI Agent][HTTP] Provider HTTP request succeeded.");
	return true;
}

bool AIOpenAIHTTPRuntimeTransport::request_chat_completion_stream(const AIProviderConfig &p_config, const Array &p_messages, const Array &p_tool_schemas, const Callable &p_stream_event_callback, String &r_error) {
	r_error = String();

	print_line(vformat("[AI Agent][HTTP] Starting streaming provider request. provider=%s model=%s messages=%d tools=%d timeout=%ds", p_config.provider_name, p_config.model, p_messages.size(), p_tool_schemas.size(), p_config.timeout_seconds));

	if (p_config.api_key.strip_edges().is_empty()) {
		r_error = "API key is not configured.";
		print_line("[AI Agent][HTTP] Failed before streaming request: API key is not configured.");
		return false;
	}
	if (p_config.base_url.strip_edges().is_empty()) {
		r_error = "Provider base URL is not configured.";
		print_line("[AI Agent][HTTP] Failed before streaming request: Provider base URL is not configured.");
		return false;
	}

	String scheme;
	String host;
	String base_path;
	String fragment;
	int port = 0;
	Error err = p_config.base_url.parse_url(scheme, host, port, base_path, fragment);
	if (err != OK || (scheme != "https://" && scheme != "http://") || host.is_empty()) {
		r_error = "Provider base URL is invalid.";
		print_line(vformat("[AI Agent][HTTP] Invalid provider base URL: %s", p_config.base_url));
		return false;
	}

	const bool use_tls = scheme == "https://";
	if (port == 0) {
		port = use_tls ? 443 : 80;
	}
	String request_path = AIOpenAICompatibleCodec::build_request_path(base_path);
	print_line(vformat("[AI Agent][HTTP] Parsed streaming endpoint. scheme=%s host=%s port=%d path=%s tls=%s", scheme, host, port, request_path, use_tls ? "yes" : "no"));

	Ref<HTTPClient> client = HTTPClient::create();
	client->set_blocking_mode(true);
	print_line(vformat("[AI Agent][HTTP] Connecting to %s:%d for stream...", host, port));
	err = client->connect_to_host(host, port, use_tls ? Ref<TLSOptions>(TLSOptions::client()) : Ref<TLSOptions>());
	if (err != OK) {
		r_error = vformat("Failed to connect to provider: %s:%d", host, port);
		print_line(vformat("[AI Agent][HTTP] stream connect_to_host failed. error=%d host=%s port=%d", err, host, port));
		return false;
	}

	const uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	const uint64_t timeout_msec = MAX(1, p_config.timeout_seconds) * 1000;
	while (client->get_status() == HTTPClient::STATUS_CONNECTING || client->get_status() == HTTPClient::STATUS_RESOLVING) {
		client->poll();
		if (OS::get_singleton()->get_ticks_msec() - start_time > timeout_msec) {
			r_error = vformat("Provider connection timed out: %s:%d", host, port);
			const int64_t elapsed_ms = (int64_t)(OS::get_singleton()->get_ticks_msec() - start_time);
			print_line(vformat("[AI Agent][HTTP] Streaming connection timed out. status=%d elapsed_ms=%d", (int)client->get_status(), elapsed_ms));
			return false;
		}
		OS::get_singleton()->delay_usec(1000);
	}

	if (client->get_status() != HTTPClient::STATUS_CONNECTED) {
		r_error = vformat("Provider connection failed: status=%d host=%s port=%d", (int)client->get_status(), host, port);
		print_line(r_error);
		return false;
	}
	print_line(vformat("[AI Agent][HTTP] Connected to streaming provider. elapsed_ms=%d", (int64_t)(OS::get_singleton()->get_ticks_msec() - start_time)));

	Vector<String> headers;
	headers.push_back("Authorization: Bearer " + p_config.api_key);
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: text/event-stream");
	headers.push_back("Cache-Control: no-cache");

	PackedByteArray body = AIOpenAICompatibleCodec::build_body(p_messages, p_config.model, p_tool_schemas, true, p_config.max_output_tokens);
	print_line(vformat("[AI Agent][HTTP] Sending streaming POST %s body_bytes=%d", request_path, (int)body.size()));
	err = client->request(HTTPClient::METHOD_POST, request_path, headers, body.ptr(), body.size());
	if (err != OK) {
		r_error = vformat("Failed to send provider request (%d): %s", err, request_path);
		print_line(vformat("[AI Agent][HTTP] Failed to send streaming request. error=%d path=%s", err, request_path));
		return false;
	}
	print_line("[AI Agent][HTTP] Streaming request sent. Waiting for events...");

	PackedByteArray event_buffer;
	String raw_response_preview;
	int last_status = -1;
	bool saw_done = false;
	bool received_response = false;
	bool received_body = false;
	int stream_event_count = 0;
	while (true) {
		client->poll();
		HTTPClient::Status status = client->get_status();
		if ((int)status != last_status) {
			print_line(vformat("[AI Agent][HTTP] Streaming client status changed: %d", (int)status));
			last_status = (int)status;
		}

		if (status == HTTPClient::STATUS_BODY) {
			received_response = true;
			received_body = true;
			PackedByteArray chunk = client->read_response_body_chunk();
			if (!chunk.is_empty()) {
				String chunk_text = String::utf8(reinterpret_cast<const char *>(chunk.ptr()), chunk.size());
				if (raw_response_preview.length() < 512) {
					raw_response_preview += chunk_text;
					if (raw_response_preview.length() > 512) {
						raw_response_preview = raw_response_preview.substr(0, 512);
					}
				}
				event_buffer.append_array(chunk);
				print_line(vformat("[AI Agent][HTTP] Read streaming chunk. chunk_bytes=%d buffered_bytes=%d", (int)chunk.size(), (int)event_buffer.size()));

				while (true) {
					int separator_size = 0;
					const int separator_index = _find_openai_sse_event_separator(event_buffer, separator_size);
					if (separator_index < 0) {
						break;
					}

					const PackedByteArray raw_event_bytes = event_buffer.slice(0, separator_index);
					event_buffer = event_buffer.slice(separator_index + separator_size);
					const String raw_event = raw_event_bytes.is_empty() ? String() : String::utf8(reinterpret_cast<const char *>(raw_event_bytes.ptr()), raw_event_bytes.size()).replace("\r\n", "\n");

					if (_process_openai_sse_event(raw_event, p_stream_event_callback, stream_event_count)) {
						saw_done = true;
						break;
					}
				}
			}
		} else if (status == HTTPClient::STATUS_CONNECTED && client->has_response() && !received_response) {
			received_response = true;
			print_line("[AI Agent][HTTP] Streaming response headers received with no body yet.");
		} else if (status == HTTPClient::STATUS_CONNECTED && received_body) {
			print_line("[AI Agent][HTTP] Streaming response complete; connection kept alive.");
			break;
		} else if (status == HTTPClient::STATUS_DISCONNECTED) {
			print_line("[AI Agent][HTTP] Streaming provider disconnected; response read loop ended.");
			break;
		} else if (status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
			r_error = vformat("Provider TLS handshake failed: %s:%d%s", host, port, request_path);
			print_line(vformat("[AI Agent][HTTP] Streaming TLS handshake failed. host=%s port=%d path=%s", host, port, request_path));
			return false;
		} else if (status >= HTTPClient::STATUS_CONNECTION_ERROR) {
			r_error = vformat("Provider connection error: status=%d host=%s path=%s", (int)status, host, request_path);
			print_line(vformat("[AI Agent][HTTP] Streaming connection error while waiting response. status=%d host=%s path=%s buffered_bytes=%d", (int)status, host, request_path, (int)event_buffer.size()));
			return false;
		}

		if (saw_done) {
			break;
		}

		if (OS::get_singleton()->get_ticks_msec() - start_time > timeout_msec) {
			const int64_t elapsed_ms = (int64_t)(OS::get_singleton()->get_ticks_msec() - start_time);
			r_error = vformat("Provider request timed out: host=%s path=%s elapsed_ms=%d received_events=%d", host, request_path, elapsed_ms, stream_event_count);
			print_line(vformat("[AI Agent][HTTP] Streaming request timed out. elapsed_ms=%d events=%d last_status=%d", elapsed_ms, stream_event_count, last_status));
			return false;
		}
		OS::get_singleton()->delay_usec(1000);
	}

	if (!saw_done && !event_buffer.is_empty()) {
		String trailing_event = String::utf8(reinterpret_cast<const char *>(event_buffer.ptr()), event_buffer.size()).replace("\r\n", "\n");
		if (!trailing_event.strip_edges().is_empty()) {
			print_line(vformat("[AI Agent][HTTP] Processing trailing streaming buffer. bytes=%d", (int)event_buffer.size()));
			saw_done = _process_openai_sse_event(trailing_event, p_stream_event_callback, stream_event_count);
		}
	}

	const int response_code = client->get_response_code();
	print_line(vformat("[AI Agent][HTTP] Provider streaming response complete. http_code=%d events=%d elapsed_ms=%d", response_code, stream_event_count, (int64_t)(OS::get_singleton()->get_ticks_msec() - start_time)));
	if (response_code < 200 || response_code >= 300) {
		String response_preview = raw_response_preview.strip_edges();
		if (response_preview.length() > 512) {
			response_preview = response_preview.substr(0, 512) + "...";
		}
		r_error = vformat("Provider returned HTTP %d from %s%s: %s", response_code, host, request_path, response_preview);
		print_line(vformat("[AI Agent][HTTP] Provider returned non-success streaming HTTP code. code=%d preview=%s", response_code, response_preview));
		return false;
	}
	if (stream_event_count == 0) {
		r_error = "Provider streaming response did not contain events.";
		print_line("[AI Agent][HTTP] Streaming request failed: no events received.");
		return false;
	}

	print_line("[AI Agent][HTTP] Provider streaming request succeeded.");
	return true;
}

void AIOpenAICompatibleRuntimeClient::_bind_methods() {
}

AIOpenAICompatibleRuntimeClient::AIOpenAICompatibleRuntimeClient() {
	Ref<AIOpenAIHTTPRuntimeTransport> http_transport;
	http_transport.instantiate();
	transport = http_transport;
}

void AIOpenAICompatibleRuntimeClient::set_config(const AIProviderConfig &p_config) {
	config = p_config;
}

AIProviderConfig AIOpenAICompatibleRuntimeClient::get_config() const {
	return config;
}

void AIOpenAICompatibleRuntimeClient::set_transport(const Ref<AIOpenAIRuntimeTransport> &p_transport) {
	transport = p_transport;
}

Ref<AIOpenAIRuntimeTransport> AIOpenAICompatibleRuntimeClient::get_transport() const {
	return transport;
}

String AIOpenAICompatibleRuntimeClient::_to_provider_tool_name(const String &p_internal_tool_name) {
	String provider_name;
	for (int i = 0; i < p_internal_tool_name.length(); i++) {
		const char32_t c = p_internal_tool_name[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
			provider_name += String::chr(c);
		} else {
			provider_name += "_";
		}
	}
	return provider_name;
}

String AIOpenAICompatibleRuntimeClient::_to_internal_tool_name(const String &p_provider_tool_name, const Dictionary &p_tool_name_map) {
	if (p_tool_name_map.has(p_provider_tool_name)) {
		return String(p_tool_name_map.get(p_provider_tool_name, p_provider_tool_name));
	}
	return p_provider_tool_name;
}

Array AIOpenAICompatibleRuntimeClient::_build_provider_tool_schemas(const Array &p_tool_schemas, Dictionary &r_tool_name_map) {
	Array provider_tool_schemas;
	r_tool_name_map.clear();

	for (int i = 0; i < p_tool_schemas.size(); i++) {
		if (Variant(p_tool_schemas[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary schema = p_tool_schemas[i];
		if (!schema.has("function") || Variant(schema["function"]).get_type() != Variant::DICTIONARY) {
			provider_tool_schemas.push_back(schema.duplicate(true));
			continue;
		}

		Dictionary provider_schema = schema.duplicate(true);
		Dictionary function = provider_schema["function"];
		const String internal_name = String(function.get("name", ""));
		if (internal_name.is_empty()) {
			provider_tool_schemas.push_back(provider_schema);
			continue;
		}

		String provider_name = _to_provider_tool_name(internal_name);
		if (r_tool_name_map.has(provider_name) && String(r_tool_name_map[provider_name]) != internal_name) {
			provider_name = provider_name + "_" + String::num_uint64(internal_name.hash64(), 16);
		}

		function["name"] = provider_name;
		provider_schema["function"] = function;
		r_tool_name_map[provider_name] = internal_name;
		provider_tool_schemas.push_back(provider_schema);
	}

	return provider_tool_schemas;
}

Array AIOpenAICompatibleRuntimeClient::_build_chat_messages(const Array &p_messages, const Dictionary &p_tool_name_map) {
	Array chat_messages;
	for (int i = 0; i < p_messages.size(); i++) {
		if (Variant(p_messages[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary message = p_messages[i];
		const String role = String(message.get("role", "user"));
		Dictionary chat_message;

		if (role == "tool") {
			chat_message["role"] = "tool";
			chat_message["content"] = (message.has("content") && Variant(message["content"]).get_type() != Variant::NIL) ? String(message["content"]) : String();
			if (message.has("metadata") && Variant(message["metadata"]).get_type() == Variant::DICTIONARY) {
				Dictionary metadata = message["metadata"];
				chat_message["tool_call_id"] = String(metadata.get("tool_call_id", ""));
			}
			chat_messages.push_back(chat_message);
			continue;
		}

		if (role != "system" && role != "user" && role != "assistant") {
			continue;
		}

		chat_message["role"] = role;
		chat_message["content"] = (message.has("content") && Variant(message["content"]).get_type() != Variant::NIL) ? String(message["content"]) : String();

		if (role == "assistant" && message.has("metadata") && Variant(message["metadata"]).get_type() == Variant::DICTIONARY) {
			Dictionary metadata = message["metadata"];
			if (metadata.has("reasoning_content") && Variant(metadata["reasoning_content"]).get_type() != Variant::NIL) {
				chat_message["reasoning_content"] = String(metadata["reasoning_content"]);
			}
			if (metadata.has("tool_calls") && Variant(metadata["tool_calls"]).get_type() == Variant::ARRAY) {
				Array internal_tool_calls = metadata["tool_calls"];
				Array openai_tool_calls;
				for (int j = 0; j < internal_tool_calls.size(); j++) {
					if (Variant(internal_tool_calls[j]).get_type() != Variant::DICTIONARY) {
						continue;
					}

					Dictionary internal_call = internal_tool_calls[j];
					Dictionary function;
					const String internal_tool_name = String(internal_call.get("tool_name", ""));
					String provider_tool_name = _to_provider_tool_name(internal_tool_name);
					if (!p_tool_name_map.is_empty()) {
						Array provider_tool_names = p_tool_name_map.keys();
						for (int k = 0; k < provider_tool_names.size(); k++) {
							const String mapped_provider_name = String(provider_tool_names[k]);
							if (String(p_tool_name_map.get(mapped_provider_name, "")) == internal_tool_name) {
								provider_tool_name = mapped_provider_name;
								break;
							}
						}
					}
					function["name"] = provider_tool_name;
					function["arguments"] = JSON::stringify(internal_call.get("arguments", Dictionary()));

					Dictionary openai_call;
					openai_call["id"] = String(internal_call.get("id", ""));
					openai_call["type"] = "function";
					openai_call["function"] = function;
					openai_tool_calls.push_back(openai_call);
				}

				if (!openai_tool_calls.is_empty()) {
					chat_message["tool_calls"] = openai_tool_calls;
				}
			}
		}

		chat_messages.push_back(chat_message);
	}
	return chat_messages;
}

void AIOpenAICompatibleRuntimeClient::_apply_tool_name_map(AIAgentRuntimeResponse &r_response, const Dictionary &p_tool_name_map) {
	for (int i = 0; i < r_response.tool_calls.size(); i++) {
		r_response.tool_calls.write[i].tool_name = _to_internal_tool_name(r_response.tool_calls[i].tool_name, p_tool_name_map);
	}
}

Array AIOpenAICompatibleRuntimeClient::build_chat_messages_for_test(const Array &p_messages) {
	return _build_chat_messages(p_messages);
}

Array AIOpenAICompatibleRuntimeClient::build_provider_tool_schemas_for_test(const Array &p_tool_schemas, Dictionary &r_tool_name_map) {
	return _build_provider_tool_schemas(p_tool_schemas, r_tool_name_map);
}

void AIOpenAICompatibleRuntimeClient::apply_tool_name_map_for_test(AIAgentRuntimeResponse &r_response, const Dictionary &p_tool_name_map) {
	_apply_tool_name_map(r_response, p_tool_name_map);
}

AIAgentRuntimeResponse AIOpenAICompatibleRuntimeClient::_complete_internal(const Array &p_messages, const Array &p_tool_schemas, const Callable &p_partial_response_callback, bool p_stream) {
	AIAgentRuntimeResponse response;
	if (transport.is_null()) {
		response.error = "OpenAI runtime transport is not configured.";
		print_line("[AI Agent][RuntimeClient] Failed: transport is not configured.");
		return response;
	}

	print_line(vformat("[AI Agent][RuntimeClient] Completing provider turn. input_messages=%d tool_schemas=%d streaming=%s", p_messages.size(), p_tool_schemas.size(), p_stream ? "yes" : "no"));
	Dictionary tool_name_map;
	Array provider_tool_schemas = _build_provider_tool_schemas(p_tool_schemas, tool_name_map);
	Array chat_messages = _build_chat_messages(p_messages, tool_name_map);
	print_line(vformat("[AI Agent][RuntimeClient] Built OpenAI-compatible messages. chat_messages=%d provider_tool_schemas=%d", chat_messages.size(), provider_tool_schemas.size()));

	if (p_stream) {
		Ref<AIOpenAIStreamEventHandler> stream_handler;
		stream_handler.instantiate();
		stream_handler->set_partial_response_callback(p_partial_response_callback);

		String transport_error;
		if (!transport->request_chat_completion_stream(config, chat_messages, provider_tool_schemas, callable_mp(stream_handler.ptr(), &AIOpenAIStreamEventHandler::handle_event), transport_error)) {
			if (transport_error == "OpenAI streaming runtime transport is not implemented.") {
				print_line("[AI Agent][RuntimeClient] Streaming transport is not implemented; falling back to non-streaming provider request.");
				return _complete_internal(p_messages, p_tool_schemas, Callable(), false);
			}
			response.error = transport_error.is_empty() ? String("Provider streaming request failed.") : transport_error;
			print_line(vformat("[AI Agent][RuntimeClient] Provider streaming transport failed: %s", response.error));
			return response;
		}

		String final_parse_error;
		response = stream_handler->finish(final_parse_error);
		if (!final_parse_error.is_empty()) {
			response.error = final_parse_error;
			print_line(vformat("[AI Agent][RuntimeClient] Provider streaming response finalize failed: %s", response.error));
			return response;
		}
		_apply_tool_name_map(response, tool_name_map);
		print_line(vformat("[AI Agent][RuntimeClient] Provider streaming response parsed. content_chars=%d tool_calls=%d", response.content.length(), response.tool_calls.size()));
		return response;
	}

	String response_text;
	String transport_error;
	if (!transport->request_chat_completion(config, chat_messages, provider_tool_schemas, response_text, transport_error)) {
		response.error = transport_error.is_empty() ? String("Provider request failed.") : transport_error;
		print_line(vformat("[AI Agent][RuntimeClient] Provider transport failed: %s", response.error));
		return response;
	}

	print_line(vformat("[AI Agent][RuntimeClient] Parsing provider response. response_chars=%d", response_text.length()));
	String parse_error;
	if (!AIOpenAICompatibleCodec::parse_chat_completion(response_text, response, parse_error)) {
		response.error = parse_error.is_empty() ? String("Failed to parse provider response.") : parse_error;
		print_line(vformat("[AI Agent][RuntimeClient] Provider response parse failed: %s", response.error));
	} else {
		_apply_tool_name_map(response, tool_name_map);
		print_line(vformat("[AI Agent][RuntimeClient] Provider response parsed. content_chars=%d tool_calls=%d", response.content.length(), response.tool_calls.size()));
	}
	return response;
}

AIAgentRuntimeResponse AIOpenAICompatibleRuntimeClient::complete(const Array &p_messages, const Array &p_tool_schemas) {
	return _complete_internal(p_messages, p_tool_schemas, Callable(), false);
}

AIAgentRuntimeResponse AIOpenAICompatibleRuntimeClient::complete_streaming(const Array &p_messages, const Array &p_tool_schemas, const Callable &p_partial_response_callback) {
	return _complete_internal(p_messages, p_tool_schemas, p_partial_response_callback, true);
}
