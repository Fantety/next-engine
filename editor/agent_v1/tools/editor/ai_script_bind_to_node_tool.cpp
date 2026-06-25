/**************************************************************************/
/*  ai_script_bind_to_node_tool.cpp                                       */
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

#include "ai_script_bind_to_node_tool.h"

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"
#include "editor/next_file_logger.h"

AIV1ScriptBindToNodeTool::AIV1ScriptBindToNodeTool() {
	service.instantiate();
}

String AIV1ScriptBindToNodeTool::get_name() const {
	return "script.bind_to_node";
}

String AIV1ScriptBindToNodeTool::get_description() const {
	return "Binds a GDScript resource to a node in the currently edited scene using editor undo/redo APIs.";
}

Dictionary AIV1ScriptBindToNodeTool::get_parameters_schema() const {
	Dictionary properties;
	properties["node_path"] = AIV1ToolHelpers::make_string_property("Node path relative to the edited scene root. Use . for the root.");
	properties["script_path"] = AIV1ToolHelpers::make_string_property("GDScript path under res://.");

	Array required;
	required.push_back("node_path");
	required.push_back("script_path");
	return AIV1ToolHelpers::make_object_schema(properties, required);
}

AIV1EditorToolResult AIV1ScriptBindToNodeTool::execute_tool(const Dictionary &p_arguments) {
	const String node_path = AIV1ToolHelpers::get_stripped_string(p_arguments, "node_path");
	const String script_path = AIV1ToolHelpers::get_stripped_string(p_arguments, "script_path");
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:script.bind_to_node] Start. node_path=%s script_path=%s", node_path, script_path));
	if (node_path.is_empty()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:script.bind_to_node] Failed: missing required node_path.");
		return AIV1ToolHelpers::make_missing_required_error("node_path");
	}
	if (script_path.is_empty()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:script.bind_to_node] Failed: missing required script_path.");
		return AIV1ToolHelpers::make_missing_required_error("script_path");
	}

	AIV1ScriptEditingResult edit_result = service->bind_to_node(node_path, script_path);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to bind script to node.");
	if (result.is_error()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:script.bind_to_node] Failed: %s", result.error));
		return result;
	}

	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:script.bind_to_node] Completed. %s", result.content));
	return result;
}
