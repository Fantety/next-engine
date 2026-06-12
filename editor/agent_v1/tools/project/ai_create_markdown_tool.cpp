/**************************************************************************/
/*  ai_create_markdown_tool.cpp                                           */
/**************************************************************************/

#include "ai_create_markdown_tool.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/variant/variant.h"

#include "editor/agent_v1/tools/project/ai_project_tool_utils.h"

String AIV1CreateMarkdownTool::get_name() const {
	return "project.create_markdown";
}

String AIV1CreateMarkdownTool::get_description() const {
	return "Creates or overwrites a Markdown document under res://.";
}

Dictionary AIV1CreateMarkdownTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary path_property;
	path_property["type"] = "string";
	path_property["description"] = "Target Markdown document path under res://. Must end with .md.";
	properties["path"] = path_property;

	Dictionary content_property;
	content_property["type"] = "string";
	content_property["description"] = "Complete Markdown document content to write.";
	properties["content"] = content_property;

	Dictionary overwrite_property;
	overwrite_property["type"] = "boolean";
	overwrite_property["description"] = "Whether to overwrite an existing Markdown document. Defaults to false.";
	properties["overwrite"] = overwrite_property;

	Array required;
	required.push_back("path");
	required.push_back("content");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIV1EditorToolResult AIV1CreateMarkdownTool::execute_tool(const Dictionary &p_arguments) {
	AIV1EditorToolResult result;
	const String path = AIV1ProjectToolUtils::get_path_argument(p_arguments, String());
	const String content = String(p_arguments.get("content", ""));
	const bool overwrite = bool(p_arguments.get("overwrite", false));
	print_line(vformat("[AI Agent][Tool:project.create_markdown] Start. path=%s content_chars=%d overwrite=%s", path, content.length(), overwrite ? "yes" : "no"));

	if (path.is_empty()) {
		result.error = "Missing required path.";
		print_line("[AI Agent][Tool:project.create_markdown] Failed: missing required path.");
		return result;
	}
	if (content.strip_edges().is_empty()) {
		result.error = "Missing required content.";
		print_line("[AI Agent][Tool:project.create_markdown] Failed: missing required content.");
		return result;
	}
	if (!AIV1ProjectToolUtils::is_allowed_path(path)) {
		result.error = "Only res:// project paths without traversal are allowed.";
		print_line(vformat("[AI Agent][Tool:project.create_markdown] Failed: path is outside allowed project boundary. path=%s", path));
		return result;
	}
	if (path.ends_with("/") || path.get_file().is_empty()) {
		result.error = "Markdown path must include a file name.";
		print_line(vformat("[AI Agent][Tool:project.create_markdown] Failed: invalid file path. path=%s", path));
		return result;
	}
	if (path.get_extension().to_lower() != "md") {
		result.error = "Only Markdown `.md` files are supported by this tool.";
		print_line(vformat("[AI Agent][Tool:project.create_markdown] Failed: unsupported extension. path=%s", path));
		return result;
	}

	const bool existed_before = FileAccess::exists(path);
	if (existed_before && !overwrite) {
		result.error = vformat("Markdown document `%s` already exists. Set overwrite=true to replace it.", path);
		print_line(vformat("[AI Agent][Tool:project.create_markdown] Failed: file already exists. path=%s", path));
		return result;
	}

	const String base_dir = path.get_base_dir();
	if (!base_dir.is_empty() && base_dir != "res://") {
		const String absolute_dir = ProjectSettings::get_singleton()->globalize_path(base_dir);
		Error dir_err = DirAccess::make_dir_recursive_absolute(absolute_dir);
		if (dir_err != OK && dir_err != ERR_ALREADY_EXISTS) {
			result.error = vformat("Failed to create Markdown directory `%s` (error %d).", base_dir, dir_err);
			print_line(vformat("[AI Agent][Tool:project.create_markdown] Failed: create_dir error=%d path=%s", dir_err, base_dir));
			return result;
		}
	}

	Error file_err = OK;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE, &file_err);
	if (file.is_null() || file_err != OK) {
		result.error = vformat("Failed to open `%s` for writing (error %d).", path, file_err);
		print_line(vformat("[AI Agent][Tool:project.create_markdown] Failed: open error=%d path=%s", file_err, path));
		return result;
	}
	file->store_string(content);
	file->close();

	AIV1ProjectToolUtils::refresh_editor_file_system(path, true);

	const int byte_count = content.to_utf8_buffer().size();
	result.content = vformat("Created Markdown document `%s`.", path);
	result.metadata["path"] = path;
	result.metadata["bytes"] = byte_count;
	result.metadata["created"] = !existed_before;
	result.metadata["overwritten"] = existed_before && overwrite;
	print_line(vformat("[AI Agent][Tool:project.create_markdown] Completed. path=%s bytes=%d overwritten=%s", path, byte_count, existed_before && overwrite ? "yes" : "no"));
	return result;
}
