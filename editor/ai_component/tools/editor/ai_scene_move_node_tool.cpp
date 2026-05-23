/**************************************************************************/
/*  ai_scene_move_node_tool.cpp                                           */
/**************************************************************************/

#include "ai_scene_move_node_tool.h"

#include "core/variant/variant.h"

AISceneMoveNodeTool::AISceneMoveNodeTool() {
	service.instantiate();
}

String AISceneMoveNodeTool::get_name() const {
	return "scene.move_node";
}

String AISceneMoveNodeTool::get_description() const {
	return "Moves a node to another parent in the current edited scene using Godot editor APIs and saves the scene.";
}

Dictionary AISceneMoveNodeTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary node_path_property;
	node_path_property["type"] = "string";
	node_path_property["description"] = "Node path relative to the edited scene root.";
	properties["node_path"] = node_path_property;

	Dictionary new_parent_path_property;
	new_parent_path_property["type"] = "string";
	new_parent_path_property["description"] = "New parent node path relative to the edited scene root. Use . for the root.";
	properties["new_parent_path"] = new_parent_path_property;

	Dictionary position_property;
	position_property["type"] = "integer";
	position_property["description"] = "Target child index inside the new parent. Use -1 to append.";
	properties["position"] = position_property;

	Array required;
	required.push_back("node_path");
	required.push_back("new_parent_path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AISceneMoveNodeTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String node_path = String(p_arguments.get("node_path", "")).strip_edges();
	const String new_parent_path = String(p_arguments.get("new_parent_path", "")).strip_edges();
	const int position = p_arguments.get("position", -1);
	print_line(vformat("[AI Agent][Tool:scene.move_node] Start. node_path=%s new_parent_path=%s position=%d", node_path, new_parent_path, position));

	if (node_path.is_empty()) {
		result.error = "Missing required node_path.";
		print_line("[AI Agent][Tool:scene.move_node] Failed: missing required node_path.");
		return result;
	}
	if (new_parent_path.is_empty()) {
		result.error = "Missing required new_parent_path.";
		print_line("[AI Agent][Tool:scene.move_node] Failed: missing required new_parent_path.");
		return result;
	}

	AISceneEditingResult edit_result = service->move_node(node_path, new_parent_path, position);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to move node.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:scene.move_node] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:scene.move_node] Completed. %s", result.content));
	return result;
}
