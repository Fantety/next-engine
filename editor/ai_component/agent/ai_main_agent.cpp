/**************************************************************************/
/*  ai_main_agent.cpp                                                     */
/**************************************************************************/

#include "ai_main_agent.h"

#include "editor/ai_component/agent/ai_mcp_service.h"
#include "editor/ai_component/planning/ai_manage_plan_tool.h"
#include "editor/ai_component/skills/ai_activate_skill_tool.h"
#include "editor/ai_component/tools/editor/ai_get_editor_context_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_add_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_create_scene_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_delete_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_list_properties_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_move_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_open_scene_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_rename_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_save_current_scene_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_set_property_tool.h"
#include "editor/ai_component/tools/editor/ai_script_bind_to_node_tool.h"
#include "editor/ai_component/tools/editor/ai_script_create_tool.h"
#include "editor/ai_component/tools/editor/ai_script_delete_tool.h"
#include "editor/ai_component/tools/editor/ai_script_inspect_tool.h"
#include "editor/ai_component/tools/editor/ai_script_patch_function_tool.h"
#include "editor/ai_component/tools/editor/ai_script_unbind_from_node_tool.h"
#include "editor/ai_component/tools/editor/ai_script_write_tool.h"
#include "editor/ai_component/tools/editor/ai_shader_apply_to_node_tool.h"
#include "editor/ai_component/tools/editor/ai_shader_create_tool.h"
#include "editor/ai_component/tools/editor/ai_shader_delete_tool.h"
#include "editor/ai_component/tools/editor/ai_shader_edit_tool.h"
#include "editor/ai_component/tools/project/ai_create_folder_tool.h"
#include "editor/ai_component/tools/project/ai_list_project_tool.h"
#include "editor/ai_component/tools/project/ai_read_file_tool.h"
#include "editor/ai_component/tools/project/ai_search_project_tool.h"

namespace {

struct MainAgentToolFactory {
	Ref<AITool> (*create)();
};

template <typename T>
Ref<AITool> _create_main_agent_tool() {
	Ref<T> tool;
	tool.instantiate();
	return tool;
}

void _register_main_agent_tool(AIMainAgent *p_agent, const MainAgentToolFactory &p_factory, AIToolPermission p_permission) {
	Ref<AITool> tool = p_factory.create();
	ERR_FAIL_COND(tool.is_null());

	const String tool_name = tool->get_name();
	const bool registered = p_agent->add_tool(tool, p_permission);
	if (registered) {
		print_line(vformat("[AI Agent][MainAgent] Registered tool: %s", tool_name));
	}
}

template <int N>
void _register_main_agent_tool_group(AIMainAgent *p_agent, const MainAgentToolFactory (&p_factories)[N], AIToolPermission p_permission) {
	for (int i = 0; i < N; i++) {
		_register_main_agent_tool(p_agent, p_factories[i], p_permission);
	}
}

} // namespace

void AIMainAgent::_bind_methods() {
}

AIMainAgent::AIMainAgent() {
	reload_tools();
}

void AIMainAgent::_register_local_tools() {
	const MainAgentToolFactory read_tools[] = {
		{ _create_main_agent_tool<AIListProjectTool> },
		{ _create_main_agent_tool<AIReadFileTool> },
		{ _create_main_agent_tool<AISearchProjectTool> },
		{ _create_main_agent_tool<AIGetEditorContextTool> },
		{ _create_main_agent_tool<AIActivateSkillTool> },
		{ _create_main_agent_tool<AIManagePlanTool> },
		{ _create_main_agent_tool<AISceneListPropertiesTool> },
		{ _create_main_agent_tool<AIScriptInspectTool> },
	};
	const MainAgentToolFactory write_tools[] = {
		{ _create_main_agent_tool<AICreateFolderTool> },
		{ _create_main_agent_tool<AISceneCreateSceneTool> },
		{ _create_main_agent_tool<AISceneAddNodeTool> },
		{ _create_main_agent_tool<AISceneDeleteNodeTool> },
		{ _create_main_agent_tool<AISceneRenameNodeTool> },
		{ _create_main_agent_tool<AISceneMoveNodeTool> },
		{ _create_main_agent_tool<AISceneSetPropertyTool> },
		{ _create_main_agent_tool<AISceneSaveCurrentSceneTool> },
		{ _create_main_agent_tool<AISceneOpenSceneTool> },
		{ _create_main_agent_tool<AIScriptCreateTool> },
		{ _create_main_agent_tool<AIScriptWriteTool> },
		{ _create_main_agent_tool<AIScriptPatchFunctionTool> },
		{ _create_main_agent_tool<AIScriptBindToNodeTool> },
		{ _create_main_agent_tool<AIScriptUnbindFromNodeTool> },
		{ _create_main_agent_tool<AIShaderCreateTool> },
		{ _create_main_agent_tool<AIShaderEditTool> },
		{ _create_main_agent_tool<AIShaderApplyToNodeTool> },
	};
	const MainAgentToolFactory explicit_approval_tools[] = {
		{ _create_main_agent_tool<AIScriptDeleteTool> },
		{ _create_main_agent_tool<AIShaderDeleteTool> },
	};

	_register_main_agent_tool_group(this, read_tools, AI_TOOL_PERMISSION_ALLOW);
	_register_main_agent_tool_group(this, write_tools, AI_TOOL_PERMISSION_ALLOW);
	_register_main_agent_tool_group(this, explicit_approval_tools, AI_TOOL_PERMISSION_ASK);

	_apply_profile_denylist();
}

void AIMainAgent::_apply_profile_denylist() {
	Ref<AIToolRegistry> registry = get_tool_registry();
	if (registry.is_null()) {
		return;
	}

	// 当前 profile 禁用的工具强制设为 DENY
	// 未列出的工具保持原有权限不变
	const AIAgentProfile active_profile = get_profile();
	const Vector<String> tool_names = registry->get_tool_names();
	for (int i = 0; i < tool_names.size(); i++) {
		const String &tool_name = tool_names[i];
		if (active_profile.denies_tool(tool_name)) {
			registry->set_tool_permission(tool_name, AI_TOOL_PERMISSION_DENY, vformat("Tool `%s` is disabled in the `%s` mode.", tool_name, active_profile.display_name));
		}
	}
}

void AIMainAgent::_register_mcp_tools() {
	Ref<AIMCPService> mcp_service = AIMCPService::get_singleton();
	if (mcp_service.is_null()) {
		return;
	}
	mcp_service->register_discovered_tools(get_tool_registry(), AI_TOOL_PERMISSION_ASK);
}

void AIMainAgent::set_profile(const AIAgentProfile &p_profile) {
	AIAgentBase::set_profile(p_profile);
	reload_tools();
}

void AIMainAgent::set_agent_profile_id(const String &p_profile_id) {
	AIAgentBase::set_agent_profile_id(p_profile_id);
}
void AIMainAgent::reload_tools() {
	clear_tools();
	_register_local_tools();
	_register_mcp_tools();
}
