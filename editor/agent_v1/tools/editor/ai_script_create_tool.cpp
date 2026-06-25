/**************************************************************************/
/*  ai_script_create_tool.cpp                                             */
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

#include "ai_script_create_tool.h"

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"
#include "editor/next_file_logger.h"

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
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:script.create] Start. path=%s extends=%s overwrite=%s", path, extends_type, overwrite ? "yes" : "no"));
	if (path.is_empty()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:script.create] Failed: missing required path.");
		return AIV1ToolHelpers::make_missing_required_error("path");
	}

	AIV1ScriptEditingResult edit_result = service->create_script(path, extends_type, source, overwrite);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to create script.");
	if (result.is_error()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:script.create] Failed: %s", result.error));
		return result;
	}

	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:script.create] Completed. %s", result.content));
	return result;
}
