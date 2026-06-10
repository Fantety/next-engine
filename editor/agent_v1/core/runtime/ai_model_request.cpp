/**************************************************************************/
/*  ai_model_request.cpp                                                  */
/**************************************************************************/

#include "ai_model_request.h"

#include "editor/agent_v1/core/base/ai_id.h"

#include "core/variant/variant.h"

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

Dictionary AIModelMessage::to_dictionary() const {
	Dictionary result;
	result["id"] = id;
	result["role"] = role;
	Array part_array;
	for (int i = 0; i < parts.size(); i++) {
		part_array.push_back(parts[i].to_dictionary());
	}
	result["parts"] = part_array;
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
