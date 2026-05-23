/**************************************************************************/
/*  ai_agent_profile.cpp                                                  */
/**************************************************************************/

#include "ai_agent_profile.h"

static void _add_read_only_tools(AIAgentProfile &r_profile) {
	r_profile.allowed_tools.insert("project.list_tree");
	r_profile.allowed_tools.insert("project.read_file");
	r_profile.allowed_tools.insert("project.search_text");
	r_profile.allowed_tools.insert("editor.get_context");
	r_profile.allowed_tools.insert("scene.list_properties");
}

static void _add_scene_write_tools(AIAgentProfile &r_profile) {
	r_profile.allowed_tools.insert("scene.create_scene");
	r_profile.allowed_tools.insert("scene.add_node");
	r_profile.allowed_tools.insert("scene.delete_node");
	r_profile.allowed_tools.insert("scene.rename_node");
	r_profile.allowed_tools.insert("scene.move_node");
	r_profile.allowed_tools.insert("scene.set_property");
	r_profile.allowed_tools.insert("scene.save_current_scene");
	r_profile.allowed_tools.insert("scene.open_scene");
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

AIAgentProfile AIAgentProfile::get_write_profile() {
	AIAgentProfile profile;
	profile.id = "write";
	profile.display_name = "Write";
	_add_read_only_tools(profile);
	_add_scene_write_tools(profile);
	profile.allowed_tools.insert("project.create_folder");
	return profile;
}

AIAgentProfile AIAgentProfile::get_build_profile() {
	AIAgentProfile profile;
	profile.id = "build";
	profile.display_name = "Build";
	_add_read_only_tools(profile);
	return profile;
}
