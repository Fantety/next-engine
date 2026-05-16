/**************************************************************************/
/*  ai_tool_permission.h                                                  */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

#include "editor/ai_component/agent/ai_agent_profile.h"

enum AIToolPermissionDecision {
	AI_TOOL_PERMISSION_ALLOW,
	AI_TOOL_PERMISSION_ASK,
	AI_TOOL_PERMISSION_DENY,
};

struct AIToolPermissionResult {
	AIToolPermissionDecision decision = AI_TOOL_PERMISSION_DENY;
	String reason;
};

class AIToolPermissionPolicy {
public:
	static AIToolPermissionResult evaluate(const AIAgentProfile &p_profile, const String &p_tool_name, const Dictionary &p_arguments);
	static String decision_to_string(AIToolPermissionDecision p_decision);
	static AIToolPermissionDecision string_to_decision(const String &p_decision);
};
