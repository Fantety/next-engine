/**************************************************************************/
/*  ai_script_delete_tool.cpp                                             */
/**************************************************************************/

#include "ai_script_delete_tool.h"

AIScriptDeleteTool::AIScriptDeleteTool() {
	service.instantiate();
}

String AIScriptDeleteTool::get_name() const {
	return "script.delete";
}

String AIScriptDeleteTool::get_description() const {
	return "Deletes a GDScript file under res://. This tool must be explicitly approved by the user.";
}

Dictionary AIScriptDeleteTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary path_property;
	path_property["type"] = "string";
	path_property["description"] = "GDScript path under res:// to delete.";
	properties["path"] = path_property;

	Array required;
	required.push_back("path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIScriptDeleteTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String path = String(p_arguments.get("path", "")).strip_edges();
	print_line(vformat("[AI Agent][Tool:script.delete] Start. path=%s", path));
	if (path.is_empty()) {
		result.error = "Missing required path.";
		print_line("[AI Agent][Tool:script.delete] Failed: missing required path.");
		return result;
	}

	AIScriptEditingResult edit_result = service->delete_script(path);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to delete script.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:script.delete] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:script.delete] Completed. %s", result.content));
	return result;
}
