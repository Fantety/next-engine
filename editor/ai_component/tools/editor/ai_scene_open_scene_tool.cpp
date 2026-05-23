/**************************************************************************/
/*  ai_scene_open_scene_tool.cpp                                          */
/**************************************************************************/

#include "ai_scene_open_scene_tool.h"

#include "core/variant/variant.h"

AISceneOpenSceneTool::AISceneOpenSceneTool() {
	service.instantiate();
}

String AISceneOpenSceneTool::get_name() const {
	return "scene.open_scene";
}

String AISceneOpenSceneTool::get_description() const {
	return "Opens a scene file in the Godot editor.";
}

Dictionary AISceneOpenSceneTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary path_property;
	path_property["type"] = "string";
	path_property["description"] = "Project scene path, for example res://scenes/main.tscn.";
	properties["path"] = path_property;

	Array required;
	required.push_back("path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AISceneOpenSceneTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String path = String(p_arguments.get("path", "")).strip_edges();
	print_line(vformat("[AI Agent][Tool:scene.open_scene] Start. path=%s", path));

	if (path.is_empty()) {
		result.error = "Missing required path.";
		print_line("[AI Agent][Tool:scene.open_scene] Failed: missing required path.");
		return result;
	}

	AISceneEditingResult edit_result = service->open_scene(path);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to open scene.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:scene.open_scene] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:scene.open_scene] Completed. %s", result.content));
	return result;
}
