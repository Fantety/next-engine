/**************************************************************************/
/*  ai_script_patch_function_tool.cpp                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "ai_script_patch_function_tool.h"

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"
#include "editor/next_file_logger.h"

AIV1ScriptPatchFunctionTool::AIV1ScriptPatchFunctionTool() {
	service.instantiate();
}

String AIV1ScriptPatchFunctionTool::get_name() const {
	return "script.patch_function";
}

String AIV1ScriptPatchFunctionTool::get_description() const {
	return "Uses the GDScript parser to replace an existing function by name or append it if allowed.";
}

Dictionary AIV1ScriptPatchFunctionTool::get_parameters_schema() const {
	Dictionary properties;
	properties["path"] = AIV1ToolHelpers::make_string_property("Target GDScript path under res://.");
	properties["function_name"] = AIV1ToolHelpers::make_string_property("Function name to replace or append.");
	properties["function_source"] = AIV1ToolHelpers::make_string_property("Complete function source starting with func or static func.");
	properties["create_if_missing"] = AIV1ToolHelpers::make_boolean_property("Append the function when it does not already exist. Defaults to false.");

	Array required;
	required.push_back("path");
	required.push_back("function_name");
	required.push_back("function_source");
	return AIV1ToolHelpers::make_object_schema(properties, required);
}

AIV1EditorToolResult AIV1ScriptPatchFunctionTool::execute_tool(const Dictionary &p_arguments) {
	const String path = AIV1ToolHelpers::get_stripped_string(p_arguments, "path");
	const String function_name = AIV1ToolHelpers::get_stripped_string(p_arguments, "function_name");
	const String function_source = String(p_arguments.get("function_source", ""));
	const bool create_if_missing = AIV1ToolHelpers::get_bool(p_arguments, "create_if_missing");
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:script.patch_function] Start. path=%s function=%s source_chars=%d create_if_missing=%s", path, function_name, function_source.length(), create_if_missing ? "yes" : "no"));
	if (path.is_empty()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:script.patch_function] Failed: missing required path.");
		return AIV1ToolHelpers::make_missing_required_error("path");
	}
	if (function_name.is_empty()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:script.patch_function] Failed: missing required function_name.");
		return AIV1ToolHelpers::make_missing_required_error("function_name");
	}
	if (function_source.strip_edges().is_empty()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:script.patch_function] Failed: missing required function_source.");
		return AIV1ToolHelpers::make_missing_required_error("function_source");
	}

	AIV1ScriptEditingResult edit_result = service->patch_function(path, function_name, function_source, create_if_missing);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to patch function.");
	if (result.is_error()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:script.patch_function] Failed: %s", result.error));
		return result;
	}

	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:script.patch_function] Completed. %s", result.content));
	return result;
}
