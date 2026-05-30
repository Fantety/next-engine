/**************************************************************************/
/*  ai_scene_instantiate_scene_tool.cpp                                   */
/**************************************************************************/

#include "ai_scene_instantiate_scene_tool.h"

#include "core/variant/variant.h"

AISceneInstantiateSceneTool::AISceneInstantiateSceneTool() {
	service.instantiate();
}

String AISceneInstantiateSceneTool::get_name() const {
	return "scene.instantiate_scene";
}

String AISceneInstantiateSceneTool::get_description() const {
	return "Instantiates a PackedScene under the current edited scene using Godot editor APIs and undo/redo, then saves the scene.";
}

Dictionary AISceneInstantiateSceneTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary parent_path_property;
	parent_path_property["type"] = "string";
	parent_path_property["description"] = "Parent path relative to the edited scene root. Use . for the root.";
	properties["parent_path"] = parent_path_property;

	Dictionary scene_path_property;
	scene_path_property["type"] = "string";
	scene_path_property["description"] = "Project scene path to instantiate, for example res://scenes/player.tscn.";
	properties["scene_path"] = scene_path_property;

	Dictionary name_property;
	name_property["type"] = "string";
	name_property["description"] = "Optional instance node name.";
	properties["name"] = name_property;

	Dictionary position_property;
	position_property["type"] = "integer";
	position_property["description"] = "Target child index inside the parent. Use -1 to append.";
	properties["position"] = position_property;

	Array required;
	required.push_back("parent_path");
	required.push_back("scene_path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AISceneInstantiateSceneTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String parent_path = String(p_arguments.get("parent_path", "")).strip_edges();
	const String scene_path = String(p_arguments.get("scene_path", "")).strip_edges();
	const String name = String(p_arguments.get("name", "")).strip_edges();
	const int position = p_arguments.get("position", -1);
	print_line(vformat("[AI Agent][Tool:scene.instantiate_scene] Start. parent_path=%s scene_path=%s name=%s position=%d", parent_path, scene_path, name, position));

	if (parent_path.is_empty()) {
		result.error = "Missing required parent_path.";
		print_line("[AI Agent][Tool:scene.instantiate_scene] Failed: missing required parent_path.");
		return result;
	}
	if (scene_path.is_empty()) {
		result.error = "Missing required scene_path.";
		print_line("[AI Agent][Tool:scene.instantiate_scene] Failed: missing required scene_path.");
		return result;
	}

	AISceneEditingResult edit_result = service->instantiate_scene(parent_path, scene_path, name, position);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to instantiate scene.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:scene.instantiate_scene] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:scene.instantiate_scene] Completed. %s", result.content));
	return result;
}
