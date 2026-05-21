/**************************************************************************/
/*  ai_openai_compatible_codec.cpp                                        */
/**************************************************************************/

#include "ai_openai_compatible_codec.h"

#include "core/io/json.h"

#include "editor/ai_component/agent/ai_agent_runtime.h"

void AIOpenAICompatibleStreamAccumulator::_ensure_tool_call_index(int p_index) {
	while (tool_calls.size() <= p_index) {
		tool_calls.push_back(AIOpenAIStreamToolCallState());
	}
}

bool AIOpenAICompatibleStreamAccumulator::apply_event(const String &p_event, AIOpenAIStreamParseResult &r_result) {
	r_result = AIOpenAIStreamParseResult();
	const String event = p_event.strip_edges();
	if (event.is_empty()) {
		return true;
	}
	if (event == "[DONE]") {
		r_result.done = true;
		String final_error;
		r_result.response = get_response(final_error);
		if (!final_error.is_empty()) {
			r_result.error = final_error;
			return false;
		}
		return true;
	}

	Ref<JSON> parser;
	parser.instantiate();
	Error err = parser->parse(event);
	if (err != OK) {
		r_result.error = "Failed to parse provider stream.";
		return false;
	}

	Variant data = parser->get_data();
	if (data.get_type() != Variant::DICTIONARY) {
		return true;
	}

	Dictionary dict = data;
	if (dict.has("error")) {
		Variant error_data = dict["error"];
		if (error_data.get_type() == Variant::DICTIONARY) {
			Dictionary error_dict = error_data;
			if (error_dict.has("message")) {
				r_result.error = error_dict["message"];
			}
		}
		if (r_result.error.is_empty()) {
			r_result.error = "Provider returned an error.";
		}
		return false;
	}

	if (dict.has("usage") && Variant(dict["usage"]).get_type() == Variant::DICTIONARY) {
		Dictionary provider_usage = dict["usage"];
		Dictionary usage;
		usage["prompt_tokens"] = (int)provider_usage.get("prompt_tokens", provider_usage.get("input_tokens", 0));
		usage["completion_tokens"] = (int)provider_usage.get("completion_tokens", provider_usage.get("output_tokens", 0));
		usage["total_tokens"] = (int)provider_usage.get("total_tokens", (int)usage["prompt_tokens"] + (int)usage["completion_tokens"]);
		usage["source"] = "provider";
		metadata["usage"] = usage;
	}

	if (!dict.has("choices") || Variant(dict["choices"]).get_type() != Variant::ARRAY) {
		return true;
	}

	Array choices = dict["choices"];
	if (choices.is_empty() || Variant(choices[0]).get_type() != Variant::DICTIONARY) {
		return true;
	}

	Dictionary choice = choices[0];
	if (choice.has("finish_reason") && Variant(choice["finish_reason"]).get_type() != Variant::NIL) {
		finish_reason = String(choice["finish_reason"]);
		if (!finish_reason.is_empty()) {
			metadata["finish_reason"] = finish_reason;
		}
	}
	if (!choice.has("delta") || Variant(choice["delta"]).get_type() != Variant::DICTIONARY) {
		r_result.done = !finish_reason.is_empty();
		if (r_result.done) {
			r_result.response = get_response(r_result.error);
		} else {
			r_result.response.content = content;
			r_result.response.metadata = metadata;
		}
		return r_result.error.is_empty();
	}

	Dictionary delta = choice["delta"];
	if (delta.has("content") && Variant(delta["content"]).get_type() != Variant::NIL) {
		content += String(delta["content"]);
		r_result.has_delta = true;
	}
	if (delta.has("reasoning_content") && Variant(delta["reasoning_content"]).get_type() != Variant::NIL) {
		reasoning_content += String(delta["reasoning_content"]);
		metadata["reasoning_content"] = reasoning_content;
		r_result.has_delta = true;
	}
	if (delta.has("tool_calls") && Variant(delta["tool_calls"]).get_type() == Variant::ARRAY) {
		Array delta_tool_calls = delta["tool_calls"];
		for (int i = 0; i < delta_tool_calls.size(); i++) {
			if (Variant(delta_tool_calls[i]).get_type() != Variant::DICTIONARY) {
				continue;
			}

			Dictionary delta_tool_call = delta_tool_calls[i];
			const int tool_call_index = (int)delta_tool_call.get("index", i);
			_ensure_tool_call_index(tool_call_index);

			AIOpenAIStreamToolCallState &tool_call = tool_calls.write[tool_call_index];
			if (delta_tool_call.has("id") && Variant(delta_tool_call["id"]).get_type() != Variant::NIL) {
				tool_call.id += String(delta_tool_call["id"]);
			}
			if (delta_tool_call.has("function") && Variant(delta_tool_call["function"]).get_type() == Variant::DICTIONARY) {
				Dictionary function = delta_tool_call["function"];
				if (function.has("name") && Variant(function["name"]).get_type() != Variant::NIL) {
					tool_call.tool_name += String(function["name"]);
				}
				if (function.has("arguments") && Variant(function["arguments"]).get_type() != Variant::NIL) {
					tool_call.arguments_json += String(function["arguments"]);
				}
			}
			r_result.has_delta = true;
		}
	}

	r_result.done = !finish_reason.is_empty();
	if (r_result.done) {
		r_result.response = get_response(r_result.error);
	} else {
		r_result.response.content = content;
		r_result.response.metadata = metadata;
	}
	return r_result.error.is_empty();
}

AIAgentRuntimeResponse AIOpenAICompatibleStreamAccumulator::get_response(String &r_error) const {
	AIAgentRuntimeResponse response;
	r_error = String();
	response.content = content;
	response.metadata = metadata;
	if (!reasoning_content.is_empty()) {
		response.metadata["reasoning_content"] = reasoning_content;
	}
	if (!finish_reason.is_empty()) {
		response.metadata["finish_reason"] = finish_reason;
	}

	for (int i = 0; i < tool_calls.size(); i++) {
		const AIOpenAIStreamToolCallState &tool_call = tool_calls[i];
		if (tool_call.tool_name.is_empty()) {
			continue;
		}

		AIToolCall call;
		call.id = tool_call.id;
		call.tool_name = tool_call.tool_name;
		if (!tool_call.arguments_json.strip_edges().is_empty()) {
			Ref<JSON> arguments_parser;
			arguments_parser.instantiate();
			Error arguments_err = arguments_parser->parse(tool_call.arguments_json);
			if (arguments_err != OK || arguments_parser->get_data().get_type() != Variant::DICTIONARY) {
				r_error = "Failed to parse provider tool arguments.";
				return AIAgentRuntimeResponse();
			}
			call.arguments = arguments_parser->get_data();
		}
		response.tool_calls.push_back(call);
	}

	return response;
}

String AIOpenAICompatibleCodec::build_request_path(const String &p_base_path) {
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

PackedByteArray AIOpenAICompatibleCodec::build_body(const Array &p_messages, const String &p_model, const Array &p_tool_schemas, bool p_stream) {
	return _build_body(p_messages, p_model, p_tool_schemas, p_stream);
}

PackedByteArray AIOpenAICompatibleCodec::_build_body(const Array &p_messages, const String &p_model, const Array &p_tool_schemas, bool p_stream) {
	Dictionary body;
	body["model"] = p_model;
	body["stream"] = p_stream;
	body["messages"] = p_messages;
	if (!p_tool_schemas.is_empty()) {
		body["tools"] = p_tool_schemas;
	}
	return JSON::stringify(body).to_utf8_buffer();
}

bool AIOpenAICompatibleCodec::parse_chat_completion(const String &p_response_text, AIAgentRuntimeResponse &r_response, String &r_error) {
	return _parse_chat_completion(p_response_text, r_response, r_error);
}

bool AIOpenAICompatibleCodec::_parse_chat_completion(const String &p_response_text, AIAgentRuntimeResponse &r_response, String &r_error) {
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

	if (dict.has("usage") && Variant(dict["usage"]).get_type() == Variant::DICTIONARY) {
		Dictionary provider_usage = dict["usage"];
		Dictionary usage;
		usage["prompt_tokens"] = (int)provider_usage.get("prompt_tokens", provider_usage.get("input_tokens", 0));
		usage["completion_tokens"] = (int)provider_usage.get("completion_tokens", provider_usage.get("output_tokens", 0));
		usage["total_tokens"] = (int)provider_usage.get("total_tokens", (int)usage["prompt_tokens"] + (int)usage["completion_tokens"]);
		usage["source"] = "provider";
		r_response.metadata["usage"] = usage;
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
	if (message.has("reasoning_content") && Variant(message["reasoning_content"]).get_type() != Variant::NIL) {
		r_response.metadata["reasoning_content"] = String(message["reasoning_content"]);
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

bool AIOpenAICompatibleCodec::extract_delta(const String &p_event, String &r_delta, String &r_finish_reason, String &r_error) {
	return _extract_delta(p_event, r_delta, r_finish_reason, r_error);
}

bool AIOpenAICompatibleCodec::_extract_delta(const String &p_event, String &r_delta, String &r_finish_reason, String &r_error) {
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

	if (!dict.has("choices") || Variant(dict["choices"]).get_type() != Variant::ARRAY) {
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
	if (!choice.has("delta") || Variant(choice["delta"]).get_type() != Variant::DICTIONARY) {
		return true;
	}

	Dictionary delta_dict = choice["delta"];
	if (delta_dict.has("content") && Variant(delta_dict["content"]).get_type() != Variant::NIL) {
		r_delta = String(delta_dict["content"]);
	}
	return true;
}
