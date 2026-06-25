/**************************************************************************/
/*  ai_delete_file_tool.cpp                                               */
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

#include "ai_delete_file_tool.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/variant/variant.h"
#include "editor/agent_v1/tools/project/ai_project_tool_utils.h"
#include "editor/editor_node.h"
#include "editor/next_file_logger.h"
#include "editor/script/script_editor_plugin.h"

String AIV1ProjectDeleteFileTool::get_name() const {
	return "project.delete_file";
}

String AIV1ProjectDeleteFileTool::get_description() const {
	return "Deletes a project file under res:// after checking whether it is open in the editor or referenced by scenes. This tool must be explicitly approved by the user.";
}

Dictionary AIV1ProjectDeleteFileTool::get_parameters_schema() const {
	Dictionary properties;
	properties["path"] = AIV1ToolHelpers::make_string_property("Project file path under res:// to delete. The tool refuses open files and files referenced by scenes.");

	Array required;
	required.push_back("path");
	return AIV1ToolHelpers::make_object_schema(properties, required);
}

bool AIV1ProjectDeleteFileTool::_is_scene_file(const String &p_path) {
	const String ext = p_path.get_extension().to_lower();
	return ext == "tscn" || ext == "scn";
}

String AIV1ProjectDeleteFileTool::_normalize_resource_path(const String &p_path) {
	String path = p_path.strip_edges();
	const int subresource_pos = path.find("::");
	if (subresource_pos >= 0) {
		path = path.substr(0, subresource_pos);
	}
	if (path.begins_with("res://")) {
		return path.simplify_path();
	}
	if (path.contains("://")) {
		return path;
	}
	return ProjectSettings::get_singleton()->localize_path(path).simplify_path();
}

bool AIV1ProjectDeleteFileTool::_dependency_matches_path(const String &p_dependency, const String &p_target_path) {
	const String normalized_target = _normalize_resource_path(p_target_path);
	const Vector<String> parts = p_dependency.split("::", true);
	for (const String &part : parts) {
		const String candidate = part.strip_edges();
		if (candidate.is_empty()) {
			continue;
		}
		if (_normalize_resource_path(candidate) == normalized_target) {
			return true;
		}
	}
	return false;
}

void AIV1ProjectDeleteFileTool::_collect_scene_files(const String &p_dir_path, Vector<String> &r_scene_paths) {
	Ref<DirAccess> dir = DirAccess::open(p_dir_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String entry = dir->get_next();
	while (!entry.is_empty()) {
		if (entry != "." && entry != ".." && !entry.begins_with(".")) {
			const String child_path = p_dir_path.path_join(entry);
			if (dir->current_is_dir()) {
				_collect_scene_files(child_path, r_scene_paths);
			} else if (_is_scene_file(child_path)) {
				r_scene_paths.push_back(child_path);
			}
		}
		entry = dir->get_next();
	}
	dir->list_dir_end();
}

bool AIV1ProjectDeleteFileTool::_find_open_file(const String &p_path, String &r_open_path) {
	const String normalized_path = _normalize_resource_path(p_path);

	if (EditorNode::get_singleton()) {
		const Vector<EditorData::EditedScene> edited_scenes = EditorNode::get_editor_data().get_edited_scenes();
		for (const EditorData::EditedScene &edited_scene : edited_scenes) {
			String scene_path = edited_scene.path;
			if (scene_path.is_empty() && edited_scene.root) {
				scene_path = edited_scene.root->get_scene_file_path();
			}
			if (!scene_path.is_empty() && _normalize_resource_path(scene_path) == normalized_path) {
				r_open_path = scene_path;
				return true;
			}
		}
	}

	ScriptEditor *script_editor = ScriptEditor::get_singleton();
	if (script_editor) {
		const Vector<Ref<Script>> open_scripts = script_editor->get_open_scripts();
		for (const Ref<Script> &script : open_scripts) {
			if (script.is_null()) {
				continue;
			}
			const String script_path = script->get_path();
			if (!script_path.is_empty() && _normalize_resource_path(script_path) == normalized_path) {
				r_open_path = script_path;
				return true;
			}
		}
	}

	return false;
}

void AIV1ProjectDeleteFileTool::_find_scene_references(const String &p_path, Vector<String> &r_referencing_scenes) {
	const String normalized_path = _normalize_resource_path(p_path);
	Vector<String> scene_paths;
	_collect_scene_files("res://", scene_paths);

	for (const String &scene_path : scene_paths) {
		if (_normalize_resource_path(scene_path) == normalized_path) {
			continue;
		}

		List<String> dependencies;
		ResourceLoader::get_dependencies(scene_path, &dependencies);
		for (const String &dependency : dependencies) {
			if (_dependency_matches_path(dependency, normalized_path)) {
				r_referencing_scenes.push_back(scene_path);
				break;
			}
		}
	}
}

Array AIV1ProjectDeleteFileTool::_vector_to_array(const Vector<String> &p_values) {
	Array output;
	for (const String &value : p_values) {
		output.push_back(value);
	}
	return output;
}

String AIV1ProjectDeleteFileTool::_format_path_list(const Vector<String> &p_paths, int p_max_count) {
	String output;
	const int count = MIN(p_paths.size(), p_max_count);
	for (int i = 0; i < count; i++) {
		if (i > 0) {
			output += ", ";
		}
		output += "`" + p_paths[i] + "`";
	}
	if (p_paths.size() > p_max_count) {
		output += vformat(", and %d more", p_paths.size() - p_max_count);
	}
	return output;
}

AIV1EditorToolResult AIV1ProjectDeleteFileTool::execute_tool(const Dictionary &p_arguments) {
	AIV1EditorToolResult result;
	String path = AIV1ProjectToolUtils::get_path_argument(p_arguments, String());
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.delete_file] Start. path=%s", path));

	if (path.is_empty()) {
		result.error = "Missing required path.";
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:project.delete_file] Failed: missing required path.");
		return result;
	}
	if (!AIV1ProjectToolUtils::is_allowed_path(path)) {
		result.error = "Only res:// project paths without traversal are allowed.";
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.delete_file] Failed: path is outside allowed project boundary. path=%s", path));
		return result;
	}

	path = path.simplify_path();
	if (path == "res://" || path.get_file().is_empty() || path.ends_with("/")) {
		result.error = "File path must include a file name.";
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.delete_file] Failed: invalid file path. path=%s", path));
		return result;
	}
	if (DirAccess::dir_exists_absolute(path)) {
		result.error = "project.delete_file deletes files only; folders are not supported.";
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.delete_file] Failed: path is a directory. path=%s", path));
		return result;
	}
	if (!FileAccess::exists(path)) {
		result.error = vformat("Project file `%s` does not exist.", path);
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.delete_file] Failed: file does not exist. path=%s", path));
		return result;
	}

	String open_path;
	if (_find_open_file(path, open_path)) {
		result.error = vformat("Project file `%s` is currently open in the editor as `%s`; close it before deleting.", path, open_path);
		result.metadata["path"] = path;
		result.metadata["open_path"] = open_path;
		result.metadata["blocked_reason"] = "open_file";
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.delete_file] Failed: file is open. path=%s open_path=%s", path, open_path));
		return result;
	}

	Vector<String> referencing_scenes;
	_find_scene_references(path, referencing_scenes);
	if (!referencing_scenes.is_empty()) {
		result.error = vformat("Project file `%s` is referenced by scene(s): %s. Remove those references before deleting.", path, _format_path_list(referencing_scenes));
		result.metadata["path"] = path;
		result.metadata["referencing_scenes"] = _vector_to_array(referencing_scenes);
		result.metadata["reference_count"] = referencing_scenes.size();
		result.metadata["blocked_reason"] = "scene_references";
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.delete_file] Failed: referenced by %d scene(s). path=%s", referencing_scenes.size(), path));
		return result;
	}

	Error err = DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));
	if (err != OK) {
		result.error = vformat("Failed to delete project file `%s` (error %d).", path, err);
		result.metadata["path"] = path;
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.delete_file] Failed: delete error=%d path=%s", err, path));
		return result;
	}

	AIV1ProjectToolUtils::refresh_editor_file_system(path, true);

	result.content = vformat("Deleted project file `%s`.", path);
	result.metadata["path"] = path;
	result.metadata["deleted"] = true;
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:project.delete_file] Completed. path=%s", path));
	return result;
}
