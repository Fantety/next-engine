/**************************************************************************/
/*  ai_scene_delete_node_tool.cpp                                         */
/**************************************************************************/

#include "ai_scene_delete_node_tool.h"

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

AIV1SceneDeleteNodeTool::AIV1SceneDeleteNodeTool() {
	service.instantiate();
}

String AIV1SceneDeleteNodeTool::get_name() const {
	return "scene.delete_node";
}

String AIV1SceneDeleteNodeTool::get_description() const {
	return "Deletes a node from the currently edited scene through Godot editor APIs. This tool must be explicitly approved by the user when registered with delete permission.";
}

Dictionary AIV1SceneDeleteNodeTool::get_parameters_schema() const {
	Dictionary properties;
	properties["node_path"] = AIV1ToolHelpers::make_string_property("Node path relative to the edited scene root to delete. The scene root itself cannot be deleted.");

	Array required;
	required.push_back("node_path");
	return AIV1ToolHelpers::make_object_schema(properties, required);
}

AIV1EditorToolResult AIV1SceneDeleteNodeTool::execute_tool(const Dictionary &p_arguments) {
	const String node_path = AIV1ToolHelpers::get_stripped_string(p_arguments, "node_path");
	print_line(vformat("[AI Agent][Tool:scene.delete_node] Start. node_path=%s", node_path));

	if (node_path.is_empty()) {
		print_line("[AI Agent][Tool:scene.delete_node] Failed: missing required node_path.");
		return AIV1ToolHelpers::make_missing_required_error("node_path");
	}

	AIV1SceneEditingResult edit_result = service->delete_node(node_path);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to delete scene node.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:scene.delete_node] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:scene.delete_node] Completed. %s", result.content));
	return result;
}
