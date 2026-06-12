/**************************************************************************/
/*  ai_script_inspect_tool.cpp                                            */
/**************************************************************************/

#include "ai_script_inspect_tool.h"

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

AIV1ScriptInspectTool::AIV1ScriptInspectTool() {
	service.instantiate();
}

String AIV1ScriptInspectTool::get_name() const {
	return "script.inspect";
}

String AIV1ScriptInspectTool::get_description() const {
	return "Parses a GDScript file and returns its functions and line ranges for safe local edits.";
}

Dictionary AIV1ScriptInspectTool::get_parameters_schema() const {
	Dictionary properties;
	properties["path"] = AIV1ToolHelpers::make_string_property("GDScript path under res://, for example res://scripts/player.gd.");

	Array required;
	required.push_back("path");
	return AIV1ToolHelpers::make_object_schema(properties, required);
}

AIV1EditorToolResult AIV1ScriptInspectTool::execute_tool(const Dictionary &p_arguments) {
	const String path = AIV1ToolHelpers::get_stripped_string(p_arguments, "path");
	print_line(vformat("[AI Agent][Tool:script.inspect] Start. path=%s", path));
	if (path.is_empty()) {
		print_line("[AI Agent][Tool:script.inspect] Failed: missing required path.");
		return AIV1ToolHelpers::make_missing_required_error("path");
	}

	AIV1ScriptEditingResult edit_result = service->inspect_script(path);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to inspect script.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:script.inspect] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:script.inspect] Completed. %s", result.content));
	return result;
}
