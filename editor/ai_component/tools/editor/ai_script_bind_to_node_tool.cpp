/**************************************************************************/
/*  ai_script_bind_to_node_tool.cpp                                       */
/**************************************************************************/

#include "ai_script_bind_to_node_tool.h"

AIScriptBindToNodeTool::AIScriptBindToNodeTool() {
	service.instantiate();
}

String AIScriptBindToNodeTool::get_name() const {
	return "script.bind_to_node";
}

String AIScriptBindToNodeTool::get_description() const {
	return "Binds a GDScript resource to a node in the currently edited scene using editor undo/redo APIs.";
}

Dictionary AIScriptBindToNodeTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary node_path_property;
	node_path_property["type"] = "string";
	node_path_property["description"] = "Node path relative to the edited scene root. Use . for the root.";
	properties["node_path"] = node_path_property;

	Dictionary script_path_property;
	script_path_property["type"] = "string";
	script_path_property["description"] = "GDScript path under res://.";
	properties["script_path"] = script_path_property;

	Array required;
	required.push_back("node_path");
	required.push_back("script_path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIScriptBindToNodeTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String node_path = String(p_arguments.get("node_path", "")).strip_edges();
	const String script_path = String(p_arguments.get("script_path", "")).strip_edges();
	print_line(vformat("[AI Agent][Tool:script.bind_to_node] Start. node_path=%s script_path=%s", node_path, script_path));
	if (node_path.is_empty()) {
		result.error = "Missing required node_path.";
		print_line("[AI Agent][Tool:script.bind_to_node] Failed: missing required node_path.");
		return result;
	}
	if (script_path.is_empty()) {
		result.error = "Missing required script_path.";
		print_line("[AI Agent][Tool:script.bind_to_node] Failed: missing required script_path.");
		return result;
	}

	AIScriptEditingResult edit_result = service->bind_to_node(node_path, script_path);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to bind script to node.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:script.bind_to_node] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:script.bind_to_node] Completed. %s", result.content));
	return result;
}
