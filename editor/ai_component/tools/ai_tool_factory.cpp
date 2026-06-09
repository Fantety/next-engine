/**************************************************************************/
/*  ai_tool_factory.cpp                                                   */
/**************************************************************************/

#include "ai_tool_factory.h"

#include "editor/ai_component/tools/editor/ai_docs_search_tool.h"
#include "editor/ai_component/tools/editor/ai_get_editor_context_tool.h"
#include "editor/ai_component/tools/editor/ai_editor_runtime_tools.h"
#include "editor/ai_component/tools/editor/ai_scene_apply_patch_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_delete_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_describe_tree_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_inspect_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_list_properties_tool.h"
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
#include "editor/ai_component/tools/editor/ai_shader_set_parameters_tool.h"
#include "editor/ai_component/tools/project/ai_attach_multimodal_file_tool.h"
#include "editor/ai_component/tools/project/ai_create_folder_tool.h"
#include "editor/ai_component/tools/project/ai_create_markdown_tool.h"
#include "editor/ai_component/tools/project/ai_list_project_tool.h"
#include "editor/ai_component/tools/project/ai_read_file_tool.h"
#include "editor/ai_component/tools/project/ai_requirement_form_tool.h"
#include "editor/ai_component/tools/project/ai_search_project_tool.h"

namespace AIToolFactory {

namespace {

const char *TOOL_CATEGORY_PROJECT = "project";
const char *TOOL_CATEGORY_EDITOR_RUNTIME = "editor_runtime";
const char *TOOL_CATEGORY_SCENE = "scene";
const char *TOOL_CATEGORY_SCRIPT = "script";
const char *TOOL_CATEGORY_SHADER = "shader";

} // namespace

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

bool register_tool_with_exposure(AIAgentBase *p_agent, const Ref<AITool> &p_tool, AIToolPermission p_permission, const String &p_category, bool p_pinned, const String &p_log_prefix) {
	const bool registered = register_tool(p_agent, p_tool, p_permission, p_log_prefix);
	if (!registered || !p_agent || p_tool.is_null()) {
		return registered;
	}

	Ref<AIToolRegistry> registry = p_agent->get_tool_registry();
	if (registry.is_valid()) {
		registry->set_tool_exposure(p_tool->get_name(), p_category, p_pinned);
	}
	return registered;
}

void register_shared_project_tools(AIAgentBase *p_agent, const String &p_log_prefix) {
	register_tool<AIListProjectTool>(p_agent, AI_TOOL_PERMISSION_ALLOW, p_log_prefix);
	register_tool<AIReadFileTool>(p_agent, AI_TOOL_PERMISSION_ALLOW, p_log_prefix);
	register_tool<AISearchProjectTool>(p_agent, AI_TOOL_PERMISSION_ALLOW, p_log_prefix);
	register_tool_with_exposure<AIAttachMultimodalFileTool>(p_agent, AI_TOOL_PERMISSION_ALLOW, TOOL_CATEGORY_PROJECT, false, p_log_prefix);
	register_tool_with_exposure<AICreateMarkdownTool>(p_agent, AI_TOOL_PERMISSION_ALLOW, TOOL_CATEGORY_PROJECT, false, p_log_prefix);
	register_tool<AIRequirementFormTool>(p_agent, AI_TOOL_PERMISSION_ASK, p_log_prefix);
	register_tool<AIGetEditorContextTool>(p_agent, AI_TOOL_PERMISSION_ALLOW, p_log_prefix);
	register_tool<AIDocsSearchTool>(p_agent, AI_TOOL_PERMISSION_ALLOW, p_log_prefix);
}

void register_editor_runtime_tools(AIAgentBase *p_agent, AIToolPermission p_permission, const String &p_log_prefix) {
	register_tool_with_exposure<AIEditorRunSceneTool>(p_agent, p_permission, TOOL_CATEGORY_EDITOR_RUNTIME, false, p_log_prefix);
	register_tool_with_exposure<AIEditorStopRunningSceneTool>(p_agent, p_permission, TOOL_CATEGORY_EDITOR_RUNTIME, false, p_log_prefix);
	register_tool_with_exposure<AIEditorGetTerminalErrorsTool>(p_agent, AI_TOOL_PERMISSION_ALLOW, TOOL_CATEGORY_EDITOR_RUNTIME, false, p_log_prefix);
}

void register_project_write_tools(AIAgentBase *p_agent, AIToolPermission p_permission, const String &p_log_prefix) {
	register_tool_with_exposure<AICreateFolderTool>(p_agent, p_permission, TOOL_CATEGORY_PROJECT, false, p_log_prefix);
}

void register_scene_inspection_tools(AIAgentBase *p_agent, AIToolPermission p_permission, const String &p_log_prefix) {
	register_tool_with_exposure<AISceneDescribeTreeTool>(p_agent, p_permission, TOOL_CATEGORY_SCENE, false, p_log_prefix);
	register_tool_with_exposure<AISceneInspectNodeTool>(p_agent, p_permission, TOOL_CATEGORY_SCENE, false, p_log_prefix);
	register_tool_with_exposure<AISceneListPropertiesTool>(p_agent, p_permission, TOOL_CATEGORY_SCENE, false, p_log_prefix);
}

void register_scene_write_tools(AIAgentBase *p_agent, AIToolPermission p_write_permission, AIToolPermission p_delete_permission, const String &p_log_prefix) {
	register_tool_with_exposure<AISceneApplyPatchTool>(p_agent, p_write_permission, TOOL_CATEGORY_SCENE, false, p_log_prefix);
	register_tool_with_exposure<AISceneDeleteNodeTool>(p_agent, p_delete_permission, TOOL_CATEGORY_SCENE, false, p_log_prefix);
}

void register_script_inspection_tools(AIAgentBase *p_agent, AIToolPermission p_permission, const String &p_log_prefix) {
	register_tool_with_exposure<AIScriptInspectTool>(p_agent, p_permission, TOOL_CATEGORY_SCRIPT, false, p_log_prefix);
}

void register_script_write_tools(AIAgentBase *p_agent, AIToolPermission p_write_permission, AIToolPermission p_delete_permission, const String &p_log_prefix) {
	register_tool_with_exposure<AIScriptCreateTool>(p_agent, p_write_permission, TOOL_CATEGORY_SCRIPT, false, p_log_prefix);
	register_tool_with_exposure<AIScriptWriteTool>(p_agent, p_write_permission, TOOL_CATEGORY_SCRIPT, false, p_log_prefix);
	register_tool_with_exposure<AIScriptPatchFunctionTool>(p_agent, p_write_permission, TOOL_CATEGORY_SCRIPT, false, p_log_prefix);
	register_tool_with_exposure<AIScriptBindToNodeTool>(p_agent, p_write_permission, TOOL_CATEGORY_SCRIPT, false, p_log_prefix);
	register_tool_with_exposure<AIScriptUnbindFromNodeTool>(p_agent, p_write_permission, TOOL_CATEGORY_SCRIPT, false, p_log_prefix);
	register_tool_with_exposure<AIScriptDeleteTool>(p_agent, p_delete_permission, TOOL_CATEGORY_SCRIPT, false, p_log_prefix);
}

void register_shader_tools(AIAgentBase *p_agent, AIToolPermission p_write_permission, AIToolPermission p_delete_permission, const String &p_log_prefix) {
	register_tool_with_exposure<AIShaderCreateTool>(p_agent, p_write_permission, TOOL_CATEGORY_SHADER, false, p_log_prefix);
	register_tool_with_exposure<AIShaderEditTool>(p_agent, p_write_permission, TOOL_CATEGORY_SHADER, false, p_log_prefix);
	register_tool_with_exposure<AIShaderApplyToNodeTool>(p_agent, p_write_permission, TOOL_CATEGORY_SHADER, false, p_log_prefix);
	register_tool_with_exposure<AIShaderSetParametersTool>(p_agent, p_write_permission, TOOL_CATEGORY_SHADER, false, p_log_prefix);
	register_tool_with_exposure<AIShaderDeleteTool>(p_agent, p_delete_permission, TOOL_CATEGORY_SHADER, false, p_log_prefix);
}

} // namespace AIToolFactory
