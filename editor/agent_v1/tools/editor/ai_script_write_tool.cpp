/**************************************************************************/
/*  ai_script_write_tool.cpp                                              */
/**************************************************************************/

#include "ai_script_write_tool.h"

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

AIV1ScriptWriteTool::AIV1ScriptWriteTool() {
	service.instantiate();
}

String AIV1ScriptWriteTool::get_name() const {
	return "script.write";
}

String AIV1ScriptWriteTool::get_description() const {
	return "Writes a complete GDScript file after parsing and validating the new source.";
}

Dictionary AIV1ScriptWriteTool::get_parameters_schema() const {
	Dictionary properties;
	properties["path"] = AIV1ToolHelpers::make_string_property("Target GDScript path under res://.");
	properties["source"] = AIV1ToolHelpers::make_string_property("Complete GDScript source to write.");
	properties["overwrite"] = AIV1ToolHelpers::make_boolean_property("Whether to overwrite an existing file. Defaults to false.");

	Array required;
	required.push_back("path");
	required.push_back("source");
	return AIV1ToolHelpers::make_object_schema(properties, required);
}

AIV1EditorToolResult AIV1ScriptWriteTool::execute_tool(const Dictionary &p_arguments) {
	const String path = AIV1ToolHelpers::get_stripped_string(p_arguments, "path");
	const String source = String(p_arguments.get("source", ""));
	const bool overwrite = AIV1ToolHelpers::get_bool(p_arguments, "overwrite");
	print_line(vformat("[AI Agent][Tool:script.write] Start. path=%s source_chars=%d overwrite=%s", path, source.length(), overwrite ? "yes" : "no"));
	if (path.is_empty()) {
		print_line("[AI Agent][Tool:script.write] Failed: missing required path.");
		return AIV1ToolHelpers::make_missing_required_error("path");
	}
	if (source.strip_edges().is_empty()) {
		print_line("[AI Agent][Tool:script.write] Failed: missing required source.");
		return AIV1ToolHelpers::make_missing_required_error("source");
	}

	AIV1ScriptEditingResult edit_result = service->write_script(path, source, overwrite);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to write script.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:script.write] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:script.write] Completed. %s", result.content));
	return result;
}
