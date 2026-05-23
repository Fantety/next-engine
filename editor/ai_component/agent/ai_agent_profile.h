/**************************************************************************/
/*  ai_agent_profile.h                                                    */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/hash_set.h"

struct AIAgentProfile {
	String id;
	String display_name;
	HashSet<String> allowed_tools;
	HashSet<String> ask_tools;

	bool allows_tool(const String &p_tool_name) const;
	bool asks_for_tool(const String &p_tool_name) const;

	static AIAgentProfile get_plan_profile();
	static AIAgentProfile get_build_profile();
	static AIAgentProfile get_review_profile();
	static AIAgentProfile get_write_profile();
};
