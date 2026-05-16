/**************************************************************************/
/*  ai_read_file_tool.cpp                                                 */
/**************************************************************************/

#include "ai_read_file_tool.h"

#include "core/io/file_access.h"
#include "core/variant/variant.h"

#include "editor/ai_component/tools/project/ai_project_tool_utils.h"

String AIReadFileTool::get_name() const {
	return "project.read_file";
}

String AIReadFileTool::get_description() const {
	return "Reads an allowlisted text file under res:// with a byte limit.";
}

Dictionary AIReadFileTool::get_parameters_schema() const {
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

AIToolResult AIReadFileTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	String path = AIProjectToolUtils::get_path_argument(p_arguments, String());
	if (path.is_empty()) {
		result.error = "Missing required path.";
		return result;
	}
	if (!AIProjectToolUtils::is_allowed_path(path)) {
		result.error = "Only res:// project paths without traversal are allowed.";
		return result;
	}
	if (!AIProjectToolUtils::is_allowed_text_extension(path)) {
		result.error = "File extension is not in the text allowlist.";
		return result;
	}
	if (!FileAccess::exists(path)) {
		result.error = "File does not exist.";
		return result;
	}

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		result.error = "Failed to open file.";
		return result;
	}

	int max_bytes = AIProjectToolUtils::get_int_argument(p_arguments, "max_bytes", 65536, 1024, 262144);
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
	return result;
}
