/**************************************************************************/
/*  ai_script_delete_tool.cpp                                             */
/**************************************************************************/

#include "ai_script_delete_tool.h"

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

AIV1ScriptDeleteTool::AIV1ScriptDeleteTool() {
	service.instantiate();
}

String AIV1ScriptDeleteTool::get_name() const {
	return "script.delete";
}

String AIV1ScriptDeleteTool::get_description() const {
	return "Deletes a GDScript file under res://. This tool must be explicitly approved by the user.";
}

Dictionary AIV1ScriptDeleteTool::get_parameters_schema() const {
	Dictionary properties;
	properties["path"] = AIV1ToolHelpers::make_string_property("GDScript path under res:// to delete.");

	Array required;
	required.push_back("path");
	return AIV1ToolHelpers::make_object_schema(properties, required);
}

AIV1EditorToolResult AIV1ScriptDeleteTool::execute_tool(const Dictionary &p_arguments) {
	const String path = AIV1ToolHelpers::get_stripped_string(p_arguments, "path");
	print_line(vformat("[AI Agent][Tool:script.delete] Start. path=%s", path));
	if (path.is_empty()) {
		print_line("[AI Agent][Tool:script.delete] Failed: missing required path.");
		return AIV1ToolHelpers::make_missing_required_error("path");
	}

	AIV1ScriptEditingResult edit_result = service->delete_script(path);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to delete script.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:script.delete] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:script.delete] Completed. %s", result.content));
	return result;
}
