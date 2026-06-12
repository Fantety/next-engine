/**************************************************************************/
/*  ai_get_editor_context_tool.cpp                                        */
/**************************************************************************/

#include "ai_get_editor_context_tool.h"

#include "core/variant/variant.h"
#include "editor/ai_component/agent/ai_agent_profile.h"
#include "editor/ai_component/context/ai_editor_context_snapshot.h"
#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

String AIV1GetEditorContextTool::get_name() const {
	return "editor.get_context";
}

String AIV1GetEditorContextTool::get_description() const {
	return "Returns safe, read-only metadata about the current Godot editor project, scene root, and available viewport/window sizes.";
}

Dictionary AIV1GetEditorContextTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	schema["properties"] = properties;
	return schema;
}

AIV1EditorToolResult AIV1GetEditorContextTool::execute_tool(const Dictionary &p_arguments) {
	(void)p_arguments;
	print_line("[AI Agent][Tool:editor.get_context] Start.");
	AIV1EditorToolResult result;

	Ref<AIEditorContextSnapshotService> snapshot_service;
	snapshot_service.instantiate();
	AIAgentProfile profile = AIAgentProfile::get_ask_profile();
	Ref<AIV1ToolExecutionState> execution_context = AIV1ToolExecutionState::get_current();
	if (execution_context.is_valid()) {
		profile = AIAgentProfile::from_id(execution_context->get_agent_profile_id());
	}
	AIEditorContextSnapshotResult snapshot = snapshot_service->collect(profile.get_capabilities_id(), profile.get_capabilities_summary());
	if (!snapshot.success) {
		result.error = snapshot.error.is_empty() ? String("Failed to collect editor context.") : snapshot.error;
		print_line(vformat("[AI Agent][Tool:editor.get_context] Failed: %s", result.error));
		return result;
	}

	result.content = snapshot.content;
	result.metadata = snapshot.metadata;
	print_line(vformat("[AI Agent][Tool:editor.get_context] Completed. project=%s path=%s", String(result.metadata.get("application_name", "")), String(result.metadata.get("resource_path", ""))));
	return result;
}
