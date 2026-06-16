/**************************************************************************/
/*  ai_editor_tools_v1.cpp                                                 */
/**************************************************************************/

#include "ai_editor_tools_v1.h"

#include "editor/agent_v1/tools/ai_tool_registry_v1.h"
#include "editor/agent_v1/tools/editor/ai_docs_search_tool.h"
#include "editor/agent_v1/tools/editor/ai_editor_runtime_tools.h"
#include "editor/agent_v1/tools/editor/ai_get_editor_context_tool.h"
#include "editor/agent_v1/tools/editor/ai_scene_apply_patch_tool.h"
#include "editor/agent_v1/tools/editor/ai_scene_delete_node_tool.h"
#include "editor/agent_v1/tools/editor/ai_scene_describe_tree_tool.h"
#include "editor/agent_v1/tools/editor/ai_scene_inspect_node_tool.h"
#include "editor/agent_v1/tools/editor/ai_scene_list_properties_tool.h"
#include "editor/agent_v1/tools/editor/ai_script_bind_to_node_tool.h"
#include "editor/agent_v1/tools/editor/ai_script_create_tool.h"
#include "editor/agent_v1/tools/editor/ai_script_delete_tool.h"
#include "editor/agent_v1/tools/editor/ai_script_inspect_tool.h"
#include "editor/agent_v1/tools/editor/ai_script_patch_function_tool.h"
#include "editor/agent_v1/tools/editor/ai_script_unbind_from_node_tool.h"
#include "editor/agent_v1/tools/editor/ai_script_write_tool.h"
#include "editor/agent_v1/tools/editor/ai_shader_apply_to_node_tool.h"
#include "editor/agent_v1/tools/editor/ai_shader_create_tool.h"
#include "editor/agent_v1/tools/editor/ai_shader_delete_tool.h"
#include "editor/agent_v1/tools/editor/ai_shader_edit_tool.h"
#include "editor/agent_v1/tools/editor/ai_shader_set_parameters_tool.h"
#include "editor/agent_v1/tools/project/ai_attach_multimodal_file_tool.h"
#include "editor/agent_v1/tools/project/ai_create_folder_tool.h"
#include "editor/agent_v1/tools/project/ai_delete_file_tool.h"
#include "editor/agent_v1/tools/project/ai_list_project_tool.h"
#include "editor/agent_v1/tools/project/ai_read_file_tool.h"
#include "editor/agent_v1/tools/project/ai_requirement_form_tool.h"
#include "editor/agent_v1/tools/project/ai_search_project_tool.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

thread_local Ref<AIV1ToolExecutionState> AIV1ToolExecutionState::current;

bool AIV1EditorToolResult::is_error() const {
	return !error.is_empty();
}

Dictionary AIV1EditorToolResult::to_dict() const {
	Dictionary dict;
	dict["content"] = content;
	dict["error"] = error;
	dict["metadata"] = metadata;
	dict["truncated"] = truncated;
	return dict;
}

void AIV1ToolExecutionState::_bind_methods() {
}

void AIV1ToolExecutionState::set_current(const Ref<AIV1ToolExecutionState> &p_context) {
	current = p_context;
}

Ref<AIV1ToolExecutionState> AIV1ToolExecutionState::get_current() {
	return current;
}

void AIV1ToolExecutionState::clear_current() {
	current.unref();
}

bool AIV1ToolExecutionState::is_current_cancel_requested() {
	return current.is_valid() && current->is_cancel_requested();
}

void AIV1ToolExecutionState::set_session_id(const String &p_session_id) {
	session_id = p_session_id;
}

String AIV1ToolExecutionState::get_session_id() const {
	return session_id;
}

void AIV1ToolExecutionState::set_agent_id(const String &p_agent_id) {
	agent_id = p_agent_id;
}

String AIV1ToolExecutionState::get_agent_id() const {
	return agent_id;
}

String AIV1ToolExecutionState::get_agent_profile_id() const {
	return agent_id;
}

void AIV1ToolExecutionState::set_tool_call_id(const String &p_tool_call_id) {
	tool_call_id = p_tool_call_id;
}

String AIV1ToolExecutionState::get_tool_call_id() const {
	return tool_call_id;
}

void AIV1ToolExecutionState::set_review_changes(bool p_review_changes) {
	review_changes = p_review_changes;
}

bool AIV1ToolExecutionState::should_review_changes() const {
	return review_changes;
}

void AIV1ToolExecutionState::set_cancel_token(const Ref<AICancelToken> &p_cancel_token) {
	cancel_token = p_cancel_token;
}

Ref<AICancelToken> AIV1ToolExecutionState::get_cancel_token() const {
	return cancel_token;
}

void AIV1ToolExecutionState::request_cancel() {
	cancel_requested.set();
}

void AIV1ToolExecutionState::clear_cancel_request() {
	cancel_requested.clear();
}

bool AIV1ToolExecutionState::is_cancel_requested() const {
	return cancel_requested.is_set() || (cancel_token.is_valid() && cancel_token->is_cancel_requested());
}

void AIV1EditorTool::_bind_methods() {
}

void AIV1EditorTool::configure_editor_tool(const String &p_permission_action, const String &p_permission_effect, const String &p_permission_reason, const Dictionary &p_metadata) {
	permission_action = p_permission_action.strip_edges();
	permission_effect = p_permission_effect.strip_edges().to_lower();
	permission_reason = p_permission_reason.strip_edges();

	Dictionary tool_metadata = p_metadata.duplicate(true);
	tool_metadata["tool_origin"] = "agent_v1";
	if (!permission_action.is_empty()) {
		tool_metadata["action"] = permission_action;
	}
	if (!permission_effect.is_empty()) {
		tool_metadata["default_effect"] = permission_effect;
	}
	configure(get_description(), get_parameters_schema(), Callable(), tool_metadata);
}

String AIV1EditorTool::get_permission_resource(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context) const {
	const Variant path = p_arguments.get("path", Variant());
	if (path.get_type() == Variant::STRING && !String(path).strip_edges().is_empty()) {
		return String(path).strip_edges();
	}
	const Variant node_path = p_arguments.get("node_path", Variant());
	if (node_path.get_type() == Variant::STRING && !String(node_path).strip_edges().is_empty()) {
		return String(node_path).strip_edges();
	}
	const Variant shader_path = p_arguments.get("shader_path", Variant());
	if (shader_path.get_type() == Variant::STRING && !String(shader_path).strip_edges().is_empty()) {
		return String(shader_path).strip_edges();
	}
	if (!p_context.root_dir.strip_edges().is_empty()) {
		return p_context.root_dir.strip_edges();
	}
	return "*";
}

bool AIV1EditorTool::assert_tool_permission(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIError &r_error, AIPermissionDecision *r_decision) const {
	if (r_decision) {
		*r_decision = AIPermissionDecision();
	}
	if (permission_action.is_empty()) {
		if (r_decision) {
			r_decision->allowed = true;
		}
		r_error = AIError::none();
		return true;
	}
	if (p_context.permission_service.is_null()) {
		if (permission_effect == "allow") {
			if (r_decision) {
				r_decision->allowed = true;
				r_decision->effect = "allow";
			}
			r_error = AIError::none();
			return true;
		}
		r_error = AIError::make(AI_ERROR_PERMISSION, "PermissionService is required for this tool.");
		return false;
	}

	Dictionary input = p_context.make_permission_input(permission_action, get_permission_resource(p_arguments, p_context), permission_reason);
	if (!permission_effect.is_empty()) {
		input["default_effect"] = permission_effect;
	}

	AIPermissionDecision decision;
	if (!p_context.permission_service->assert_permission_struct(input, decision, r_error)) {
		if (r_decision) {
			*r_decision = decision;
		}
		if (!r_error.is_error()) {
			r_error = decision.error.is_error() ? decision.error : AIError::make(AI_ERROR_PERMISSION, "Permission denied.");
		}
		return false;
	}
	if (r_decision) {
		*r_decision = decision;
	}
	r_error = AIError::none();
	return true;
}

bool AIV1EditorTool::execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) {
	if (p_context.cancel_token.is_valid() && p_context.cancel_token->is_cancel_requested()) {
		r_error = AIError::make(AI_ERROR_CANCELLED, p_context.cancel_token->get_cancel_message());
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}
	AIPermissionDecision permission_decision;
	if (!assert_tool_permission(p_arguments, p_context, r_error, &permission_decision)) {
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Ref<AIV1ToolExecutionState> previous_context = AIV1ToolExecutionState::get_current();
	Ref<AIV1ToolExecutionState> current_context;
	current_context.instantiate();
	current_context->set_session_id(p_context.session_id);
	current_context->set_agent_id(p_context.agent_id);
	current_context->set_tool_call_id(p_context.call_id);
	current_context->set_cancel_token(p_context.cancel_token);
	current_context->set_review_changes(bool(p_context.metadata.get("review_changes", true)));
	AIV1ToolExecutionState::set_current(current_context);

	AIV1EditorToolResult tool_result;
	const Variant submitted_answers = permission_decision.metadata.get("answers", Variant());
	if (AIV1RequirementFormTool::is_requirement_form_tool(get_name()) && submitted_answers.get_type() == Variant::DICTIONARY) {
		tool_result = AIV1RequirementFormTool::make_submission_result(p_arguments, Dictionary(submitted_answers));
	} else {
		tool_result = execute_tool(p_arguments);
	}

	if (previous_context.is_valid()) {
		AIV1ToolExecutionState::set_current(previous_context);
	} else {
		AIV1ToolExecutionState::clear_current();
	}

	Dictionary structured = tool_result.to_dict();
	r_result.metadata = tool_result.metadata.duplicate(true);
	r_result.metadata["tool_origin"] = "agent_v1";
	r_result.metadata["agent_tool_name"] = p_context.tool_name;
	r_result.metadata["legacy_tool_name"] = get_name();
	r_result.metadata["truncated"] = tool_result.truncated;
	if (tool_result.is_error()) {
		r_error = AIError::make(AI_ERROR_INTERNAL, tool_result.error, structured);
		r_result = AIV1ToolExecutionResult::fail(r_error);
		r_result.metadata = tool_result.metadata.duplicate(true);
		r_result.metadata["tool_origin"] = "agent_v1";
		r_result.metadata["agent_tool_name"] = p_context.tool_name;
		r_result.metadata["legacy_tool_name"] = get_name();
		r_result.metadata["truncated"] = tool_result.truncated;
		return false;
	}

	r_result = AIV1ToolExecutionResult::ok(structured, tool_result.content, structured);
	r_result.metadata = tool_result.metadata.duplicate(true);
	r_result.metadata["tool_origin"] = "agent_v1";
	r_result.metadata["agent_tool_name"] = p_context.tool_name;
	r_result.metadata["legacy_tool_name"] = get_name();
	r_result.metadata["truncated"] = tool_result.truncated;
	return true;
}

namespace AIV1ToolHelpers {

Dictionary make_string_property(const String &p_description) {
	Dictionary property;
	property["type"] = "string";
	property["description"] = p_description;
	return property;
}

Dictionary make_boolean_property(const String &p_description) {
	Dictionary property;
	property["type"] = "boolean";
	property["description"] = p_description;
	return property;
}

Dictionary make_object_schema(const Dictionary &p_properties, const Array &p_required) {
	Dictionary schema;
	schema["type"] = "object";
	schema["properties"] = p_properties;
	if (!p_required.is_empty()) {
		schema["required"] = p_required;
	}
	return schema;
}

String get_stripped_string(const Dictionary &p_arguments, const String &p_key, const String &p_default) {
	return String(p_arguments.get(p_key, p_default)).strip_edges();
}

bool get_bool(const Dictionary &p_arguments, const String &p_key, bool p_default) {
	return bool(p_arguments.get(p_key, p_default));
}

AIV1EditorToolResult make_missing_required_error(const String &p_key) {
	AIV1EditorToolResult result;
	result.error = "Missing required " + p_key + ".";
	return result;
}

} // namespace AIV1ToolHelpers

namespace AIV1EditorTools {

namespace {

template <typename TTool>
void _register_tool(const Ref<AIV1ToolRegistry> &p_registry, const String &p_category, const String &p_action, const String &p_default_effect, const String &p_reason) {
	if (p_registry.is_null()) {
		return;
	}

	Ref<TTool> tool;
	tool.instantiate();
	const String legacy_name = tool->get_name();
	const String name = legacy_name.replace(".", "_");
	if (p_registry->has_tool(name)) {
		return;
	}

	Dictionary metadata;
	metadata["tool_origin"] = "agent_v1";
	metadata["agent_tool_name"] = name;
	metadata["legacy_tool_name"] = legacy_name;
	metadata["category"] = p_category;
	metadata["phase"] = "editor_tools";
	if (!p_action.is_empty()) {
		metadata["action"] = p_action;
	}
	if (!p_default_effect.is_empty()) {
		metadata["default_effect"] = p_default_effect;
	}
	tool->configure_editor_tool(p_action, p_default_effect, p_reason, metadata);
	p_registry->register_tool_struct(name, tool, "agent_v1_editor", metadata);
}

} // namespace

void register_editor_tools(const Ref<AIV1ToolRegistry> &p_registry) {
	_register_tool<AIV1ListProjectTool>(p_registry, "project", "project.read", "allow", "List project files.");
	_register_tool<AIV1ProjectReadFileTool>(p_registry, "project", "project.read", "allow", "Read project file content.");
	_register_tool<AIV1SearchProjectTool>(p_registry, "project", "project.read", "allow", "Search project files.");
	_register_tool<AIV1AttachMultimodalFileTool>(p_registry, "project", "project.read", "allow", "Attach a project file to the model context.");
	_register_tool<AIV1RequirementFormTool>(p_registry, "project", "agent.requirement_form", "ask", "Ask the user to confirm requirements.");
	_register_tool<AIV1GetEditorContextTool>(p_registry, "editor", "editor.read", "allow", "Read editor context.");
	_register_tool<AIV1DocsSearchTool>(p_registry, "editor", "docs.search", "allow", "Search Godot documentation.");
	_register_tool<AIV1EditorRunSceneTool>(p_registry, "editor_runtime", "editor.run", "allow", "Run the current or selected scene.");
	_register_tool<AIV1EditorStopRunningSceneTool>(p_registry, "editor_runtime", "editor.run", "allow", "Stop the running scene.");
	_register_tool<AIV1EditorGetTerminalErrorsTool>(p_registry, "editor_runtime", "editor.read", "allow", "Read recent editor/runtime terminal errors.");
	_register_tool<AIV1CreateFolderTool>(p_registry, "project", "project.write", "allow", "Create a project folder.");
	_register_tool<AIV1ProjectDeleteFileTool>(p_registry, "project", "project.write", "ask", "Delete a project file.");
	_register_tool<AIV1SceneDescribeTreeTool>(p_registry, "scene", "scene.read", "allow", "Describe the current scene tree.");
	_register_tool<AIV1SceneInspectNodeTool>(p_registry, "scene", "scene.read", "allow", "Inspect a scene node.");
	_register_tool<AIV1SceneListPropertiesTool>(p_registry, "scene", "scene.read", "allow", "List scene node properties.");
	_register_tool<AIV1SceneApplyPatchTool>(p_registry, "scene", "scene.write", "allow", "Apply a scene patch.");
	_register_tool<AIV1SceneDeleteNodeTool>(p_registry, "scene", "scene.write", "allow", "Delete a scene node.");
	_register_tool<AIV1ScriptInspectTool>(p_registry, "script", "script.read", "allow", "Inspect a GDScript file.");
	_register_tool<AIV1ScriptCreateTool>(p_registry, "script", "script.write", "allow", "Create a GDScript file.");
	_register_tool<AIV1ScriptWriteTool>(p_registry, "script", "script.write", "allow", "Write a GDScript file.");
	_register_tool<AIV1ScriptPatchFunctionTool>(p_registry, "script", "script.write", "allow", "Patch a GDScript function.");
	_register_tool<AIV1ScriptBindToNodeTool>(p_registry, "script", "script.write", "allow", "Bind a script to a scene node.");
	_register_tool<AIV1ScriptUnbindFromNodeTool>(p_registry, "script", "script.write", "allow", "Unbind a script from a scene node.");
	_register_tool<AIV1ScriptDeleteTool>(p_registry, "script", "script.write", "ask", "Delete a GDScript file.");
	_register_tool<AIV1ShaderCreateTool>(p_registry, "shader", "shader.write", "allow", "Create a shader file.");
	_register_tool<AIV1ShaderEditTool>(p_registry, "shader", "shader.write", "allow", "Edit a shader file.");
	_register_tool<AIV1ShaderApplyToNodeTool>(p_registry, "shader", "shader.write", "allow", "Apply a shader to a scene node.");
	_register_tool<AIV1ShaderSetParametersTool>(p_registry, "shader", "shader.write", "allow", "Set shader parameters on a scene node.");
	_register_tool<AIV1ShaderDeleteTool>(p_registry, "shader", "shader.write", "ask", "Delete a shader file.");
}

} // namespace AIV1EditorTools
