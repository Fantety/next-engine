/**************************************************************************/
/*  ai_model_request.cpp                                                  */
/**************************************************************************/

#include "ai_model_request.h"

#include "editor/agent_v1/core/base/ai_id.h"

#include "core/io/json.h"
#include "core/variant/variant.h"

static Variant _ai_model_tool_call_input_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::STRING) {
		const String text = String(p_value).strip_edges();
		if (text.is_empty()) {
			return Dictionary();
		}

		Ref<JSON> json;
		json.instantiate();
		if (json->parse(text) == OK) {
			return json->get_data();
		}
	}
	if (p_value.get_type() == Variant::DICTIONARY) {
		return Dictionary(p_value).duplicate(true);
	}
	if (p_value.get_type() == Variant::ARRAY) {
		return Array(p_value).duplicate(true);
	}
	return p_value;
}

Dictionary AIModelPart::to_dictionary() const {
	Dictionary result;
	result["type"] = type_to_string(type);
	result["text"] = text;
	result["mime"] = mime;
	result["name"] = name;
	result["data"] = data;
	result["metadata"] = metadata;
	return result;
}

String AIModelPart::type_to_string(AIModelPartType p_type) {
	switch (p_type) {
		case AI_MODEL_PART_TEXT:
			return "text";
		case AI_MODEL_PART_IMAGE:
			return "image";
		case AI_MODEL_PART_AUDIO:
			return "audio";
		case AI_MODEL_PART_FILE:
			return "file";
	}
	return "text";
}

AIModelPart AIModelPart::text_part(const String &p_text) {
	AIModelPart part;
	part.type = AI_MODEL_PART_TEXT;
	part.text = p_text;
	return part;
}

AIModelPart AIModelPart::data_part(AIModelPartType p_type, const String &p_mime, const String &p_data, const String &p_name) {
	AIModelPart part;
	part.type = p_type;
	part.mime = p_mime;
	part.data = p_data;
	part.name = p_name;
	return part;
}

bool AIModelToolCall::is_valid() const {
	return AIId::is_valid_name(name) && !id.strip_edges().is_empty();
}

Dictionary AIModelToolCall::to_dictionary() const {
	Dictionary result;
	result["id"] = id;
	result["name"] = name;
	result["input"] = input;
	result["provider_metadata"] = provider_metadata;
	return result;
}

AIModelToolCall AIModelToolCall::from_dictionary(const Dictionary &p_dict) {
	AIModelToolCall result;
	Dictionary function;
	if (p_dict.get("function", Variant()).get_type() == Variant::DICTIONARY) {
		function = Dictionary(p_dict["function"]);
	}
	const Variant raw_arguments = p_dict.has("arguments") ? p_dict["arguments"] : function.get("arguments", Variant());
	result.id = p_dict.get("id", p_dict.get("call_id", p_dict.get("callID", String())));
	result.name = p_dict.get("name", p_dict.get("tool", function.get("name", String())));
	result.input = _ai_model_tool_call_input_from_variant(p_dict.get("input", raw_arguments));
	if (p_dict.get("provider_metadata", Variant()).get_type() == Variant::DICTIONARY) {
		result.provider_metadata = Dictionary(p_dict["provider_metadata"]).duplicate(true);
	} else if (p_dict.get("providerMetadata", Variant()).get_type() == Variant::DICTIONARY) {
		result.provider_metadata = Dictionary(p_dict["providerMetadata"]).duplicate(true);
	}
	if (raw_arguments.get_type() == Variant::STRING && !result.provider_metadata.has("raw_arguments")) {
		result.provider_metadata["raw_arguments"] = String(raw_arguments);
	}
	return result;
}

Dictionary AIModelMessage::to_dictionary() const {
	Dictionary result;
	result["id"] = id;
	result["role"] = role;
	result["tool_call_id"] = tool_call_id;
	result["name"] = name;
	Array part_array;
	for (int i = 0; i < parts.size(); i++) {
		part_array.push_back(parts[i].to_dictionary());
	}
	result["parts"] = part_array;
	Array tool_call_array;
	for (int i = 0; i < tool_calls.size(); i++) {
		tool_call_array.push_back(tool_calls[i].to_dictionary());
	}
	result["tool_calls"] = tool_call_array;
	result["metadata"] = metadata;
	return result;
}

AIModelMessage AIModelMessage::text_message(const String &p_role, const String &p_text, const String &p_id) {
	AIModelMessage message;
	message.id = p_id;
	message.role = p_role;
	message.parts.push_back(AIModelPart::text_part(p_text));
	return message;
}

AIModelMessage AIModelMessage::tool_result_message(const String &p_tool_call_id, const String &p_name, const String &p_text, const String &p_id) {
	AIModelMessage message;
	message.id = p_id;
	message.role = "tool";
	message.tool_call_id = p_tool_call_id;
	message.name = p_name;
	message.parts.push_back(AIModelPart::text_part(p_text));
	return message;
}

bool AIModelToolDefinition::is_valid() const {
	return AIId::is_valid_name(name) && !description.is_empty();
}

Dictionary AIModelToolDefinition::to_dictionary() const {
	Dictionary result;
	result["name"] = name;
	result["description"] = description;
	result["input_schema"] = input_schema;
	result["metadata"] = metadata;
	return result;
}

Dictionary AIModelRequest::to_dictionary() const {
	Dictionary result;
	result["request_id"] = request_id;
	result["provider"] = provider;
	result["model"] = model;
	result["provider_options"] = provider_options;
	result["metadata"] = metadata;
	result["max_output_tokens"] = max_output_tokens;
	result["stream"] = stream;

	Array system_array;
	for (int i = 0; i < system.size(); i++) {
		system_array.push_back(system[i].to_dictionary());
	}
	result["system"] = system_array;

	Array message_array;
	for (int i = 0; i < messages.size(); i++) {
		message_array.push_back(messages[i].to_dictionary());
	}
	result["messages"] = message_array;

	Array tool_array;
	for (int i = 0; i < tools.size(); i++) {
		tool_array.push_back(tools[i].to_dictionary());
	}
	result["tools"] = tool_array;
	return result;
}
