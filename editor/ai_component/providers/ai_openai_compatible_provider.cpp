/**************************************************************************/
/*  ai_openai_compatible_provider.cpp                                     */
/**************************************************************************/

#include "ai_openai_compatible_provider.h"

#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/os/os.h"

#include "editor/ai_component/agent/ai_agent_runtime.h"

void AIOpenAICompatibleProvider::_bind_methods() {
}

void AIOpenAICompatibleProvider::_notification(int p_what) {
	if (p_what == NOTIFICATION_PREDELETE) {
		cancel();
	}
}

AIOpenAICompatibleProvider::AIOpenAICompatibleProvider() {
	json.instantiate();
	cancel_requested.clear();
	running.clear();
}

AIOpenAICompatibleProvider::~AIOpenAICompatibleProvider() {
	cancel();
}

AIProviderFeatures AIOpenAICompatibleProvider::get_features() const {
	AIProviderFeatures features;
	features.supports_streaming = true;
	features.supports_tools = false;
	return features;
}

bool AIOpenAICompatibleProvider::start_chat(const Array &p_messages) {
	if (running.is_set()) {
		cancel();
	}
	if (config.api_key.strip_edges().is_empty()) {
		call_deferred("emit_signal", SNAME("request_failed"), "API key is not configured.");
		return false;
	}
	if (config.base_url.strip_edges().is_empty()) {
		call_deferred("emit_signal", SNAME("request_failed"), "Provider base URL is not configured.");
		return false;
	}

	ThreadParams *params = memnew(ThreadParams);
	params->self = this;
	params->messages = p_messages.duplicate(true);
	params->config = config;

	cancel_requested.clear();
	running.set();
	thread.start(_thread_func, params);
	return true;
}

void AIOpenAICompatibleProvider::cancel() {
	cancel_requested.set();
	if (thread.is_started()) {
		thread.wait_to_finish();
	}
	running.clear();
}

void AIOpenAICompatibleProvider::_thread_func(void *p_userdata) {
	ThreadParams *params = static_cast<ThreadParams *>(p_userdata);
	AIOpenAICompatibleProvider *self = params->self;
	AIProviderConfig request_config = params->config;
	Array messages = params->messages;
	memdelete(params);

	String scheme;
	String host;
	String base_path;
	String fragment;
	int port = 0;
	Error err = request_config.base_url.parse_url(scheme, host, port, base_path, fragment);
	if (err != OK || (scheme != "https://" && scheme != "http://") || host.is_empty()) {
		self->running.clear();
		self->call_deferred("emit_signal", SNAME("request_failed"), "Provider base URL is invalid.");
		return;
	}

	const bool use_tls = scheme == "https://";
	if (port == 0) {
		port = use_tls ? 443 : 80;
	}

	Ref<HTTPClient> client = HTTPClient::create();
	client->set_blocking_mode(true);
	err = client->connect_to_host(host, port, use_tls ? Ref<TLSOptions>(TLSOptions::client()) : Ref<TLSOptions>());
	if (err != OK) {
		self->running.clear();
		self->call_deferred("emit_signal", SNAME("request_failed"), "Failed to connect to provider.");
		return;
	}

	while (!self->cancel_requested.is_set() &&
			(client->get_status() == HTTPClient::STATUS_CONNECTING ||
					client->get_status() == HTTPClient::STATUS_RESOLVING)) {
		client->poll();
		OS::get_singleton()->delay_usec(1000);
	}

	if (self->cancel_requested.is_set()) {
		self->running.clear();
		self->call_deferred("emit_signal", SNAME("response_finished"), "cancelled");
		return;
	}

	if (client->get_status() != HTTPClient::STATUS_CONNECTED) {
		self->running.clear();
		self->call_deferred("emit_signal", SNAME("request_failed"), "Provider connection failed.");
		return;
	}

	Vector<String> headers;
	headers.push_back("Authorization: Bearer " + request_config.api_key);
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: text/event-stream");

	PackedByteArray body = _build_body(messages, request_config.model);
	String request_path = build_request_path(base_path);
	err = client->request(HTTPClient::METHOD_POST, request_path, headers, body.ptr(), body.size());
	if (err != OK) {
		self->running.clear();
		self->call_deferred("emit_signal", SNAME("request_failed"), vformat("Failed to send provider request (%d): %s", err, request_path));
		return;
	}

	self->call_deferred("emit_signal", SNAME("response_started"));

	Ref<AISSEParser> parser;
	parser.instantiate();
	PackedByteArray utf8_buffer;

	while (!self->cancel_requested.is_set()) {
		client->poll();
		HTTPClient::Status status = client->get_status();

		if (status == HTTPClient::STATUS_BODY) {
			PackedByteArray chunk = client->read_response_body_chunk();
			if (!chunk.is_empty()) {
				utf8_buffer.append_array(chunk);
				int valid_length = utf8_buffer.size();
				while (valid_length > 0 && !_is_valid_utf8(utf8_buffer.ptr(), valid_length)) {
					valid_length--;
				}
				if (valid_length == 0) {
					continue;
				}

				String chunk_string = String::utf8(reinterpret_cast<const char *>(utf8_buffer.ptr()), valid_length);
				utf8_buffer = utf8_buffer.slice(valid_length);
				Array events = parser->push_chunk(chunk_string);
				for (int i = 0; i < events.size(); i++) {
					String event = events[i];
					if (event == "[DONE]") {
						self->running.clear();
						self->call_deferred("emit_signal", SNAME("response_finished"), "stop");
						return;
					}

					String delta;
					String finish_reason;
					String error_message;
					if (!_extract_delta(event, delta, finish_reason, error_message)) {
						if (!error_message.is_empty()) {
							self->running.clear();
							self->call_deferred("emit_signal", SNAME("request_failed"), error_message);
							return;
						}
						continue;
					}
					if (!delta.is_empty()) {
						self->call_deferred("emit_signal", SNAME("response_delta"), delta);
					}
					if (!finish_reason.is_empty() && finish_reason != "null") {
						self->running.clear();
						self->call_deferred("emit_signal", SNAME("response_finished"), finish_reason);
						return;
					}
				}
			}
		} else if (status == HTTPClient::STATUS_DISCONNECTED) {
			self->running.clear();
			self->call_deferred("emit_signal", SNAME("response_finished"), "stop");
			return;
		} else if (status >= HTTPClient::STATUS_CONNECTION_ERROR) {
			self->running.clear();
			self->call_deferred("emit_signal", SNAME("request_failed"), "Provider connection error.");
			return;
		}

		OS::get_singleton()->delay_usec(1000);
	}

	self->running.clear();
	self->call_deferred("emit_signal", SNAME("response_finished"), "cancelled");
}

String AIOpenAICompatibleProvider::build_request_path(const String &p_base_path) {
	String path = p_base_path.strip_edges();
	if (path.is_empty()) {
		path = "/";
	}
	if (!path.begins_with("/")) {
		path = "/" + path;
	}
	while (path.length() > 1 && path.ends_with("/")) {
		path = path.substr(0, path.length() - 1);
	}
	if (path.ends_with("/chat/completions")) {
		return path;
	}
	if (path == "/") {
		return "/chat/completions";
	}
	return path + "/chat/completions";
}

PackedByteArray AIOpenAICompatibleProvider::build_body_for_test(const Array &p_messages, const String &p_model, const Array &p_tool_schemas, bool p_stream) {
	return build_body(p_messages, p_model, p_tool_schemas, p_stream);
}

PackedByteArray AIOpenAICompatibleProvider::build_body(const Array &p_messages, const String &p_model, const Array &p_tool_schemas, bool p_stream) {
	return _build_body(p_messages, p_model, p_tool_schemas, p_stream);
}

PackedByteArray AIOpenAICompatibleProvider::_build_body(const Array &p_messages, const String &p_model, const Array &p_tool_schemas, bool p_stream) {
	Dictionary body;
	body["model"] = p_model;
	body["stream"] = p_stream;
	body["messages"] = p_messages;
	if (!p_tool_schemas.is_empty()) {
		body["tools"] = p_tool_schemas;
	}
	return JSON::stringify(body).to_utf8_buffer();
}

bool AIOpenAICompatibleProvider::parse_chat_completion_for_test(const String &p_response_text, AIAgentRuntimeResponse &r_response, String &r_error) {
	return parse_chat_completion(p_response_text, r_response, r_error);
}

bool AIOpenAICompatibleProvider::extract_delta_for_test(const String &p_event, String &r_delta, String &r_finish_reason, String &r_error) {
	return _extract_delta(p_event, r_delta, r_finish_reason, r_error);
}

bool AIOpenAICompatibleProvider::parse_chat_completion(const String &p_response_text, AIAgentRuntimeResponse &r_response, String &r_error) {
	return _parse_chat_completion(p_response_text, r_response, r_error);
}

bool AIOpenAICompatibleProvider::_parse_chat_completion(const String &p_response_text, AIAgentRuntimeResponse &r_response, String &r_error) {
	r_response = AIAgentRuntimeResponse();
	r_error = String();

	Ref<JSON> parser;
	parser.instantiate();
	Error err = parser->parse(p_response_text);
	if (err != OK) {
		r_error = "Failed to parse provider response.";
		return false;
	}

	Variant data = parser->get_data();
	if (data.get_type() != Variant::DICTIONARY) {
		r_error = "Provider response was not an object.";
		return false;
	}

	Dictionary dict = data;
	if (dict.has("error")) {
		Variant error_data = dict["error"];
		if (error_data.get_type() == Variant::DICTIONARY) {
			Dictionary error_dict = error_data;
			if (error_dict.has("message")) {
				r_error = error_dict["message"];
			}
		}
		if (r_error.is_empty()) {
			r_error = "Provider returned an error.";
		}
		return false;
	}

	if (!dict.has("choices") || Variant(dict["choices"]).get_type() != Variant::ARRAY) {
		r_error = "Provider response did not contain choices.";
		return false;
	}

	Array choices = dict["choices"];
	if (choices.is_empty() || Variant(choices[0]).get_type() != Variant::DICTIONARY) {
		r_error = "Provider response did not contain a valid choice.";
		return false;
	}

	Dictionary choice = choices[0];
	if (!choice.has("message") || Variant(choice["message"]).get_type() != Variant::DICTIONARY) {
		r_error = "Provider response did not contain a message.";
		return false;
	}

	Dictionary message = choice["message"];
	if (message.has("content") && Variant(message["content"]).get_type() != Variant::NIL) {
		r_response.content = String(message["content"]);
	}
	if (choice.has("finish_reason") && Variant(choice["finish_reason"]).get_type() != Variant::NIL) {
		r_response.metadata["finish_reason"] = String(choice["finish_reason"]);
	}

	if (!message.has("tool_calls") || Variant(message["tool_calls"]).get_type() != Variant::ARRAY) {
		return true;
	}

	Array tool_calls = message["tool_calls"];
	for (int i = 0; i < tool_calls.size(); i++) {
		if (Variant(tool_calls[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary tool_call_dict = tool_calls[i];
		if (!tool_call_dict.has("function") || Variant(tool_call_dict["function"]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary function_dict = tool_call_dict["function"];
		AIToolCall call;
		if (tool_call_dict.has("id")) {
			call.id = String(tool_call_dict["id"]);
		}
		if (function_dict.has("name")) {
			call.tool_name = String(function_dict["name"]);
		}

		if (function_dict.has("arguments")) {
			String arguments_text = String(function_dict["arguments"]);
			Ref<JSON> arguments_parser;
			arguments_parser.instantiate();
			Error arguments_err = arguments_parser->parse(arguments_text);
			if (arguments_err != OK || arguments_parser->get_data().get_type() != Variant::DICTIONARY) {
				r_error = "Failed to parse provider tool arguments.";
				return false;
			}
			call.arguments = arguments_parser->get_data();
		}

		if (!call.tool_name.is_empty()) {
			r_response.tool_calls.push_back(call);
		}
	}

	return true;
}

bool AIOpenAICompatibleProvider::_extract_delta(const String &p_event, String &r_delta, String &r_finish_reason, String &r_error) {
	Ref<JSON> parser;
	parser.instantiate();
	Error err = parser->parse(p_event);
	if (err != OK) {
		r_error = "Failed to parse provider stream.";
		return false;
	}

	Variant data = parser->get_data();
	if (data.get_type() != Variant::DICTIONARY) {
		return false;
	}

	Dictionary dict = data;
	if (dict.has("error")) {
		Variant error_data = dict["error"];
		if (error_data.get_type() == Variant::DICTIONARY) {
			Dictionary error_dict = error_data;
			if (error_dict.has("message")) {
				r_error = error_dict["message"];
			}
		}
		if (r_error.is_empty()) {
			r_error = "Provider returned an error.";
		}
		return false;
	}

	if (!dict.has("choices")) {
		return false;
	}
	Array choices = dict["choices"];
	if (choices.is_empty() || Variant(choices[0]).get_type() != Variant::DICTIONARY) {
		return false;
	}

	Dictionary choice = choices[0];
	if (choice.has("finish_reason") && Variant(choice["finish_reason"]).get_type() != Variant::NIL) {
		r_finish_reason = String(choice["finish_reason"]);
	}
	if (choice.has("delta") && Variant(choice["delta"]).get_type() == Variant::DICTIONARY) {
		Dictionary delta = choice["delta"];
		if (delta.has("content") && Variant(delta["content"]).get_type() != Variant::NIL) {
			r_delta = String(delta["content"]);
		}
	}

	return true;
}

bool AIOpenAICompatibleProvider::_is_valid_utf8(const uint8_t *p_data, int p_length) {
	int i = 0;
	while (i < p_length) {
		if (p_data[i] <= 0x7F) {
			i++;
		} else if ((p_data[i] & 0xE0) == 0xC0) {
			if (i + 1 >= p_length || (p_data[i + 1] & 0xC0) != 0x80) {
				return false;
			}
			i += 2;
		} else if ((p_data[i] & 0xF0) == 0xE0) {
			if (i + 2 >= p_length || (p_data[i + 1] & 0xC0) != 0x80 || (p_data[i + 2] & 0xC0) != 0x80) {
				return false;
			}
			i += 3;
		} else if ((p_data[i] & 0xF8) == 0xF0) {
			if (i + 3 >= p_length || (p_data[i + 1] & 0xC0) != 0x80 || (p_data[i + 2] & 0xC0) != 0x80 || (p_data[i + 3] & 0xC0) != 0x80) {
				return false;
			}
			i += 4;
		} else {
			return false;
		}
	}
	return true;
}
