/**************************************************************************/
/*  ai_scene_create_scene_tool.cpp                                        */
/**************************************************************************/

#include "ai_scene_create_scene_tool.h"

#include "core/variant/variant.h"

AISceneCreateSceneTool::AISceneCreateSceneTool() {
	service.instantiate();
}

String AISceneCreateSceneTool::get_name() const {
	return "scene.create_scene";
}

String AISceneCreateSceneTool::get_description() const {
	return "Creates a new editor scene tab with a root node using Godot editor APIs.";
}

Dictionary AISceneCreateSceneTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary root_type_property;
	root_type_property["type"] = "string";
	root_type_property["description"] = "Root node class, for example Node2D, Node3D, Control, or Node.";
	properties["root_type"] = root_type_property;

	Dictionary root_name_property;
	root_name_property["type"] = "string";
	root_name_property["description"] = "Optional root node name.";
	properties["root_name"] = root_name_property;

	Array required;
	required.push_back("root_type");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AISceneCreateSceneTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String root_type = String(p_arguments.get("root_type", "")).strip_edges();
	const String root_name = String(p_arguments.get("root_name", "")).strip_edges();
	print_line(vformat("[AI Agent][Tool:scene.create_scene] Start. root_type=%s root_name=%s", root_type, root_name));

	if (root_type.is_empty()) {
		result.error = "Missing required root_type.";
		print_line("[AI Agent][Tool:scene.create_scene] Failed: missing required root_type.");
		return result;
	}

	AISceneEditingResult edit_result = service->create_scene(root_type, root_name);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to create scene.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:scene.create_scene] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:scene.create_scene] Completed. %s", result.content));
	return result;
}
