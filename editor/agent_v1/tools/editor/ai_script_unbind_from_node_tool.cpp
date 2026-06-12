/**************************************************************************/
/*  ai_script_unbind_from_node_tool.cpp                                   */
/**************************************************************************/

#include "ai_script_unbind_from_node_tool.h"

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

AIV1ScriptUnbindFromNodeTool::AIV1ScriptUnbindFromNodeTool() {
	service.instantiate();
}

String AIV1ScriptUnbindFromNodeTool::get_name() const {
	return "script.unbind_from_node";
}

String AIV1ScriptUnbindFromNodeTool::get_description() const {
	return "Removes the script binding from a node in the currently edited scene using editor undo/redo APIs.";
}

Dictionary AIV1ScriptUnbindFromNodeTool::get_parameters_schema() const {
	Dictionary properties;
	properties["node_path"] = AIV1ToolHelpers::make_string_property("Node path relative to the edited scene root. Use . for the root.");

	Array required;
	required.push_back("node_path");
	return AIV1ToolHelpers::make_object_schema(properties, required);
}

AIV1EditorToolResult AIV1ScriptUnbindFromNodeTool::execute_tool(const Dictionary &p_arguments) {
	const String node_path = AIV1ToolHelpers::get_stripped_string(p_arguments, "node_path");
	print_line(vformat("[AI Agent][Tool:script.unbind_from_node] Start. node_path=%s", node_path));
	if (node_path.is_empty()) {
		print_line("[AI Agent][Tool:script.unbind_from_node] Failed: missing required node_path.");
		return AIV1ToolHelpers::make_missing_required_error("node_path");
	}

	AIV1ScriptEditingResult edit_result = service->unbind_from_node(node_path);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to unbind script from node.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:script.unbind_from_node] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:script.unbind_from_node] Completed. %s", result.content));
	return result;
}
