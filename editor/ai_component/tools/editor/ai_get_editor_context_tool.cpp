/**************************************************************************/
/*  ai_get_editor_context_tool.cpp                                        */
/**************************************************************************/

#include "ai_get_editor_context_tool.h"

#include "core/variant/variant.h"
#include "editor/ai_component/context/ai_editor_context_snapshot.h"

String AIGetEditorContextTool::get_name() const {
	return "editor.get_context";
}

String AIGetEditorContextTool::get_description() const {
	return "Returns safe, read-only metadata about the current Godot editor project, scene root, and available viewport/window sizes.";
}

Dictionary AIGetEditorContextTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIGetEditorContextTool::execute(const Dictionary &p_arguments) {
	(void)p_arguments;
	print_line("[AI Agent][Tool:editor.get_context] Start.");
	AIToolResult result;

	Ref<AIEditorContextSnapshotService> snapshot_service;
	snapshot_service.instantiate();
	AIEditorContextSnapshotResult snapshot = snapshot_service->collect();
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
