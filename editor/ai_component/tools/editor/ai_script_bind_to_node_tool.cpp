/**************************************************************************/
/*  ai_script_bind_to_node_tool.cpp                                       */
/**************************************************************************/

#include "ai_script_bind_to_node_tool.h"

#include "editor/ai_component/tools/ai_tool_helpers.h"

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
	Dictionary properties;
	properties["node_path"] = AIToolHelpers::make_string_property("Node path relative to the edited scene root. Use . for the root.");
	properties["script_path"] = AIToolHelpers::make_string_property("GDScript path under res://.");

	Array required;
	required.push_back("node_path");
	required.push_back("script_path");
	return AIToolHelpers::make_object_schema(properties, required);
}

AIToolResult AIScriptBindToNodeTool::execute(const Dictionary &p_arguments) {
	const String node_path = AIToolHelpers::get_stripped_string(p_arguments, "node_path");
	const String script_path = AIToolHelpers::get_stripped_string(p_arguments, "script_path");
	print_line(vformat("[AI Agent][Tool:script.bind_to_node] Start. node_path=%s script_path=%s", node_path, script_path));
	if (node_path.is_empty()) {
		print_line("[AI Agent][Tool:script.bind_to_node] Failed: missing required node_path.");
		return AIToolHelpers::make_missing_required_error("node_path");
	}
	if (script_path.is_empty()) {
		print_line("[AI Agent][Tool:script.bind_to_node] Failed: missing required script_path.");
		return AIToolHelpers::make_missing_required_error("script_path");
	}

	AIScriptEditingResult edit_result = service->bind_to_node(node_path, script_path);
	AIToolResult result = AIToolHelpers::from_editing_result(edit_result, "Failed to bind script to node.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:script.bind_to_node] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:script.bind_to_node] Completed. %s", result.content));
	return result;
}
