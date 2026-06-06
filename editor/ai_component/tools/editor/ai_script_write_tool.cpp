/**************************************************************************/
/*  ai_script_write_tool.cpp                                              */
/**************************************************************************/

#include "ai_script_write_tool.h"

#include "editor/ai_component/tools/ai_tool_helpers.h"

AIScriptWriteTool::AIScriptWriteTool() {
	service.instantiate();
}

String AIScriptWriteTool::get_name() const {
	return "script.write";
}

String AIScriptWriteTool::get_description() const {
	return "Writes a complete GDScript file after parsing and validating the new source.";
}

Dictionary AIScriptWriteTool::get_parameters_schema() const {
	Dictionary properties;
	properties["path"] = AIToolHelpers::make_string_property("Target GDScript path under res://.");
	properties["source"] = AIToolHelpers::make_string_property("Complete GDScript source to write.");
	properties["overwrite"] = AIToolHelpers::make_boolean_property("Whether to overwrite an existing file. Defaults to false.");

	Array required;
	required.push_back("path");
	required.push_back("source");
	return AIToolHelpers::make_object_schema(properties, required);
}

AIToolResult AIScriptWriteTool::execute(const Dictionary &p_arguments) {
	const String path = AIToolHelpers::get_stripped_string(p_arguments, "path");
	const String source = String(p_arguments.get("source", ""));
	const bool overwrite = AIToolHelpers::get_bool(p_arguments, "overwrite");
	print_line(vformat("[AI Agent][Tool:script.write] Start. path=%s source_chars=%d overwrite=%s", path, source.length(), overwrite ? "yes" : "no"));
	if (path.is_empty()) {
		print_line("[AI Agent][Tool:script.write] Failed: missing required path.");
		return AIToolHelpers::make_missing_required_error("path");
	}
	if (source.strip_edges().is_empty()) {
		print_line("[AI Agent][Tool:script.write] Failed: missing required source.");
		return AIToolHelpers::make_missing_required_error("source");
	}

	AIScriptEditingResult edit_result = service->write_script(path, source, overwrite);
	AIToolResult result = AIToolHelpers::from_editing_result(edit_result, "Failed to write script.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:script.write] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:script.write] Completed. %s", result.content));
	return result;
}
