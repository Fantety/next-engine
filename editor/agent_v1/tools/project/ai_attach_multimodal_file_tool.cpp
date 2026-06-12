/**************************************************************************/
/*  ai_attach_multimodal_file_tool.cpp                                    */
/**************************************************************************/

#include "ai_attach_multimodal_file_tool.h"

#include "core/io/file_access.h"

#include "editor/agent_v1/tools/project/ai_project_tool_utils.h"

namespace {

String _mime_type_from_path(const String &p_path) {
	const String ext = p_path.get_extension().to_lower();
	if (ext == "png") {
		return "image/png";
	}
	if (ext == "jpg" || ext == "jpeg") {
		return "image/jpeg";
	}
	if (ext == "webp") {
		return "image/webp";
	}
	if (ext == "gif") {
		return "image/gif";
	}
	return "application/octet-stream";
}

String _normalize_detail(const String &p_detail) {
	const String detail = p_detail.strip_edges().to_lower();
	if (detail == "low" || detail == "high" || detail == "auto") {
		return detail;
	}
	return "auto";
}

} // namespace

String AIV1AttachMultimodalFileTool::get_name() const {
	return "project.attach_multimodal_file";
}

String AIV1AttachMultimodalFileTool::get_description() const {
	return "Attaches an allowlisted image under res:// to the next multimodal user context. The image is sent as user message content only when the selected model supports multimodal input.";
}

Dictionary AIV1AttachMultimodalFileTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary path_property;
	path_property["type"] = "string";
	path_property["description"] = "Image path under res://. Supported extensions: png, jpg, jpeg, webp, gif.";
	properties["path"] = path_property;

	Dictionary detail_property;
	detail_property["type"] = "string";
	detail_property["description"] = "Image detail level for multimodal models.";
	Array detail_enum;
	detail_enum.push_back("auto");
	detail_enum.push_back("low");
	detail_enum.push_back("high");
	detail_property["enum"] = detail_enum;
	properties["detail"] = detail_property;

	Array required;
	required.push_back("path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIV1EditorToolResult AIV1AttachMultimodalFileTool::execute_tool(const Dictionary &p_arguments) {
	AIV1EditorToolResult result;
	const String path = AIV1ProjectToolUtils::get_path_argument(p_arguments, String());
	print_line(vformat("[AI Agent][Tool:project.attach_multimodal_file] Start. path=%s", path));
	if (path.is_empty()) {
		result.error = "Missing required path.";
		return result;
	}
	if (!AIV1ProjectToolUtils::is_allowed_path(path)) {
		result.error = "Only res:// project paths without traversal are allowed.";
		return result;
	}
	if (!AIV1ProjectToolUtils::is_allowed_image_extension(path)) {
		result.error = "File extension is not in the image allowlist.";
		return result;
	}
	if (!FileAccess::exists(path)) {
		result.error = "File does not exist.";
		return result;
	}

	Dictionary attachment;
	attachment["type"] = "image";
	attachment["path"] = path;
	attachment["mime_type"] = _mime_type_from_path(path);
	attachment["detail"] = _normalize_detail(String(p_arguments.get("detail", "auto")));

	Array attachments;
	attachments.push_back(attachment);
	result.metadata["attachments"] = attachments;
	result.metadata["path"] = path;
	result.metadata["mime_type"] = String(attachment["mime_type"]);
	result.content = "Attached image `" + path + "` to multimodal user context.";
	print_line(vformat("[AI Agent][Tool:project.attach_multimodal_file] Completed. path=%s", path));
	return result;
}
