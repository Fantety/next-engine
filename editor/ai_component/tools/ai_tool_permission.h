/**************************************************************************/
/*  ai_tool_permission.h                                                  */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"

enum AIToolPermission {
	AI_TOOL_PERMISSION_ALLOW,
	AI_TOOL_PERMISSION_ASK,
	AI_TOOL_PERMISSION_DENY,
};

struct AIToolPermissionResult {
	AIToolPermission permission = AI_TOOL_PERMISSION_DENY;
	String reason;
};

class AIToolPermissionPolicy {
public:
	static AIToolPermissionResult evaluate(AIToolPermission p_permission, const String &p_tool_name, const String &p_reason = String());
	static String permission_to_string(AIToolPermission p_permission);
	static AIToolPermission string_to_permission(const String &p_permission);
};
