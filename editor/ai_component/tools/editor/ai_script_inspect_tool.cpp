/**************************************************************************/
/*  ai_script_inspect_tool.cpp                                            */
/**************************************************************************/

#include "ai_script_inspect_tool.h"

AIScriptInspectTool::AIScriptInspectTool() {
	service.instantiate();
}

String AIScriptInspectTool::get_name() const {
	return "script.inspect";
}

String AIScriptInspectTool::get_description() const {
	return "Parses a GDScript file and returns its functions and line ranges for safe local edits.";
}

Dictionary AIScriptInspectTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary path_property;
	path_property["type"] = "string";
	path_property["description"] = "GDScript path under res://, for example res://scripts/player.gd.";
	properties["path"] = path_property;

	Array required;
	required.push_back("path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIScriptInspectTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String path = String(p_arguments.get("path", "")).strip_edges();
	print_line(vformat("[AI Agent][Tool:script.inspect] Start. path=%s", path));
	if (path.is_empty()) {
		result.error = "Missing required path.";
		print_line("[AI Agent][Tool:script.inspect] Failed: missing required path.");
		return result;
	}

	AIScriptEditingResult edit_result = service->inspect_script(path);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to inspect script.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:script.inspect] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:script.inspect] Completed. %s", result.content));
	return result;
}
