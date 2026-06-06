/**************************************************************************/
/*  ai_script_patch_function_tool.cpp                                     */
/**************************************************************************/

#include "ai_script_patch_function_tool.h"

#include "editor/ai_component/tools/ai_tool_helpers.h"

AIScriptPatchFunctionTool::AIScriptPatchFunctionTool() {
	service.instantiate();
}

String AIScriptPatchFunctionTool::get_name() const {
	return "script.patch_function";
}

String AIScriptPatchFunctionTool::get_description() const {
	return "Uses the GDScript parser to replace an existing function by name or append it if allowed.";
}

Dictionary AIScriptPatchFunctionTool::get_parameters_schema() const {
	Dictionary properties;
	properties["path"] = AIToolHelpers::make_string_property("Target GDScript path under res://.");
	properties["function_name"] = AIToolHelpers::make_string_property("Function name to replace or append.");
	properties["function_source"] = AIToolHelpers::make_string_property("Complete function source starting with func or static func.");
	properties["create_if_missing"] = AIToolHelpers::make_boolean_property("Append the function when it does not already exist. Defaults to false.");

	Array required;
	required.push_back("path");
	required.push_back("function_name");
	required.push_back("function_source");
	return AIToolHelpers::make_object_schema(properties, required);
}

AIToolResult AIScriptPatchFunctionTool::execute(const Dictionary &p_arguments) {
	const String path = AIToolHelpers::get_stripped_string(p_arguments, "path");
	const String function_name = AIToolHelpers::get_stripped_string(p_arguments, "function_name");
	const String function_source = String(p_arguments.get("function_source", ""));
	const bool create_if_missing = AIToolHelpers::get_bool(p_arguments, "create_if_missing");
	print_line(vformat("[AI Agent][Tool:script.patch_function] Start. path=%s function=%s source_chars=%d create_if_missing=%s", path, function_name, function_source.length(), create_if_missing ? "yes" : "no"));
	if (path.is_empty()) {
		print_line("[AI Agent][Tool:script.patch_function] Failed: missing required path.");
		return AIToolHelpers::make_missing_required_error("path");
	}
	if (function_name.is_empty()) {
		print_line("[AI Agent][Tool:script.patch_function] Failed: missing required function_name.");
		return AIToolHelpers::make_missing_required_error("function_name");
	}
	if (function_source.strip_edges().is_empty()) {
		print_line("[AI Agent][Tool:script.patch_function] Failed: missing required function_source.");
		return AIToolHelpers::make_missing_required_error("function_source");
	}

	AIScriptEditingResult edit_result = service->patch_function(path, function_name, function_source, create_if_missing);
	AIToolResult result = AIToolHelpers::from_editing_result(edit_result, "Failed to patch function.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:script.patch_function] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:script.patch_function] Completed. %s", result.content));
	return result;
}
