/**************************************************************************/
/*  ai_create_folder_tool.cpp                                             */
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

#include "ai_create_folder_tool.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/variant/variant.h"
#include "editor/agent_v1/tools/project/ai_project_tool_utils.h"
#include "editor/next_file_logger.h"

String AIV1CreateFolderTool::get_name() const {
	return "project.create_folder";
}

String AIV1CreateFolderTool::get_description() const {
	return "Creates a project folder under res:// using Godot editor APIs.";
}

Dictionary AIV1CreateFolderTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary path_property;
	path_property["type"] = "string";
	path_property["description"] = "Folder path under res:// to create, for example res://scenes/ui/widgets.";
	properties["path"] = path_property;

	Array required;
	required.push_back("path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIV1EditorToolResult AIV1CreateFolderTool::execute_tool(const Dictionary &p_arguments) {
	AIV1EditorToolResult result;
	String path = AIV1ProjectToolUtils::get_path_argument(p_arguments, String());
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.create_folder] Start. path=%s", path));

	if (path.is_empty()) {
		result.error = "Missing required path.";
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:project.create_folder] Failed: missing required path.");
		return result;
	}
	if (!AIV1ProjectToolUtils::is_allowed_path(path)) {
		result.error = "Only res:// project paths without traversal are allowed.";
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.create_folder] Failed: path is outside allowed project boundary. path=%s", path));
		return result;
	}
	while (path.ends_with("/")) {
		path = path.trim_suffix("/");
	}
	if (path == "res://" || path.get_file().is_empty()) {
		result.error = "Folder path must include a folder name.";
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.create_folder] Failed: invalid folder path. path=%s", path));
		return result;
	}

	const String absolute_path = ProjectSettings::get_singleton()->globalize_path(path);
	Error err = DirAccess::make_dir_recursive_absolute(absolute_path);
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		result.error = vformat("Failed to create folder `%s` (error %d).", path, err);
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.create_folder] Failed: create_dir error=%d path=%s", err, path));
		return result;
	}

	AIV1ProjectToolUtils::refresh_editor_file_system(path, false);

	result.content = vformat("Created folder `%s`.", path);
	result.metadata["path"] = path;
	result.metadata["created"] = true;
	result.metadata["already_existed"] = (err == ERR_ALREADY_EXISTS);
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.create_folder] Completed. path=%s already_existed=%s", path, err == ERR_ALREADY_EXISTS ? "yes" : "no"));
	return result;
}
