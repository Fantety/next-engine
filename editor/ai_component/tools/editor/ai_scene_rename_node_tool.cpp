/**************************************************************************/
/*  ai_scene_rename_node_tool.cpp                                         */
/**************************************************************************/

#include "ai_scene_rename_node_tool.h"

#include "core/variant/variant.h"

AISceneRenameNodeTool::AISceneRenameNodeTool() {
	service.instantiate();
}

String AISceneRenameNodeTool::get_name() const {
	return "scene.rename_node";
}

String AISceneRenameNodeTool::get_description() const {
	return "Renames a node in the current edited scene using Godot editor APIs and saves the scene.";
}

Dictionary AISceneRenameNodeTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary node_path_property;
	node_path_property["type"] = "string";
	node_path_property["description"] = "Node path relative to the edited scene root.";
	properties["node_path"] = node_path_property;

	Dictionary new_name_property;
	new_name_property["type"] = "string";
	new_name_property["description"] = "New node name.";
	properties["new_name"] = new_name_property;

	Array required;
	required.push_back("node_path");
	required.push_back("new_name");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AISceneRenameNodeTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String node_path = String(p_arguments.get("node_path", "")).strip_edges();
	const String new_name = String(p_arguments.get("new_name", "")).strip_edges();
	print_line(vformat("[AI Agent][Tool:scene.rename_node] Start. node_path=%s new_name=%s", node_path, new_name));

	if (node_path.is_empty()) {
		result.error = "Missing required node_path.";
		print_line("[AI Agent][Tool:scene.rename_node] Failed: missing required node_path.");
		return result;
	}
	if (new_name.is_empty()) {
		result.error = "Missing required new_name.";
		print_line("[AI Agent][Tool:scene.rename_node] Failed: missing required new_name.");
		return result;
	}

	AISceneEditingResult edit_result = service->rename_node(node_path, new_name);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to rename node.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:scene.rename_node] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:scene.rename_node] Completed. %s", result.content));
	return result;
}
