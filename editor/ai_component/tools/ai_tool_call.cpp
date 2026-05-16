/**************************************************************************/
/*  ai_tool_call.cpp                                                      */
/**************************************************************************/

#include "ai_tool_call.h"

#include "core/variant/variant.h"

Dictionary AIToolCall::to_dict() const {
	Dictionary dict;
	dict["id"] = id;
	dict["tool_name"] = tool_name;
	dict["arguments"] = arguments;
	dict["status"] = status_to_string(status);
	dict["created_at"] = created_at;
	dict["updated_at"] = updated_at;
	return dict;
}

AIToolCall AIToolCall::from_dict(const Dictionary &p_dict) {
	AIToolCall call;
	call.id = p_dict.get("id", String());
	call.tool_name = p_dict.get("tool_name", String());
	if (p_dict.has("arguments") && Variant(p_dict["arguments"]).get_type() == Variant::DICTIONARY) {
		call.arguments = p_dict["arguments"];
	}
	call.status = string_to_status(p_dict.get("status", "pending"));
	call.created_at = p_dict.get("created_at", 0);
	call.updated_at = p_dict.get("updated_at", 0);
	return call;
}

String AIToolCall::status_to_string(AIToolCallStatus p_status) {
	switch (p_status) {
		case AI_TOOL_CALL_STATUS_PENDING:
			return "pending";
		case AI_TOOL_CALL_STATUS_RUNNING:
			return "running";
		case AI_TOOL_CALL_STATUS_COMPLETED:
			return "completed";
		case AI_TOOL_CALL_STATUS_DENIED:
			return "denied";
		case AI_TOOL_CALL_STATUS_FAILED:
			return "failed";
	}
	return "pending";
}

AIToolCallStatus AIToolCall::string_to_status(const String &p_status) {
	if (p_status == "running") {
		return AI_TOOL_CALL_STATUS_RUNNING;
	}
	if (p_status == "completed") {
		return AI_TOOL_CALL_STATUS_COMPLETED;
	}
	if (p_status == "denied") {
		return AI_TOOL_CALL_STATUS_DENIED;
	}
	if (p_status == "failed") {
		return AI_TOOL_CALL_STATUS_FAILED;
	}
	return AI_TOOL_CALL_STATUS_PENDING;
}
