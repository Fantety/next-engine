/**************************************************************************/
/*  ai_shader_create_tool.cpp                                             */
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

#include "ai_shader_create_tool.h"

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"
#include "editor/next_file_logger.h"

AIV1ShaderCreateTool::AIV1ShaderCreateTool() {
	service.instantiate();
}

String AIV1ShaderCreateTool::get_name() const {
	return "shader.create";
}

String AIV1ShaderCreateTool::get_description() const {
	return "Creates a .gdshader resource through the editor resource save flow.";
}

Dictionary AIV1ShaderCreateTool::get_parameters_schema() const {
	Dictionary properties;
	properties["path"] = AIV1ToolHelpers::make_string_property("Target .gdshader path under res://.");
	properties["shader_type"] = AIV1ToolHelpers::make_string_property("Shader type used when shader_code is omitted. Defaults to canvas_item.");
	properties["shader_code"] = AIV1ToolHelpers::make_string_property("Optional initial Godot shader source. Include the shader_type line if provided.");
	properties["overwrite"] = AIV1ToolHelpers::make_boolean_property("Whether to overwrite an existing shader. Defaults to false.");

	Array required;
	required.push_back("path");
	return AIV1ToolHelpers::make_object_schema(properties, required);
}

AIV1EditorToolResult AIV1ShaderCreateTool::execute_tool(const Dictionary &p_arguments) {
	const String path = AIV1ToolHelpers::get_stripped_string(p_arguments, "path");
	const String shader_type = AIV1ToolHelpers::get_stripped_string(p_arguments, "shader_type", "canvas_item");
	const String shader_code = String(p_arguments.get("shader_code", ""));
	const bool overwrite = AIV1ToolHelpers::get_bool(p_arguments, "overwrite");
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:shader.create] Start. path=%s shader_type=%s overwrite=%s", path, shader_type, overwrite ? "yes" : "no"));

	if (path.is_empty()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:shader.create] Failed: missing required path.");
		return AIV1ToolHelpers::make_missing_required_error("path");
	}

	AIV1ShaderEditingResult edit_result = service->create_shader(path, shader_type, shader_code, overwrite);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to create shader.");
	if (result.is_error()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:shader.create] Failed: %s", result.error));
		return result;
	}

	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:shader.create] Completed. %s", result.content));
	return result;
}
