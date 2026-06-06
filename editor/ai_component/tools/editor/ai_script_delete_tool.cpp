/**************************************************************************/
/*  ai_script_delete_tool.cpp                                             */
/**************************************************************************/

#include "ai_script_delete_tool.h"

#include "editor/ai_component/tools/ai_tool_helpers.h"

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
	Dictionary properties;
	properties["path"] = AIToolHelpers::make_string_property("GDScript path under res:// to delete.");

	Array required;
	required.push_back("path");
	return AIToolHelpers::make_object_schema(properties, required);
}

AIToolResult AIScriptDeleteTool::execute(const Dictionary &p_arguments) {
	const String path = AIToolHelpers::get_stripped_string(p_arguments, "path");
	print_line(vformat("[AI Agent][Tool:script.delete] Start. path=%s", path));
	if (path.is_empty()) {
		print_line("[AI Agent][Tool:script.delete] Failed: missing required path.");
		return AIToolHelpers::make_missing_required_error("path");
	}

	AIScriptEditingResult edit_result = service->delete_script(path);
	AIToolResult result = AIToolHelpers::from_editing_result(edit_result, "Failed to delete script.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:script.delete] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:script.delete] Completed. %s", result.content));
	return result;
}
