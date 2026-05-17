/**************************************************************************/
/*  ai_agent_message.cpp                                                   */
/**************************************************************************/

#include "ai_agent_message.h"

#include "core/os/time.h"

Dictionary AIAgentMessage::to_dict() const {
	Dictionary dict;
	dict["role"] = role_to_string(role);
	dict["content"] = content;
	dict["metadata"] = metadata;
	dict["created_at"] = created_at;
	return dict;
}

AIAgentMessage AIAgentMessage::from_dict(const Dictionary &p_dict) {
	AIAgentMessage message;
	if (p_dict.has("role")) {
		message.role = string_to_role(p_dict["role"]);
	}
	if (p_dict.has("content") && Variant(p_dict["content"]).get_type() != Variant::NIL) {
		message.content = p_dict["content"];
	}
	if (p_dict.has("metadata") && Variant(p_dict["metadata"]).get_type() == Variant::DICTIONARY) {
		message.metadata = p_dict["metadata"];
	}
	if (p_dict.has("created_at")) {
		message.created_at = p_dict["created_at"];
	} else {
		message.created_at = Time::get_singleton()->get_unix_time_from_system();
	}
	return message;
}

String AIAgentMessage::role_to_string(AIAgentRole p_role) {
	switch (p_role) {
		case AI_AGENT_ROLE_SYSTEM:
			return "system";
		case AI_AGENT_ROLE_USER:
			return "user";
		case AI_AGENT_ROLE_ASSISTANT:
			return "assistant";
		case AI_AGENT_ROLE_TOOL:
			return "tool";
		case AI_AGENT_ROLE_CONTEXT:
			return "context";
		case AI_AGENT_ROLE_ERROR:
			return "error";
	}
	return "user";
}

AIAgentRole AIAgentMessage::string_to_role(const String &p_role) {
	if (p_role == "system") {
		return AI_AGENT_ROLE_SYSTEM;
	}
	if (p_role == "assistant") {
		return AI_AGENT_ROLE_ASSISTANT;
	}
	if (p_role == "tool") {
		return AI_AGENT_ROLE_TOOL;
	}
	if (p_role == "context") {
		return AI_AGENT_ROLE_CONTEXT;
	}
	if (p_role == "error") {
		return AI_AGENT_ROLE_ERROR;
	}
	return AI_AGENT_ROLE_USER;
}
