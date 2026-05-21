/**************************************************************************/
/*  ai_scene_delete_node_tool.cpp                                         */
/**************************************************************************/

#include "ai_scene_delete_node_tool.h"

#include "core/variant/variant.h"

AISceneDeleteNodeTool::AISceneDeleteNodeTool() {
	service.instantiate();
}

String AISceneDeleteNodeTool::get_name() const {
	return "scene.delete_node";
}

String AISceneDeleteNodeTool::get_description() const {
	return "Deletes a non-root node from the current edited scene using Godot editor APIs and undo/redo.";
}

Dictionary AISceneDeleteNodeTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary node_path_property;
	node_path_property["type"] = "string";
	node_path_property["description"] = "Node path relative to the edited scene root. The root itself cannot be deleted.";
	properties["node_path"] = node_path_property;

	Array required;
	required.push_back("node_path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AISceneDeleteNodeTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String node_path = String(p_arguments.get("node_path", "")).strip_edges();
	print_line(vformat("[AI Agent][Tool:scene.delete_node] Start. node_path=%s", node_path));

	if (node_path.is_empty()) {
		result.error = "Missing required node_path.";
		print_line("[AI Agent][Tool:scene.delete_node] Failed: missing required node_path.");
		return result;
	}

	AISceneEditingResult edit_result = service->delete_node(node_path);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to delete node.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:scene.delete_node] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:scene.delete_node] Completed. %s", result.content));
	return result;
}
