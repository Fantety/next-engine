/**************************************************************************/
/*  ai_scene_delete_node_tool.cpp                                         */
/**************************************************************************/

#include "ai_scene_delete_node_tool.h"

#include "editor/ai_component/tools/ai_tool_helpers.h"

AISceneDeleteNodeTool::AISceneDeleteNodeTool() {
	service.instantiate();
}

String AISceneDeleteNodeTool::get_name() const {
	return "scene.delete_node";
}

String AISceneDeleteNodeTool::get_description() const {
	return "Deletes a node from the currently edited scene through Godot editor APIs. This tool must be explicitly approved by the user when registered with delete permission.";
}

Dictionary AISceneDeleteNodeTool::get_parameters_schema() const {
	Dictionary properties;
	properties["node_path"] = AIToolHelpers::make_string_property("Node path relative to the edited scene root to delete. The scene root itself cannot be deleted.");

	Array required;
	required.push_back("node_path");
	return AIToolHelpers::make_object_schema(properties, required);
}

AIToolResult AISceneDeleteNodeTool::execute(const Dictionary &p_arguments) {
	const String node_path = AIToolHelpers::get_stripped_string(p_arguments, "node_path");
	print_line(vformat("[AI Agent][Tool:scene.delete_node] Start. node_path=%s", node_path));

	if (node_path.is_empty()) {
		print_line("[AI Agent][Tool:scene.delete_node] Failed: missing required node_path.");
		return AIToolHelpers::make_missing_required_error("node_path");
	}

	AISceneEditingResult edit_result = service->delete_node(node_path);
	AIToolResult result = AIToolHelpers::from_editing_result(edit_result, "Failed to delete scene node.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:scene.delete_node] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:scene.delete_node] Completed. %s", result.content));
	return result;
}
