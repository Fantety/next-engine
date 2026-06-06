/**************************************************************************/
/*  ai_script_create_tool.cpp                                             */
/**************************************************************************/

#include "ai_script_create_tool.h"

#include "editor/ai_component/tools/ai_tool_helpers.h"

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
	Dictionary properties;
	properties["path"] = AIToolHelpers::make_string_property("Target GDScript path under res://.");
	properties["extends"] = AIToolHelpers::make_string_property("Optional base type used when source is omitted. Defaults to Node.");
	properties["source"] = AIToolHelpers::make_string_property("Optional full GDScript source. If omitted, a minimal template is created.");
	properties["overwrite"] = AIToolHelpers::make_boolean_property("Whether to overwrite an existing file. Defaults to false.");

	Array required;
	required.push_back("path");
	return AIToolHelpers::make_object_schema(properties, required);
}

AIToolResult AIScriptCreateTool::execute(const Dictionary &p_arguments) {
	const String path = AIToolHelpers::get_stripped_string(p_arguments, "path");
	const String extends_type = AIToolHelpers::get_stripped_string(p_arguments, "extends", "Node");
	const String source = String(p_arguments.get("source", ""));
	const bool overwrite = AIToolHelpers::get_bool(p_arguments, "overwrite");
	print_line(vformat("[AI Agent][Tool:script.create] Start. path=%s extends=%s overwrite=%s", path, extends_type, overwrite ? "yes" : "no"));
	if (path.is_empty()) {
		print_line("[AI Agent][Tool:script.create] Failed: missing required path.");
		return AIToolHelpers::make_missing_required_error("path");
	}

	AIScriptEditingResult edit_result = service->create_script(path, extends_type, source, overwrite);
	AIToolResult result = AIToolHelpers::from_editing_result(edit_result, "Failed to create script.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:script.create] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:script.create] Completed. %s", result.content));
	return result;
}
