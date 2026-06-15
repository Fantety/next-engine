/**************************************************************************/
/*  ai_openai_compatible_runtime.cpp                                      */
/**************************************************************************/

#include "ai_openai_compatible_runtime.h"

#include "core/io/json.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"

class AIOpenAICompatibleStreamBridge : public RefCounted {
	struct ToolCallState {
		String id;
		String name;
		String arguments;
		bool emitted = false;
	};

	Ref<AIStreamSink> sink;
	Ref<AICancelToken> cancel_token;
	String error;
	String text_id;
	String reasoning_id;
	Vector<ToolCallState> tool_calls;

	bool _push_event(const AIStreamEvent &p_event) {
		if (sink.is_null()) {
			error = "OpenAI stream bridge has no sink.";
			return true;
		}
		bool stop_requested = false;
		String sink_error;
		if (!sink->push_event(p_event, stop_requested, sink_error)) {
			error = sink_error;
			return true;
		}
		return stop_requested;
	}

	ToolCallState &_get_tool_call_state(int p_index) {
		while (tool_calls.size() <= p_index) {
			tool_calls.push_back(ToolCallState());
		}
		return tool_calls.write[p_index];
	}

	Variant _parse_tool_arguments(const String &p_arguments) {
		const String arguments = p_arguments.strip_edges();
		if (arguments.is_empty()) {
			return Dictionary();
		}

		Ref<JSON> json;
		json.instantiate();
		const Error parse_err = json->parse(arguments);
		if (parse_err == OK) {
			return json->get_data();
		}

		Dictionary fallback;
		fallback["raw"] = p_arguments;
		return fallback;
	}

	static int64_t _usage_int(const Dictionary &p_dict, const String &p_key, int64_t p_default = 0) {
		if (!p_dict.has(p_key) || p_dict[p_key].get_type() == Variant::NIL) {
			return p_default;
		}
		return int64_t(p_dict[p_key]);
	}

	static Dictionary _normalize_usage(const Dictionary &p_usage) {
		Dictionary prompt_details;
		if (p_usage.get("prompt_tokens_details", Variant()).get_type() == Variant::DICTIONARY) {
			prompt_details = Dictionary(p_usage["prompt_tokens_details"]);
		} else if (p_usage.get("input_token_details", Variant()).get_type() == Variant::DICTIONARY) {
			prompt_details = Dictionary(p_usage["input_token_details"]);
		}

		const int64_t cache_read_tokens = MAX(int64_t(0), _usage_int(prompt_details, "cached_tokens", _usage_int(p_usage, "cache_read_tokens", _usage_int(p_usage, "cache_read", 0))));
		const int64_t raw_input_tokens = MAX(int64_t(0), _usage_int(p_usage, "prompt_tokens", _usage_int(p_usage, "input_tokens", _usage_int(p_usage, "input", 0))));
		const int64_t output_tokens = MAX(int64_t(0), _usage_int(p_usage, "completion_tokens", _usage_int(p_usage, "output_tokens", _usage_int(p_usage, "output", 0))));
		const int64_t cache_write_tokens = MAX(int64_t(0), _usage_int(p_usage, "cache_write_tokens", _usage_int(p_usage, "cache_write", 0)));
		const int64_t input_tokens = MAX(int64_t(0), raw_input_tokens - cache_read_tokens - cache_write_tokens);

		Dictionary usage;
		usage["input_tokens"] = input_tokens;
		usage["output_tokens"] = output_tokens;
		usage["cache_read_tokens"] = cache_read_tokens;
		usage["cache_write_tokens"] = cache_write_tokens;
		usage["total_tokens"] = input_tokens + output_tokens + cache_read_tokens + cache_write_tokens;
		return usage;
	}

	bool _handle_usage(const Dictionary &p_root) {
		if (p_root.get("usage", Variant()).get_type() != Variant::DICTIONARY) {
			return false;
		}

		const Dictionary raw_usage = p_root["usage"];
		AIStreamEvent event = AIStreamEvent::usage_event(_normalize_usage(raw_usage));
		event.provider_metadata["provider"] = "openai-compatible";
		event.provider_metadata["raw_usage"] = raw_usage.duplicate(true);
		return _push_event(event);
	}

	bool _handle_tool_call_deltas(const Dictionary &p_delta) {
		if (p_delta.get("tool_calls", Variant()).get_type() != Variant::ARRAY) {
			return false;
		}

		const Array delta_tool_calls = p_delta["tool_calls"];
		for (int i = 0; i < delta_tool_calls.size(); i++) {
			if (delta_tool_calls[i].get_type() != Variant::DICTIONARY) {
				continue;
			}

			const Dictionary delta_tool_call = delta_tool_calls[i];
			const int index = int(delta_tool_call.get("index", i));
			ToolCallState &tool_call = _get_tool_call_state(index < 0 ? i : index);
			if (delta_tool_call.has("id") && delta_tool_call["id"].get_type() != Variant::NIL) {
				tool_call.id = delta_tool_call["id"];
			}
			if (delta_tool_call.get("function", Variant()).get_type() == Variant::DICTIONARY) {
				const Dictionary function = delta_tool_call["function"];
				if (function.has("name") && function["name"].get_type() != Variant::NIL) {
					tool_call.name = function["name"];
				}
				if (function.has("arguments") && function["arguments"].get_type() != Variant::NIL) {
					tool_call.arguments += String(function["arguments"]);
				}
			}
		}
		return false;
	}

	bool _flush_tool_calls() {
		for (int i = 0; i < tool_calls.size(); i++) {
			ToolCallState &tool_call = tool_calls.write[i];
			if (tool_call.emitted) {
				continue;
			}
			if (tool_call.name.is_empty()) {
				if (!tool_call.id.is_empty() || !tool_call.arguments.strip_edges().is_empty()) {
					error = vformat("Provider returned incomplete tool call at index %d: missing function name.", i);
					return true;
				}
				continue;
			}

			const String call_id = tool_call.id.is_empty() ? vformat("tool_call_%d", i) : tool_call.id;
			AIStreamEvent event = AIStreamEvent::tool_call(call_id, tool_call.name, _parse_tool_arguments(tool_call.arguments));
			event.provider_metadata["provider"] = "openai-compatible";
			event.provider_metadata["index"] = i;
			event.provider_metadata["id"] = tool_call.id;
			event.provider_metadata["raw_arguments"] = tool_call.arguments;
			tool_call.emitted = true;
			if (_push_event(event)) {
				return true;
			}
		}
		return false;
	}

public:
	void setup(const Ref<AIStreamSink> &p_sink, const Ref<AICancelToken> &p_cancel_token) {
		sink = p_sink;
		cancel_token = p_cancel_token;
		text_id = "text";
		reasoning_id = "reasoning";
	}

	String get_error() const {
		return error;
	}

	bool flush_pending_tool_calls() {
		if (cancel_token.is_valid() && cancel_token->is_cancel_requested()) {
			error = cancel_token->get_cancel_message("OpenAI-compatible stream interrupted.");
			return true;
		}
		return _flush_tool_calls();
	}

	bool handle_sse_event(const Dictionary &p_event) {
		if (cancel_token.is_valid() && cancel_token->is_cancel_requested()) {
			error = cancel_token->get_cancel_message("OpenAI-compatible stream interrupted.");
			return true;
		}

		const String data = String(p_event.get("data", String())).strip_edges();
		if (data.is_empty()) {
			return false;
		}
		if (data == "[DONE]") {
			if (_flush_tool_calls()) {
				return true;
			}
			return true;
		}

		Ref<JSON> json;
		json.instantiate();
		const Error parse_err = json->parse(data);
		if (parse_err != OK || json->get_data().get_type() != Variant::DICTIONARY) {
			error = "Failed to parse OpenAI-compatible stream event.";
			return true;
		}

		const Dictionary root = json->get_data();
		if (root.get("error", Variant()).get_type() == Variant::DICTIONARY) {
			const Dictionary error_dict = root["error"];
			error = error_dict.get("message", "Provider returned an error.");
			return true;
		}
		if (_handle_usage(root)) {
			return true;
		}
		if (root.get("choices", Variant()).get_type() != Variant::ARRAY) {
			return false;
		}

		const Array choices = root["choices"];
		if (choices.is_empty() || choices[0].get_type() != Variant::DICTIONARY) {
			return false;
		}

		const Dictionary choice = choices[0];
		if (choice.get("delta", Variant()).get_type() != Variant::DICTIONARY) {
			return false;
		}

		const Dictionary delta = choice["delta"];
		if (delta.has("content") && delta["content"].get_type() != Variant::NIL) {
			AIStreamEvent event = AIStreamEvent::text_delta(text_id, String(delta["content"]));
			if (_push_event(event)) {
				return true;
			}
		}
		if (delta.has("reasoning_content") && delta["reasoning_content"].get_type() != Variant::NIL) {
			AIStreamEvent event;
			event.type = AI_STREAM_EVENT_REASONING_DELTA;
			event.id = reasoning_id;
			event.text = String(delta["reasoning_content"]);
			if (_push_event(event)) {
				return true;
			}
		}
		if (_handle_tool_call_deltas(delta)) {
			return true;
		}
		if (String(choice.get("finish_reason", String())) == "tool_calls") {
			if (_flush_tool_calls()) {
				return true;
			}
		}
		return false;
	}
};

void AIOpenAICompatibleRuntime::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_base_url", "base_url"), &AIOpenAICompatibleRuntime::set_base_url);
	ClassDB::bind_method(D_METHOD("get_base_url"), &AIOpenAICompatibleRuntime::get_base_url);
	ClassDB::bind_method(D_METHOD("set_api_key", "api_key"), &AIOpenAICompatibleRuntime::set_api_key);
	ClassDB::bind_method(D_METHOD("get_api_key"), &AIOpenAICompatibleRuntime::get_api_key);
}

AIOpenAICompatibleRuntime::AIOpenAICompatibleRuntime() {
	http_client.instantiate();
}

String AIOpenAICompatibleRuntime::_chat_completions_url(const String &p_base_url) {
	String url = p_base_url.strip_edges();
	if (url.is_empty()) {
		url = "https://api.openai.com/v1";
	}
	while (url.ends_with("/")) {
		url = url.substr(0, url.length() - 1);
	}
	if (url.ends_with("/chat/completions")) {
		return url;
	}
	return url + "/chat/completions";
}

String AIOpenAICompatibleRuntime::_message_text(const AIModelMessage &p_message) {
	String text;
	for (int i = 0; i < p_message.parts.size(); i++) {
		const AIModelPart &part = p_message.parts[i];
		if (part.type != AI_MODEL_PART_TEXT) {
			continue;
		}
		text += text.is_empty() ? part.text : "\n" + part.text;
	}
	return text;
}

String AIOpenAICompatibleRuntime::_data_url_payload(const String &p_data_url) {
	const int comma = p_data_url.find(",");
	if (comma < 0) {
		return p_data_url;
	}
	return p_data_url.substr(comma + 1);
}

String AIOpenAICompatibleRuntime::_audio_format_from_mime(const String &p_mime) {
	const String mime = p_mime.strip_edges().to_lower();
	if (mime == "audio/wav" || mime == "audio/x-wav") {
		return "wav";
	}
	if (mime == "audio/ogg") {
		return "ogg";
	}
	if (mime == "audio/flac") {
		return "flac";
	}
	if (mime == "audio/mp4" || mime == "audio/m4a") {
		return "mp4";
	}
	return "mp3";
}

Dictionary AIOpenAICompatibleRuntime::_part_to_openai(const AIModelPart &p_part) {
	Dictionary result;
	if (p_part.type == AI_MODEL_PART_IMAGE) {
		Dictionary image_url;
		image_url["url"] = p_part.data;
		image_url["detail"] = String(p_part.metadata.get("detail", "auto")).strip_edges().is_empty() ? String("auto") : String(p_part.metadata.get("detail", "auto"));
		result["type"] = "image_url";
		result["image_url"] = image_url;
		return result;
	}
	if (p_part.type == AI_MODEL_PART_AUDIO) {
		Dictionary input_audio;
		input_audio["data"] = _data_url_payload(p_part.data);
		input_audio["format"] = _audio_format_from_mime(p_part.mime);
		result["type"] = "input_audio";
		result["input_audio"] = input_audio;
		return result;
	}
	if (p_part.type == AI_MODEL_PART_FILE) {
		Dictionary file;
		file["filename"] = p_part.name.strip_edges().is_empty() ? String("attachment") : p_part.name;
		file["file_data"] = p_part.data;
		result["type"] = "file";
		result["file"] = file;
		return result;
	}

	result["type"] = "text";
	result["text"] = p_part.text;
	return result;
}

String AIOpenAICompatibleRuntime::_tool_call_arguments_to_json(const AIModelToolCall &p_tool_call) {
	const Variant raw_arguments = p_tool_call.provider_metadata.get("raw_arguments", p_tool_call.provider_metadata.get("rawArguments", Variant()));
	if (raw_arguments.get_type() == Variant::STRING) {
		return String(raw_arguments);
	}

	if (p_tool_call.input.get_type() == Variant::NIL) {
		return "{}";
	}

	const String json = JSON::stringify(p_tool_call.input);
	return json.strip_edges().is_empty() ? String("{}") : json;
}

Array AIOpenAICompatibleRuntime::_tool_calls_to_openai(const AIModelMessage &p_message) {
	Array result;
	for (int i = 0; i < p_message.tool_calls.size(); i++) {
		const AIModelToolCall &tool_call = p_message.tool_calls[i];
		if (tool_call.id.strip_edges().is_empty() || tool_call.name.strip_edges().is_empty()) {
			continue;
		}

		Dictionary function;
		function["name"] = tool_call.name;
		function["arguments"] = _tool_call_arguments_to_json(tool_call);

		Dictionary call;
		call["id"] = tool_call.id;
		call["type"] = "function";
		call["function"] = function;
		result.push_back(call);
	}
	return result;
}

Dictionary AIOpenAICompatibleRuntime::_message_to_openai(const AIModelMessage &p_message) {
	Dictionary result;
	const String role = p_message.role.is_empty() ? String("user") : p_message.role;
	result["role"] = role;
	if (role == "tool") {
		result["tool_call_id"] = p_message.tool_call_id;
		result["content"] = _message_text(p_message);
		return result;
	}

	bool has_non_text = false;
	for (int i = 0; i < p_message.parts.size(); i++) {
		if (p_message.parts[i].type != AI_MODEL_PART_TEXT) {
			has_non_text = true;
			break;
		}
	}
	const Array tool_calls = _tool_calls_to_openai(p_message);
	if (!tool_calls.is_empty()) {
		const String text = _message_text(p_message);
		result["content"] = text.is_empty() ? Variant() : Variant(text);
		result["tool_calls"] = tool_calls;
		return result;
	}
	if (!has_non_text) {
		result["content"] = _message_text(p_message);
		return result;
	}

	Array content;
	for (int i = 0; i < p_message.parts.size(); i++) {
		content.push_back(_part_to_openai(p_message.parts[i]));
	}
	result["content"] = content;
	return result;
}

Array AIOpenAICompatibleRuntime::_messages_to_openai(const AIModelRequest &p_request) {
	Array result;
	String system_text;
	for (int i = 0; i < p_request.system.size(); i++) {
		if (p_request.system[i].type == AI_MODEL_PART_TEXT && !p_request.system[i].text.is_empty()) {
			system_text += system_text.is_empty() ? p_request.system[i].text : "\n" + p_request.system[i].text;
		}
	}
	if (!system_text.is_empty()) {
		Dictionary system_message;
		system_message["role"] = "system";
		system_message["content"] = system_text;
		result.push_back(system_message);
	}

	for (int i = 0; i < p_request.messages.size(); i++) {
		result.push_back(_message_to_openai(p_request.messages[i]));
	}
	return result;
}

Array AIOpenAICompatibleRuntime::_tools_to_openai(const AIModelRequest &p_request) {
	Array result;
	for (int i = 0; i < p_request.tools.size(); i++) {
		const AIModelToolDefinition &tool = p_request.tools[i];
		if (!tool.is_valid()) {
			continue;
		}
		Dictionary function;
		function["name"] = tool.name;
		function["description"] = tool.description;
		function["parameters"] = tool.input_schema;

		Dictionary wrapper;
		wrapper["type"] = "function";
		wrapper["function"] = function;
		result.push_back(wrapper);
	}
	return result;
}

PackedByteArray AIOpenAICompatibleRuntime::_body_to_bytes(const AIModelRequest &p_request) {
	Dictionary body;
	body["model"] = p_request.model;
	body["stream"] = true;
	Dictionary stream_options;
	stream_options["include_usage"] = true;
	body["stream_options"] = stream_options;
	body["messages"] = _messages_to_openai(p_request);
	const Array tools = _tools_to_openai(p_request);
	if (!tools.is_empty()) {
		body["tools"] = tools;
	}
	if (p_request.max_output_tokens > 0) {
		body["max_tokens"] = p_request.max_output_tokens;
	}
	return JSON::stringify(body).to_utf8_buffer();
}

String AIOpenAICompatibleRuntime::get_runtime_type() const {
	return "openai-compatible";
}

bool AIOpenAICompatibleRuntime::configure(const Dictionary &p_config, AIError &r_error) {
	base_url = p_config.get("base_url", p_config.get("url", base_url));
	api_key = p_config.get("api_key", api_key);
	organization = p_config.get("organization", organization);
	timeout_msec = int(p_config.get("timeout_msec", p_config.get("timeoutMs", timeout_msec)));
	r_error = AIError::none();
	return true;
}

bool AIOpenAICompatibleRuntime::stream_struct(const AIModelRequest &p_request, const Ref<AIStreamSink> &p_sink, const Ref<AICancelToken> &p_cancel_token, AIError &r_error) {
	if (p_sink.is_null()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "OpenAI-compatible runtime requires a stream sink.");
		return false;
	}

	const String request_base_url = String(p_request.provider_options.get("base_url", p_request.provider_options.get("url", base_url))).strip_edges();
	const String request_api_key = String(p_request.provider_options.get("api_key", api_key)).strip_edges();

	AIHTTPRequest request;
	request.method = HTTPClient::METHOD_POST;
	request.url = _chat_completions_url(request_base_url);
	request.body = _body_to_bytes(p_request);
	request.timeout_msec = int(p_request.provider_options.get("timeout_msec", timeout_msec));
	request.fail_on_http_error = true;
	request.label = "OpenAI-compatible chat completion";
	request.cancel_token = p_cancel_token;
	request.headers.push_back("Content-Type: application/json");
	request.headers.push_back("Accept: text/event-stream");
	if (!request_api_key.is_empty()) {
		request.headers.push_back("Authorization: Bearer " + request_api_key);
	}
	if (!organization.is_empty()) {
		request.headers.push_back("OpenAI-Organization: " + organization);
	}

	Ref<AIOpenAICompatibleStreamBridge> bridge;
	bridge.instantiate();
	bridge->setup(p_sink, p_cancel_token);

	AIHTTPResponse response;
	String http_error;
	if (!http_client->request_sse(request, callable_mp(bridge.ptr(), &AIOpenAICompatibleStreamBridge::handle_sse_event), response, http_error)) {
		r_error = AIError::make(p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested() ? AI_ERROR_INTERRUPTED : AI_ERROR_NETWORK, http_error);
		return false;
	}
	if (p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested()) {
		r_error = AIError::make(AI_ERROR_INTERRUPTED, p_cancel_token->get_cancel_message("OpenAI-compatible stream interrupted."));
		return false;
	}
	if (bridge->flush_pending_tool_calls()) {
		const String bridge_error = bridge->get_error();
		r_error = AIError::make(p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested() ? AI_ERROR_INTERRUPTED : (bridge_error.is_empty() ? AI_ERROR_CANCELLED : AI_ERROR_PROVIDER), bridge_error.is_empty() ? String("OpenAI-compatible stream stopped while flushing tool calls.") : bridge_error);
		return false;
	}

	const String bridge_error = bridge->get_error();
	if (!bridge_error.is_empty()) {
		r_error = AIError::make(AI_ERROR_PROVIDER, bridge_error);
		return false;
	}

	r_error = AIError::none();
	return true;
}

void AIOpenAICompatibleRuntime::set_base_url(const String &p_base_url) {
	base_url = p_base_url.strip_edges();
}

String AIOpenAICompatibleRuntime::get_base_url() const {
	return base_url;
}

void AIOpenAICompatibleRuntime::set_api_key(const String &p_api_key) {
	api_key = p_api_key;
}

String AIOpenAICompatibleRuntime::get_api_key() const {
	return api_key;
}
