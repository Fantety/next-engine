/**************************************************************************/
/*  ai_agent_profile.cpp                                                  */
/**************************************************************************/

#include "ai_agent_profile.h"

static void _add_read_only_tools(AIAgentProfile &r_profile) {
	r_profile.allowed_tools.insert("project.list_tree");
	r_profile.allowed_tools.insert("project.read_file");
	r_profile.allowed_tools.insert("project.search_text");
	r_profile.allowed_tools.insert("editor.get_context");
}

bool AIAgentProfile::allows_tool(const String &p_tool_name) const {
	return allowed_tools.has(p_tool_name);
}

AIAgentProfile AIAgentProfile::get_plan_profile() {
	AIAgentProfile profile;
	profile.id = "plan";
	profile.display_name = "Plan";
	_add_read_only_tools(profile);
	return profile;
}

AIAgentProfile AIAgentProfile::get_build_profile() {
	AIAgentProfile profile;
	profile.id = "build";
	profile.display_name = "Build";
	_add_read_only_tools(profile);
	return profile;
}
