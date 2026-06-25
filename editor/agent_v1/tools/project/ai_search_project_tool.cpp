/**************************************************************************/
/*  ai_search_project_tool.cpp                                            */
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

#include "ai_search_project_tool.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/variant/variant.h"
#include "editor/agent_v1/tools/project/ai_project_tool_utils.h"
#include "editor/next_file_logger.h"

String AIV1SearchProjectTool::get_name() const {
	return "project.search_text";
}

String AIV1SearchProjectTool::get_description() const {
	return "Searches allowlisted text files under res:// using literal substring matching.";
}

Dictionary AIV1SearchProjectTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary query_property;
	query_property["type"] = "string";
	query_property["description"] = "Literal text to search for.";
	properties["query"] = query_property;

	Dictionary path_property;
	path_property["type"] = "string";
	path_property["description"] = "Project path under res:// to search.";
	properties["path"] = path_property;

	Dictionary max_results_property;
	max_results_property["type"] = "integer";
	max_results_property["description"] = "Maximum number of matching lines.";
	properties["max_results"] = max_results_property;

	Array required;
	required.push_back("query");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIV1EditorToolResult AIV1SearchProjectTool::execute_tool(const Dictionary &p_arguments) {
	AIV1EditorToolResult result;
	query = String(p_arguments.get("query", "")).strip_edges();
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.search_text] Start. query_chars=%d", query.length()));
	if (query.is_empty()) {
		result.error = "Missing required query.";
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:project.search_text] Failed: missing required query.");
		return result;
	}

	String path = AIV1ProjectToolUtils::get_path_argument(p_arguments);
	if (!AIV1ProjectToolUtils::is_allowed_path(path)) {
		result.error = "Only res:// project paths without traversal are allowed.";
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.search_text] Failed: path is outside allowed project boundary. path=%s", path));
		return result;
	}
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.search_text] Searching. path=%s max_results=%d", path, AIV1ProjectToolUtils::get_int_argument(p_arguments, "max_results", 50, 1, 200)));

	max_results = AIV1ProjectToolUtils::get_int_argument(p_arguments, "max_results", 50, 1, 200);
	max_chars = AIV1ProjectToolUtils::get_int_argument(p_arguments, "max_chars", 4096, 1000, 32000);
	result_count = 0;
	truncated = false;

	String output;
	_search_dir(output, path);
	if (output.is_empty()) {
		output = "No matches.";
	}
	if (truncated) {
		output += "\n[truncated]";
	}

	result.content = output.substr(0, max_chars);
	result.truncated = truncated || output.length() > max_chars;
	result.metadata["path"] = path;
	result.metadata["query"] = query;
	result.metadata["result_count"] = result_count;
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.search_text] Completed. path=%s results=%d truncated=%s", path, result_count, result.truncated ? "yes" : "no"));
	return result;
}

void AIV1SearchProjectTool::_search_dir(String &r_output, const String &p_path) {
	if (result_count >= max_results || r_output.length() >= max_chars) {
		truncated = true;
		return;
	}

	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String entry = dir->get_next();
	while (!entry.is_empty()) {
		if (entry != "." && entry != ".." && !entry.begins_with(".")) {
			String child_path = p_path.path_join(entry);
			if (dir->current_is_dir()) {
				_search_dir(r_output, child_path);
			} else if (AIV1ProjectToolUtils::is_allowed_text_extension(child_path)) {
				_search_file(r_output, child_path);
			}

			if (result_count >= max_results || r_output.length() >= max_chars) {
				truncated = true;
				break;
			}
		}
		entry = dir->get_next();
	}
	dir->list_dir_end();
}

void AIV1SearchProjectTool::_search_file(String &r_output, const String &p_path) {
	if (!FileAccess::exists(p_path)) {
		return;
	}

	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		return;
	}

	Vector<String> lines = file->get_as_text().split("\n");
	for (int i = 0; i < lines.size(); i++) {
		if (lines[i].contains(query)) {
			String preview = lines[i].strip_edges();
			if (preview.length() > 160) {
				preview = preview.substr(0, 157) + "...";
			}
			r_output += vformat("%s:%d: %s\n", p_path, i + 1, preview);
			result_count++;
			if (result_count >= max_results || r_output.length() >= max_chars) {
				truncated = true;
				return;
			}
		}
	}
}
