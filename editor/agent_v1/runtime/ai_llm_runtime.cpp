/**************************************************************************/
/*  ai_llm_runtime.cpp                                                    */
/**************************************************************************/

#include "ai_llm_runtime.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

static Vector<AIModelPart> _ai_runtime_parts_from_array(const Array &p_array) {
	Vector<AIModelPart> result;
	for (int i = 0; i < p_array.size(); i++) {
		if (p_array[i].get_type() == Variant::STRING) {
			result.push_back(AIModelPart::text_part(String(p_array[i])));
			continue;
		}
		if (p_array[i].get_type() != Variant::DICTIONARY) {
			continue;
		}

		const Dictionary part_dict = p_array[i];
		const String type = String(part_dict.get("type", "text")).strip_edges().to_lower();
		AIModelPart part;
		if (type == "image") {
			part.type = AI_MODEL_PART_IMAGE;
		} else if (type == "audio") {
			part.type = AI_MODEL_PART_AUDIO;
		} else if (type == "file") {
			part.type = AI_MODEL_PART_FILE;
		} else {
			part.type = AI_MODEL_PART_TEXT;
		}
		part.text = part_dict.get("text", String());
		part.mime = part_dict.get("mime", String());
		part.name = part_dict.get("name", String());
		part.data = part_dict.get("data", String());
		if (part_dict.get("metadata", Variant()).get_type() == Variant::DICTIONARY) {
			part.metadata = Dictionary(part_dict["metadata"]).duplicate(true);
		}
		result.push_back(part);
	}
	return result;
}

void AILLMRuntime::_bind_methods() {
	ClassDB::bind_method(D_METHOD("configure_runtime", "config"), &AILLMRuntime::configure_runtime);
	ClassDB::bind_method(D_METHOD("stream", "request", "event_callback"), &AILLMRuntime::stream, DEFVAL(Callable()));
}

AIModelRequest AILLMRuntime::request_from_dictionary(const Dictionary &p_dict) {
	AIModelRequest request;
	request.request_id = p_dict.get("request_id", p_dict.get("requestID", String()));
	request.provider = p_dict.get("provider", String());
	request.model = p_dict.get("model", String());
	request.max_output_tokens = int(p_dict.get("max_output_tokens", p_dict.get("maxOutputTokens", 0)));
	request.stream = bool(p_dict.get("stream", true));
	if (p_dict.get("provider_options", Variant()).get_type() == Variant::DICTIONARY) {
		request.provider_options = Dictionary(p_dict["provider_options"]).duplicate(true);
	}
	if (p_dict.get("metadata", Variant()).get_type() == Variant::DICTIONARY) {
		request.metadata = Dictionary(p_dict["metadata"]).duplicate(true);
	}

	if (p_dict.get("system", Variant()).get_type() == Variant::ARRAY) {
		request.system = _ai_runtime_parts_from_array(p_dict["system"]);
	}

	if (p_dict.get("messages", Variant()).get_type() == Variant::ARRAY) {
		const Array messages = p_dict["messages"];
		for (int i = 0; i < messages.size(); i++) {
			if (messages[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			const Dictionary message_dict = messages[i];
			AIModelMessage message;
			message.id = message_dict.get("id", String());
			message.role = message_dict.get("role", String());
			message.tool_call_id = message_dict.get("tool_call_id", message_dict.get("toolCallID", String()));
			message.name = message_dict.get("name", String());
			if (message_dict.get("parts", Variant()).get_type() == Variant::ARRAY) {
				message.parts = _ai_runtime_parts_from_array(message_dict["parts"]);
			}
			if (message_dict.get("tool_calls", Variant()).get_type() == Variant::ARRAY) {
				const Array tool_calls = message_dict["tool_calls"];
				for (int j = 0; j < tool_calls.size(); j++) {
					if (tool_calls[j].get_type() == Variant::DICTIONARY) {
						message.tool_calls.push_back(AIModelToolCall::from_dictionary(tool_calls[j]));
					}
				}
			} else if (message_dict.get("toolCalls", Variant()).get_type() == Variant::ARRAY) {
				const Array tool_calls = message_dict["toolCalls"];
				for (int j = 0; j < tool_calls.size(); j++) {
					if (tool_calls[j].get_type() == Variant::DICTIONARY) {
						message.tool_calls.push_back(AIModelToolCall::from_dictionary(tool_calls[j]));
					}
				}
			}
			if (message_dict.get("metadata", Variant()).get_type() == Variant::DICTIONARY) {
				message.metadata = Dictionary(message_dict["metadata"]).duplicate(true);
			}
			request.messages.push_back(message);
		}
	}

	if (p_dict.get("tools", Variant()).get_type() == Variant::ARRAY) {
		const Array tools = p_dict["tools"];
		for (int i = 0; i < tools.size(); i++) {
			if (tools[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			const Dictionary tool_dict = tools[i];
			AIModelToolDefinition tool;
			tool.name = tool_dict.get("name", String());
			tool.description = tool_dict.get("description", String());
			if (tool_dict.get("input_schema", Variant()).get_type() == Variant::DICTIONARY) {
				tool.input_schema = Dictionary(tool_dict["input_schema"]).duplicate(true);
			}
			if (tool_dict.get("metadata", Variant()).get_type() == Variant::DICTIONARY) {
				tool.metadata = Dictionary(tool_dict["metadata"]).duplicate(true);
			}
			request.tools.push_back(tool);
		}
	}
	return request;
}

String AILLMRuntime::get_runtime_type() const {
	return "base";
}

bool AILLMRuntime::configure(const Dictionary &p_config, AIError &r_error) {
	(void)p_config;
	r_error = AIError::none();
	return true;
}

bool AILLMRuntime::stream_struct(const AIModelRequest &p_request, const Ref<AIStreamSink> &p_sink, const Ref<AICancelToken> &p_cancel_token, AIError &r_error) {
	(void)p_request;
	(void)p_sink;
	(void)p_cancel_token;
	r_error = AIError::make(AI_ERROR_UNAVAILABLE, "LLM runtime does not implement streaming.");
	return false;
}

Dictionary AILLMRuntime::configure_runtime(const Dictionary &p_config) {
	AIError error;
	if (!configure(p_config, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}
	Dictionary result;
	result["success"] = true;
	return result;
}

Dictionary AILLMRuntime::stream(const Dictionary &p_request, const Callable &p_event_callback) {
	Ref<AICallableStreamSink> sink;
	sink.instantiate();
	sink->set_callback(p_event_callback);

	Ref<AICancelToken> cancel_token;
	cancel_token.instantiate();

	AIError error;
	if (!stream_struct(request_from_dictionary(p_request), sink, cancel_token, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}
	Dictionary result;
	result["success"] = true;
	return result;
}
