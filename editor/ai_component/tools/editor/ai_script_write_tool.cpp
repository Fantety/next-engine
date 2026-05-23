/**************************************************************************/
/*  ai_script_write_tool.cpp                                              */
/**************************************************************************/

#include "ai_script_write_tool.h"

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
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary path_property;
	path_property["type"] = "string";
	path_property["description"] = "Target GDScript path under res://.";
	properties["path"] = path_property;

	Dictionary source_property;
	source_property["type"] = "string";
	source_property["description"] = "Complete GDScript source to write.";
	properties["source"] = source_property;

	Dictionary overwrite_property;
	overwrite_property["type"] = "boolean";
	overwrite_property["description"] = "Whether to overwrite an existing file. Defaults to false.";
	properties["overwrite"] = overwrite_property;

	Array required;
	required.push_back("path");
	required.push_back("source");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIScriptWriteTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String path = String(p_arguments.get("path", "")).strip_edges();
	const String source = String(p_arguments.get("source", ""));
	const bool overwrite = bool(p_arguments.get("overwrite", false));
	print_line(vformat("[AI Agent][Tool:script.write] Start. path=%s source_chars=%d overwrite=%s", path, source.length(), overwrite ? "yes" : "no"));
	if (path.is_empty()) {
		result.error = "Missing required path.";
		print_line("[AI Agent][Tool:script.write] Failed: missing required path.");
		return result;
	}
	if (source.strip_edges().is_empty()) {
		result.error = "Missing required source.";
		print_line("[AI Agent][Tool:script.write] Failed: missing required source.");
		return result;
	}

	AIScriptEditingResult edit_result = service->write_script(path, source, overwrite);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to write script.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:script.write] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:script.write] Completed. %s", result.content));
	return result;
}
