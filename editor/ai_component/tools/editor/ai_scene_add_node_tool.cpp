/**************************************************************************/
/*  ai_scene_add_node_tool.cpp                                            */
/**************************************************************************/

#include "ai_scene_add_node_tool.h"

#include "core/variant/variant.h"

AISceneAddNodeTool::AISceneAddNodeTool() {
	service.instantiate();
}

String AISceneAddNodeTool::get_name() const {
	return "scene.add_node";
}

String AISceneAddNodeTool::get_description() const {
	return "Adds a node under the current edited scene using Godot editor APIs and undo/redo, then saves the scene.";
}

Dictionary AISceneAddNodeTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary parent_path_property;
	parent_path_property["type"] = "string";
	parent_path_property["description"] = "Parent path relative to the edited scene root. Use . for the root.";
	properties["parent_path"] = parent_path_property;

	Dictionary type_property;
	type_property["type"] = "string";
	type_property["description"] = "Node class to instantiate, for example Sprite2D, Label, Node3D, or Timer.";
	properties["type"] = type_property;

	Dictionary name_property;
	name_property["type"] = "string";
	name_property["description"] = "Optional node name.";
	properties["name"] = name_property;

	Array required;
	required.push_back("parent_path");
	required.push_back("type");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AISceneAddNodeTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String parent_path = String(p_arguments.get("parent_path", "")).strip_edges();
	const String type = String(p_arguments.get("type", "")).strip_edges();
	const String name = String(p_arguments.get("name", "")).strip_edges();
	print_line(vformat("[AI Agent][Tool:scene.add_node] Start. parent_path=%s type=%s name=%s", parent_path, type, name));

	if (parent_path.is_empty()) {
		result.error = "Missing required parent_path.";
		print_line("[AI Agent][Tool:scene.add_node] Failed: missing required parent_path.");
		return result;
	}
	if (type.is_empty()) {
		result.error = "Missing required type.";
		print_line("[AI Agent][Tool:scene.add_node] Failed: missing required type.");
		return result;
	}

	AISceneEditingResult edit_result = service->add_node(parent_path, type, name);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to add node.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:scene.add_node] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:scene.add_node] Completed. %s", result.content));
	return result;
}
