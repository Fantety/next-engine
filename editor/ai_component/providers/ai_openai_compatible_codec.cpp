/**************************************************************************/
/*  ai_openai_compatible_codec.cpp                                        */
/**************************************************************************/

#include "ai_openai_compatible_codec.h"

#include "core/io/json.h"

#include "editor/ai_component/agent/ai_agent_runtime.h"

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
