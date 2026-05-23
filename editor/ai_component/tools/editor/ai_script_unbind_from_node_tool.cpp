/**************************************************************************/
/*  ai_script_unbind_from_node_tool.cpp                                   */
/**************************************************************************/

#include "ai_script_unbind_from_node_tool.h"

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
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary node_path_property;
	node_path_property["type"] = "string";
	node_path_property["description"] = "Node path relative to the edited scene root. Use . for the root.";
	properties["node_path"] = node_path_property;

	Array required;
	required.push_back("node_path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIScriptUnbindFromNodeTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String node_path = String(p_arguments.get("node_path", "")).strip_edges();
	print_line(vformat("[AI Agent][Tool:script.unbind_from_node] Start. node_path=%s", node_path));
	if (node_path.is_empty()) {
		result.error = "Missing required node_path.";
		print_line("[AI Agent][Tool:script.unbind_from_node] Failed: missing required node_path.");
		return result;
	}

	AIScriptEditingResult edit_result = service->unbind_from_node(node_path);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to unbind script from node.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:script.unbind_from_node] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:script.unbind_from_node] Completed. %s", result.content));
	return result;
}
