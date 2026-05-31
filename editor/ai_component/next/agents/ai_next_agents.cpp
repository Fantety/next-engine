/**************************************************************************/
/*  ai_next_agents.cpp                                                    */
/**************************************************************************/

#include "ai_next_agents.h"

#include "editor/ai_component/next/ai_next_manage_project_tool.h"
#include "editor/ai_component/next/ai_next_prompts.h"
#include "editor/ai_component/tools/editor/ai_get_editor_context_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_add_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_create_scene_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_delete_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_instantiate_scene_tool.h"
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

template <typename T>
Ref<AITool> _make_tool() {
	Ref<T> tool;
	tool.instantiate();
	return tool;
}

void _configure_next_agent(AIAgentBase *p_agent, const String &p_agent_id, const String &p_prompt, const String &p_profile_id) {
	if (!p_agent) {
		return;
	}
	p_agent->set_session_id("next:" + p_agent_id);
	p_agent->set_system_prompt(p_prompt);
	p_agent->set_agent_profile_id(p_profile_id);
}

void _register_shared_read_tools(AIAgentBase *p_agent) {
	if (!p_agent) {
		return;
	}
	p_agent->add_tool(_make_tool<AIListProjectTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AIReadFileTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AISearchProjectTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AIGetEditorContextTool>(), AI_TOOL_PERMISSION_ALLOW);
}

void _register_next_tools(AIAgentBase *p_agent, const Ref<AINextProjectState> &p_project_state) {
	if (!p_agent || p_project_state.is_null()) {
		return;
	}

	Ref<AINextManageProjectTool> manage_project_tool;
	manage_project_tool.instantiate();
	manage_project_tool->set_project_state(p_project_state);
	p_agent->add_tool(manage_project_tool, AI_TOOL_PERMISSION_ALLOW);
}

void _register_script_tools(AIAgentBase *p_agent) {
	if (!p_agent) {
		return;
	}
	p_agent->add_tool(_make_tool<AICreateFolderTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AIScriptInspectTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AIScriptCreateTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AIScriptWriteTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AIScriptPatchFunctionTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AIScriptBindToNodeTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AIScriptUnbindFromNodeTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AIScriptDeleteTool>(), AI_TOOL_PERMISSION_ASK);
}

void _register_scene_tools(AIAgentBase *p_agent) {
	if (!p_agent) {
		return;
	}
	p_agent->add_tool(_make_tool<AISceneListPropertiesTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AISceneCreateSceneTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AISceneAddNodeTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AISceneInstantiateSceneTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AISceneRenameNodeTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AISceneMoveNodeTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AISceneSetPropertyTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AISceneSaveCurrentSceneTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AISceneOpenSceneTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AISceneDeleteNodeTool>(), AI_TOOL_PERMISSION_ASK);
}

void _register_shader_tools(AIAgentBase *p_agent) {
	if (!p_agent) {
		return;
	}
	p_agent->add_tool(_make_tool<AICreateFolderTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AIShaderCreateTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AIShaderEditTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AIShaderApplyToNodeTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AIShaderDeleteTool>(), AI_TOOL_PERMISSION_ASK);
}

} // namespace

void AINextPlanningAgent::_bind_methods() {
}

AINextPlanningAgent::AINextPlanningAgent() {
	_configure_next_agent(this, "planning_agent", AINextPrompts::get_planning_prompt(), "plan");
	_rebuild_tools();
}

void AINextPlanningAgent::_rebuild_tools() {
	clear_tools();
	_register_next_tools(this, project_state);
	_register_shared_read_tools(this);
}

void AINextPlanningAgent::set_project_state(const Ref<AINextProjectState> &p_project_state) {
	project_state = p_project_state;
	_rebuild_tools();
}

Ref<AINextProjectState> AINextPlanningAgent::get_project_state() const {
	return project_state;
}

void AINextScriptAgent::_bind_methods() {
}

AINextScriptAgent::AINextScriptAgent() {
	_configure_next_agent(this, "script_agent", AINextPrompts::get_script_prompt(), "write");
	_register_shared_read_tools(this);
	_register_script_tools(this);
}

void AINextSceneAgent::_bind_methods() {
}

AINextSceneAgent::AINextSceneAgent() {
	_configure_next_agent(this, "scene_agent", AINextPrompts::get_scene_prompt(), "write");
	_register_shared_read_tools(this);
	_register_scene_tools(this);
}

void AINextShaderAgent::_bind_methods() {
}

AINextShaderAgent::AINextShaderAgent() {
	_configure_next_agent(this, "shader_agent", AINextPrompts::get_shader_prompt(), "write");
	_register_shared_read_tools(this);
	_register_shader_tools(this);
}

void AINextReviewAgent::_bind_methods() {
}

AINextReviewAgent::AINextReviewAgent() {
	_configure_next_agent(this, "review_agent", AINextPrompts::get_review_prompt(), "review");
	_register_shared_read_tools(this);
}
