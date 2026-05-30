/**************************************************************************/
/*  ai_tool_permission.cpp                                                */
/**************************************************************************/

#include "ai_tool_permission.h"

AIToolPermissionResult AIToolPermissionPolicy::evaluate(AIToolPermission p_permission, const String &p_tool_name, const String &p_reason) {
	AIToolPermissionResult result;
	result.permission = p_permission;
	if (!p_reason.is_empty()) {
		result.reason = p_reason;
		return result;
	}

	switch (p_permission) {
		case AI_TOOL_PERMISSION_ALLOW:
			return result;
		case AI_TOOL_PERMISSION_ASK:
			result.reason = "Tool requires explicit user approval.";
			return result;
		case AI_TOOL_PERMISSION_DENY:
			result.reason = "Tool is not allowed by the active agent.";
			if (!p_tool_name.is_empty()) {
				result.reason += " Tool: " + p_tool_name + ".";
			}
			return result;
	}

	result.permission = AI_TOOL_PERMISSION_DENY;
	result.reason = "Tool is not allowed by the active agent.";
	return result;
}

String AIToolPermissionPolicy::permission_to_string(AIToolPermission p_permission) {
	switch (p_permission) {
		case AI_TOOL_PERMISSION_ALLOW:
			return "allow";
		case AI_TOOL_PERMISSION_ASK:
			return "ask";
		case AI_TOOL_PERMISSION_DENY:
			return "deny";
	}
	return "deny";
}

AIToolPermission AIToolPermissionPolicy::string_to_permission(const String &p_permission) {
	if (p_permission == "allow") {
		return AI_TOOL_PERMISSION_ALLOW;
	}
	if (p_permission == "ask") {
		return AI_TOOL_PERMISSION_ASK;
	}
	return AI_TOOL_PERMISSION_DENY;
}
