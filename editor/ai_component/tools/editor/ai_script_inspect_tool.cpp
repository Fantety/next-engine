/**************************************************************************/
/*  ai_script_inspect_tool.cpp                                            */
/**************************************************************************/

#include "ai_script_inspect_tool.h"

#include "editor/ai_component/tools/ai_tool_helpers.h"

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
	Dictionary properties;
	properties["path"] = AIToolHelpers::make_string_property("GDScript path under res://, for example res://scripts/player.gd.");

	Array required;
	required.push_back("path");
	return AIToolHelpers::make_object_schema(properties, required);
}

AIToolResult AIScriptInspectTool::execute(const Dictionary &p_arguments) {
	const String path = AIToolHelpers::get_stripped_string(p_arguments, "path");
	print_line(vformat("[AI Agent][Tool:script.inspect] Start. path=%s", path));
	if (path.is_empty()) {
		print_line("[AI Agent][Tool:script.inspect] Failed: missing required path.");
		return AIToolHelpers::make_missing_required_error("path");
	}

	AIScriptEditingResult edit_result = service->inspect_script(path);
	AIToolResult result = AIToolHelpers::from_editing_result(edit_result, "Failed to inspect script.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:script.inspect] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:script.inspect] Completed. %s", result.content));
	return result;
}
