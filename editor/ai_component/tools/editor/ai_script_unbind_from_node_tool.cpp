/**************************************************************************/
/*  ai_script_unbind_from_node_tool.cpp                                   */
/**************************************************************************/

#include "ai_script_unbind_from_node_tool.h"

#include "editor/ai_component/tools/ai_tool_helpers.h"

AIScriptUnbindFromNodeTool::AIScriptUnbindFromNodeTool() {
	service.instantiate();
}

String AIScriptUnbindFromNodeTool::get_name() const {
	return "script.unbind_from_node";
}

String AIScriptUnbindFromNodeTool::get_description() const {
	return "Removes the script binding from a node in the currently edited scene using editor undo/redo APIs.";
}

Dictionary AIScriptUnbindFromNodeTool::get_parameters_schema() const {
	Dictionary properties;
	properties["node_path"] = AIToolHelpers::make_string_property("Node path relative to the edited scene root. Use . for the root.");

	Array required;
	required.push_back("node_path");
	return AIToolHelpers::make_object_schema(properties, required);
}

AIToolResult AIScriptUnbindFromNodeTool::execute(const Dictionary &p_arguments) {
	const String node_path = AIToolHelpers::get_stripped_string(p_arguments, "node_path");
	print_line(vformat("[AI Agent][Tool:script.unbind_from_node] Start. node_path=%s", node_path));
	if (node_path.is_empty()) {
		print_line("[AI Agent][Tool:script.unbind_from_node] Failed: missing required node_path.");
		return AIToolHelpers::make_missing_required_error("node_path");
	}

	AIScriptEditingResult edit_result = service->unbind_from_node(node_path);
	AIToolResult result = AIToolHelpers::from_editing_result(edit_result, "Failed to unbind script from node.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:script.unbind_from_node] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:script.unbind_from_node] Completed. %s", result.content));
	return result;
}
