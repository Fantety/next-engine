/**************************************************************************/
/*  ai_tool_permission.cpp                                                */
/**************************************************************************/

#include "ai_tool_permission.h"

AIToolPermissionResult AIToolPermissionPolicy::evaluate(const AIAgentProfile &p_profile, const String &p_tool_name, const Dictionary &p_arguments) {
	AIToolPermissionResult result;
	if (p_profile.asks_for_tool(p_tool_name)) {
		result.decision = AI_TOOL_PERMISSION_ASK;
		result.reason = "Tool requires explicit user approval.";
		return result;
	}
	if (p_profile.allows_tool(p_tool_name)) {
		result.decision = AI_TOOL_PERMISSION_ALLOW;
		return result;
	}

	result.decision = AI_TOOL_PERMISSION_DENY;
	result.reason = "Tool is not allowed by the active agent profile.";
	return result;
}

String AIToolPermissionPolicy::decision_to_string(AIToolPermissionDecision p_decision) {
	switch (p_decision) {
		case AI_TOOL_PERMISSION_ALLOW:
			return "allow";
		case AI_TOOL_PERMISSION_ASK:
			return "ask";
		case AI_TOOL_PERMISSION_DENY:
			return "deny";
	}
	return "deny";
}

AIToolPermissionDecision AIToolPermissionPolicy::string_to_decision(const String &p_decision) {
	if (p_decision == "allow") {
		return AI_TOOL_PERMISSION_ALLOW;
	}
	if (p_decision == "ask") {
		return AI_TOOL_PERMISSION_ASK;
	}
	return AI_TOOL_PERMISSION_DENY;
}
