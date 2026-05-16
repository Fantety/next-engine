/**************************************************************************/
/*  ai_tool_call.h                                                        */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

enum AIToolCallStatus {
	AI_TOOL_CALL_STATUS_PENDING,
	AI_TOOL_CALL_STATUS_RUNNING,
	AI_TOOL_CALL_STATUS_COMPLETED,
	AI_TOOL_CALL_STATUS_DENIED,
	AI_TOOL_CALL_STATUS_FAILED,
};

struct AIToolCall {
	String id;
	String tool_name;
	Dictionary arguments;
	AIToolCallStatus status = AI_TOOL_CALL_STATUS_PENDING;
	uint64_t created_at = 0;
	uint64_t updated_at = 0;

	Dictionary to_dict() const;
	static AIToolCall from_dict(const Dictionary &p_dict);
	static String status_to_string(AIToolCallStatus p_status);
	static AIToolCallStatus string_to_status(const String &p_status);
};
