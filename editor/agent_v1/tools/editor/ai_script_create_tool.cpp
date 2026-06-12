/**************************************************************************/
/*  ai_script_create_tool.cpp                                             */
/**************************************************************************/

#include "ai_script_create_tool.h"

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

AIV1ScriptCreateTool::AIV1ScriptCreateTool() {
	service.instantiate();
}

String AIV1ScriptCreateTool::get_name() const {
	return "script.create";
}

String AIV1ScriptCreateTool::get_description() const {
	return "Creates a GDScript file under res:// using explicit source or a minimal extends template.";
}

Dictionary AIV1ScriptCreateTool::get_parameters_schema() const {
	Dictionary properties;
	properties["path"] = AIV1ToolHelpers::make_string_property("Target GDScript path under res://.");
	properties["extends"] = AIV1ToolHelpers::make_string_property("Optional base type used when source is omitted. Defaults to Node.");
	properties["source"] = AIV1ToolHelpers::make_string_property("Optional full GDScript source. If omitted, a minimal template is created.");
	properties["overwrite"] = AIV1ToolHelpers::make_boolean_property("Whether to overwrite an existing file. Defaults to false.");

	Array required;
	required.push_back("path");
	return AIV1ToolHelpers::make_object_schema(properties, required);
}

AIV1EditorToolResult AIV1ScriptCreateTool::execute_tool(const Dictionary &p_arguments) {
	const String path = AIV1ToolHelpers::get_stripped_string(p_arguments, "path");
	const String extends_type = AIV1ToolHelpers::get_stripped_string(p_arguments, "extends", "Node");
	const String source = String(p_arguments.get("source", ""));
	const bool overwrite = AIV1ToolHelpers::get_bool(p_arguments, "overwrite");
	print_line(vformat("[AI Agent][Tool:script.create] Start. path=%s extends=%s overwrite=%s", path, extends_type, overwrite ? "yes" : "no"));
	if (path.is_empty()) {
		print_line("[AI Agent][Tool:script.create] Failed: missing required path.");
		return AIV1ToolHelpers::make_missing_required_error("path");
	}

	AIV1ScriptEditingResult edit_result = service->create_script(path, extends_type, source, overwrite);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to create script.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:script.create] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:script.create] Completed. %s", result.content));
	return result;
}
