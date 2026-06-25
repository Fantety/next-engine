/**************************************************************************/
/*  ai_script_delete_tool.cpp                                             */
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

#include "ai_script_delete_tool.h"

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"
#include "editor/next_file_logger.h"

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
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:script.delete] Start. path=%s", path));
	if (path.is_empty()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:script.delete] Failed: missing required path.");
		return AIV1ToolHelpers::make_missing_required_error("path");
	}

	AIV1ScriptEditingResult edit_result = service->delete_script(path);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to delete script.");
	if (result.is_error()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:script.delete] Failed: %s", result.error));
		return result;
	}

	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:script.delete] Completed. %s", result.content));
	return result;
}
