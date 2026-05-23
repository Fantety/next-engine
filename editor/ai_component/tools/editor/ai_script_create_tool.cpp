/**************************************************************************/
/*  ai_script_create_tool.cpp                                             */
/**************************************************************************/

#include "ai_script_create_tool.h"

AIScriptCreateTool::AIScriptCreateTool() {
	service.instantiate();
}

String AIScriptCreateTool::get_name() const {
	return "script.create";
}

String AIScriptCreateTool::get_description() const {
	return "Creates a GDScript file under res:// using explicit source or a minimal extends template.";
}

Dictionary AIScriptCreateTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary path_property;
	path_property["type"] = "string";
	path_property["description"] = "Target GDScript path under res://.";
	properties["path"] = path_property;

	Dictionary extends_property;
	extends_property["type"] = "string";
	extends_property["description"] = "Optional base type used when source is omitted. Defaults to Node.";
	properties["extends"] = extends_property;

	Dictionary source_property;
	source_property["type"] = "string";
	source_property["description"] = "Optional full GDScript source. If omitted, a minimal template is created.";
	properties["source"] = source_property;

	Dictionary overwrite_property;
	overwrite_property["type"] = "boolean";
	overwrite_property["description"] = "Whether to overwrite an existing file. Defaults to false.";
	properties["overwrite"] = overwrite_property;

	Array required;
	required.push_back("path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIScriptCreateTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String path = String(p_arguments.get("path", "")).strip_edges();
	const String extends_type = String(p_arguments.get("extends", "Node")).strip_edges();
	const String source = String(p_arguments.get("source", ""));
	const bool overwrite = bool(p_arguments.get("overwrite", false));
	print_line(vformat("[AI Agent][Tool:script.create] Start. path=%s extends=%s overwrite=%s", path, extends_type, overwrite ? "yes" : "no"));
	if (path.is_empty()) {
		result.error = "Missing required path.";
		print_line("[AI Agent][Tool:script.create] Failed: missing required path.");
		return result;
	}

	AIScriptEditingResult edit_result = service->create_script(path, extends_type, source, overwrite);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to create script.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:script.create] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:script.create] Completed. %s", result.content));
	return result;
}
