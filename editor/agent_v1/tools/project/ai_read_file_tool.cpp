/**************************************************************************/
/*  ai_read_file_tool.cpp                                                 */
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

#include "ai_read_file_tool.h"

#include "core/io/file_access.h"
#include "core/variant/variant.h"
#include "editor/agent_v1/tools/project/ai_project_tool_utils.h"
#include "editor/next_file_logger.h"

String AIV1ProjectReadFileTool::get_name() const {
	return "project.read_file";
}

String AIV1ProjectReadFileTool::get_description() const {
	return "Reads an allowlisted text file under res:// with a byte limit.";
}

Dictionary AIV1ProjectReadFileTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary path_property;
	path_property["type"] = "string";
	path_property["description"] = "Text file path under res://.";
	properties["path"] = path_property;

	Dictionary max_bytes_property;
	max_bytes_property["type"] = "integer";
	max_bytes_property["description"] = "Maximum number of bytes to return.";
	properties["max_bytes"] = max_bytes_property;

	Array required;
	required.push_back("path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIV1EditorToolResult AIV1ProjectReadFileTool::execute_tool(const Dictionary &p_arguments) {
	AIV1EditorToolResult result;
	String path = AIV1ProjectToolUtils::get_path_argument(p_arguments, String());
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.read_file] Start. path=%s", path));
	if (path.is_empty()) {
		result.error = "Missing required path.";
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:project.read_file] Failed: missing required path.");
		return result;
	}
	if (!AIV1ProjectToolUtils::is_allowed_path(path)) {
		result.error = "Only res:// project paths without traversal are allowed.";
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.read_file] Failed: path is outside allowed project boundary. path=%s", path));
		return result;
	}
	if (!AIV1ProjectToolUtils::is_allowed_text_extension(path)) {
		result.error = "File extension is not in the text allowlist.";
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.read_file] Failed: extension is not allowlisted. path=%s", path));
		return result;
	}
	if (!FileAccess::exists(path)) {
		result.error = "File does not exist.";
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.read_file] Failed: file does not exist. path=%s", path));
		return result;
	}

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		result.error = "Failed to open file.";
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.read_file] Failed: could not open file. path=%s", path));
		return result;
	}

	int max_bytes = AIV1ProjectToolUtils::get_int_argument(p_arguments, "max_bytes", 65536, 1024, 262144);
	uint64_t length = file->get_length();
	String text = file->get_as_text();
	if (text.length() > max_bytes) {
		text = text.substr(0, max_bytes);
		result.truncated = true;
	} else if (length > (uint64_t)max_bytes) {
		result.truncated = true;
	}

	result.content = text;
	result.metadata["path"] = path;
	result.metadata["bytes"] = (int)MIN(length, (uint64_t)max_bytes);
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.read_file] Completed. path=%s bytes=%d truncated=%s", path, (int)MIN(length, (uint64_t)max_bytes), result.truncated ? "yes" : "no"));
	return result;
}
