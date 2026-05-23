/**************************************************************************/
/*  ai_script_patch_function_tool.cpp                                     */
/**************************************************************************/

#include "ai_script_patch_function_tool.h"

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
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary path_property;
	path_property["type"] = "string";
	path_property["description"] = "Target GDScript path under res://.";
	properties["path"] = path_property;

	Dictionary function_name_property;
	function_name_property["type"] = "string";
	function_name_property["description"] = "Function name to replace or append.";
	properties["function_name"] = function_name_property;

	Dictionary function_source_property;
	function_source_property["type"] = "string";
	function_source_property["description"] = "Complete function source starting with func or static func.";
	properties["function_source"] = function_source_property;

	Dictionary create_property;
	create_property["type"] = "boolean";
	create_property["description"] = "Append the function when it does not already exist. Defaults to false.";
	properties["create_if_missing"] = create_property;

	Array required;
	required.push_back("path");
	required.push_back("function_name");
	required.push_back("function_source");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIScriptPatchFunctionTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String path = String(p_arguments.get("path", "")).strip_edges();
	const String function_name = String(p_arguments.get("function_name", "")).strip_edges();
	const String function_source = String(p_arguments.get("function_source", ""));
	const bool create_if_missing = bool(p_arguments.get("create_if_missing", false));
	print_line(vformat("[AI Agent][Tool:script.patch_function] Start. path=%s function=%s source_chars=%d create_if_missing=%s", path, function_name, function_source.length(), create_if_missing ? "yes" : "no"));
	if (path.is_empty()) {
		result.error = "Missing required path.";
		print_line("[AI Agent][Tool:script.patch_function] Failed: missing required path.");
		return result;
	}
	if (function_name.is_empty()) {
		result.error = "Missing required function_name.";
		print_line("[AI Agent][Tool:script.patch_function] Failed: missing required function_name.");
		return result;
	}
	if (function_source.strip_edges().is_empty()) {
		result.error = "Missing required function_source.";
		print_line("[AI Agent][Tool:script.patch_function] Failed: missing required function_source.");
		return result;
	}

	AIScriptEditingResult edit_result = service->patch_function(path, function_name, function_source, create_if_missing);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to patch function.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:script.patch_function] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:script.patch_function] Completed. %s", result.content));
	return result;
}
