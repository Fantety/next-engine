/**************************************************************************/
/*  ai_create_folder_tool.cpp                                             */
/**************************************************************************/

#include "ai_create_folder_tool.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/variant/variant.h"

#include "editor/ai_component/tools/project/ai_project_tool_utils.h"
#include "editor/file_system/editor_file_system.h"

String AICreateFolderTool::get_name() const {
	return "project.create_folder";
}

String AICreateFolderTool::get_description() const {
	return "Creates a project folder under res:// using Godot editor APIs.";
}

Dictionary AICreateFolderTool::get_parameters_schema() const {
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

AIToolResult AICreateFolderTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	String path = AIProjectToolUtils::get_path_argument(p_arguments, String());
	print_line(vformat("[AI Agent][Tool:project.create_folder] Start. path=%s", path));

	if (path.is_empty()) {
		result.error = "Missing required path.";
		print_line("[AI Agent][Tool:project.create_folder] Failed: missing required path.");
		return result;
	}
	if (!AIProjectToolUtils::is_allowed_path(path)) {
		result.error = "Only res:// project paths without traversal are allowed.";
		print_line(vformat("[AI Agent][Tool:project.create_folder] Failed: path is outside allowed project boundary. path=%s", path));
		return result;
	}
	while (path.ends_with("/")) {
		path = path.trim_suffix("/");
	}
	if (path == "res://" || path.get_file().is_empty()) {
		result.error = "Folder path must include a folder name.";
		print_line(vformat("[AI Agent][Tool:project.create_folder] Failed: invalid folder path. path=%s", path));
		return result;
	}

	const String absolute_path = ProjectSettings::get_singleton()->globalize_path(path);
	Error err = DirAccess::make_dir_recursive_absolute(absolute_path);
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		result.error = vformat("Failed to create folder `%s` (error %d).", path, err);
		print_line(vformat("[AI Agent][Tool:project.create_folder] Failed: create_dir error=%d path=%s", err, path));
		return result;
	}

	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->call_deferred("scan_changes");
	}

	result.content = vformat("Created folder `%s`.", path);
	result.metadata["path"] = path;
	result.metadata["created"] = true;
	result.metadata["already_existed"] = (err == ERR_ALREADY_EXISTS);
	print_line(vformat("[AI Agent][Tool:project.create_folder] Completed. path=%s already_existed=%s", path, err == ERR_ALREADY_EXISTS ? "yes" : "no"));
	return result;
}
