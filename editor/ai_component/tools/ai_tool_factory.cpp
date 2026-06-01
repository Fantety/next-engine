/**************************************************************************/
/*  ai_tool_factory.cpp                                                   */
/**************************************************************************/

#include "ai_tool_factory.h"

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
#include "editor/ai_component/tools/project/ai_create_markdown_tool.h"
#include "editor/ai_component/tools/project/ai_list_project_tool.h"
#include "editor/ai_component/tools/project/ai_read_file_tool.h"
#include "editor/ai_component/tools/project/ai_search_project_tool.h"

namespace AIToolFactory {

bool register_tool(AIAgentBase *p_agent, const Ref<AITool> &p_tool, AIToolPermission p_permission, const String &p_log_prefix) {
	if (!p_agent || p_tool.is_null()) {
		return false;
	}

	const String tool_name = p_tool->get_name();
	const bool registered = p_agent->add_tool(p_tool, p_permission);
	if (registered && !p_log_prefix.is_empty()) {
		print_line(vformat("%s Registered tool: %s", p_log_prefix, tool_name));
	}
	return registered;
}

void register_shared_project_tools(AIAgentBase *p_agent, const String &p_log_prefix) {
	register_tool<AIListProjectTool>(p_agent, AI_TOOL_PERMISSION_ALLOW, p_log_prefix);
	register_tool<AIReadFileTool>(p_agent, AI_TOOL_PERMISSION_ALLOW, p_log_prefix);
	register_tool<AISearchProjectTool>(p_agent, AI_TOOL_PERMISSION_ALLOW, p_log_prefix);
	register_tool<AICreateMarkdownTool>(p_agent, AI_TOOL_PERMISSION_ALLOW, p_log_prefix);
	register_tool<AIGetEditorContextTool>(p_agent, AI_TOOL_PERMISSION_ALLOW, p_log_prefix);
}

void register_project_write_tools(AIAgentBase *p_agent, AIToolPermission p_permission, const String &p_log_prefix) {
	register_tool<AICreateFolderTool>(p_agent, p_permission, p_log_prefix);
}

void register_scene_inspection_tools(AIAgentBase *p_agent, AIToolPermission p_permission, const String &p_log_prefix) {
	register_tool<AISceneListPropertiesTool>(p_agent, p_permission, p_log_prefix);
}

void register_scene_write_tools(AIAgentBase *p_agent, AIToolPermission p_write_permission, AIToolPermission p_delete_permission, const String &p_log_prefix) {
	register_tool<AISceneCreateSceneTool>(p_agent, p_write_permission, p_log_prefix);
	register_tool<AISceneAddNodeTool>(p_agent, p_write_permission, p_log_prefix);
	register_tool<AISceneInstantiateSceneTool>(p_agent, p_write_permission, p_log_prefix);
	register_tool<AISceneDeleteNodeTool>(p_agent, p_delete_permission, p_log_prefix);
	register_tool<AISceneRenameNodeTool>(p_agent, p_write_permission, p_log_prefix);
	register_tool<AISceneMoveNodeTool>(p_agent, p_write_permission, p_log_prefix);
	register_tool<AISceneSetPropertyTool>(p_agent, p_write_permission, p_log_prefix);
	register_tool<AISceneSaveCurrentSceneTool>(p_agent, p_write_permission, p_log_prefix);
	register_tool<AISceneOpenSceneTool>(p_agent, p_write_permission, p_log_prefix);
}

void register_script_inspection_tools(AIAgentBase *p_agent, AIToolPermission p_permission, const String &p_log_prefix) {
	register_tool<AIScriptInspectTool>(p_agent, p_permission, p_log_prefix);
}

void register_script_write_tools(AIAgentBase *p_agent, AIToolPermission p_write_permission, AIToolPermission p_delete_permission, const String &p_log_prefix) {
	register_tool<AIScriptCreateTool>(p_agent, p_write_permission, p_log_prefix);
	register_tool<AIScriptWriteTool>(p_agent, p_write_permission, p_log_prefix);
	register_tool<AIScriptPatchFunctionTool>(p_agent, p_write_permission, p_log_prefix);
	register_tool<AIScriptBindToNodeTool>(p_agent, p_write_permission, p_log_prefix);
	register_tool<AIScriptUnbindFromNodeTool>(p_agent, p_write_permission, p_log_prefix);
	register_tool<AIScriptDeleteTool>(p_agent, p_delete_permission, p_log_prefix);
}

void register_shader_tools(AIAgentBase *p_agent, AIToolPermission p_write_permission, AIToolPermission p_delete_permission, const String &p_log_prefix) {
	register_tool<AIShaderCreateTool>(p_agent, p_write_permission, p_log_prefix);
	register_tool<AIShaderEditTool>(p_agent, p_write_permission, p_log_prefix);
	register_tool<AIShaderApplyToNodeTool>(p_agent, p_write_permission, p_log_prefix);
	register_tool<AIShaderDeleteTool>(p_agent, p_delete_permission, p_log_prefix);
}

} // namespace AIToolFactory
