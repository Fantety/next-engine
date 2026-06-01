/**************************************************************************/
/*  ai_agent_profile.cpp                                                  */
/**************************************************************************/

#include "ai_agent_profile.h"

// 受限 profile 禁用所有会修改项目/场景/脚本/着色器状态的工具
static void _deny_write_tools(AIAgentProfile &r_profile) {
	r_profile.denied_tools.insert("project.create_folder");
	r_profile.denied_tools.insert("scene.create_scene");
	r_profile.denied_tools.insert("scene.add_node");
	r_profile.denied_tools.insert("scene.instantiate_scene");
	r_profile.denied_tools.insert("scene.delete_node");
	r_profile.denied_tools.insert("scene.rename_node");
	r_profile.denied_tools.insert("scene.move_node");
	r_profile.denied_tools.insert("scene.set_property");
	r_profile.denied_tools.insert("scene.save_current_scene");
	r_profile.denied_tools.insert("scene.open_scene");
	r_profile.denied_tools.insert("script.create");
	r_profile.denied_tools.insert("script.write");
	r_profile.denied_tools.insert("script.patch_function");
	r_profile.denied_tools.insert("script.bind_to_node");
	r_profile.denied_tools.insert("script.unbind_from_node");
	r_profile.denied_tools.insert("script.delete");
	r_profile.denied_tools.insert("shader.create");
	r_profile.denied_tools.insert("shader.edit");
	r_profile.denied_tools.insert("shader.apply_to_node");
	r_profile.denied_tools.insert("shader.delete");
}

bool AIAgentProfile::denies_tool(const String &p_tool_name) const {
	return denied_tools.has(p_tool_name);
}

AIAgentProfile AIAgentProfile::get_ask_profile() {
	AIAgentProfile profile;
	profile.id = "ask";
	profile.display_name = "Ask";
	// 只读：禁用所有状态修改工具。
	_deny_write_tools(profile);
	profile.review_changes = false;
	return profile;
}

AIAgentProfile AIAgentProfile::get_auto_profile() {
	AIAgentProfile profile;
	profile.id = "auto";
	profile.display_name = "Auto";
	// 宽松模式：不禁用任何工具。写入效果暂存到审查存储中。
	profile.review_changes = true;
	return profile;
}
