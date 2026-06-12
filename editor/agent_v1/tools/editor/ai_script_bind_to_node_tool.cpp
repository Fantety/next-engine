/**************************************************************************/
/*  ai_script_bind_to_node_tool.cpp                                       */
/**************************************************************************/

#include "ai_script_bind_to_node_tool.h"

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

AIV1ScriptBindToNodeTool::AIV1ScriptBindToNodeTool() {
	service.instantiate();
}

String AIV1ScriptBindToNodeTool::get_name() const {
	return "script.bind_to_node";
}

String AIV1ScriptBindToNodeTool::get_description() const {
	return "Binds a GDScript resource to a node in the currently edited scene using editor undo/redo APIs.";
}

Dictionary AIV1ScriptBindToNodeTool::get_parameters_schema() const {
	Dictionary properties;
	properties["node_path"] = AIV1ToolHelpers::make_string_property("Node path relative to the edited scene root. Use . for the root.");
	properties["script_path"] = AIV1ToolHelpers::make_string_property("GDScript path under res://.");

	Array required;
	required.push_back("node_path");
	required.push_back("script_path");
	return AIV1ToolHelpers::make_object_schema(properties, required);
}

AIV1EditorToolResult AIV1ScriptBindToNodeTool::execute_tool(const Dictionary &p_arguments) {
	const String node_path = AIV1ToolHelpers::get_stripped_string(p_arguments, "node_path");
	const String script_path = AIV1ToolHelpers::get_stripped_string(p_arguments, "script_path");
	print_line(vformat("[AI Agent][Tool:script.bind_to_node] Start. node_path=%s script_path=%s", node_path, script_path));
	if (node_path.is_empty()) {
		print_line("[AI Agent][Tool:script.bind_to_node] Failed: missing required node_path.");
		return AIV1ToolHelpers::make_missing_required_error("node_path");
	}
	if (script_path.is_empty()) {
		print_line("[AI Agent][Tool:script.bind_to_node] Failed: missing required script_path.");
		return AIV1ToolHelpers::make_missing_required_error("script_path");
	}

	AIV1ScriptEditingResult edit_result = service->bind_to_node(node_path, script_path);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to bind script to node.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:script.bind_to_node] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:script.bind_to_node] Completed. %s", result.content));
	return result;
}
