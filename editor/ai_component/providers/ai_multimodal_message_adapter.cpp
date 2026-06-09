/**************************************************************************/
/*  ai_multimodal_message_adapter.cpp                                     */
/**************************************************************************/

#include "ai_multimodal_message_adapter.h"

#include "core/core_bind.h"
#include "core/io/file_access.h"

namespace {

bool _is_supported_image_extension(const String &p_path) {
	const String ext = p_path.get_extension().to_lower();
	return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "gif";
}

bool _is_supported_text_extension(const String &p_path) {
	const String ext = p_path.get_extension().to_lower();
	static const char *text_extensions[] = {
		"gd", "cs", "tscn", "tres", "md", "txt", "json", "cfg", "shader", "gdshader",
		"xml", "yaml", "yml", "csv", "ini", "toml", "h", "hpp", "hh", "c",
		"cpp", "cc", "cxx", "py", "js", "ts", "tsx", "jsx", "html", "css",
		"scss", "rs", "go"
	};
	for (const char *text_extension : text_extensions) {
		if (ext == text_extension) {
			return true;
		}
	}
	return false;
}

bool _is_allowed_project_path(const String &p_path) {
	return p_path.begins_with("res://") && !p_path.contains("..");
}

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
	return String();
}

String _normalize_detail(const String &p_detail) {
	const String detail = p_detail.strip_edges().to_lower();
	if (detail == "low" || detail == "high" || detail == "auto") {
		return detail;
	}
	return "auto";
}

String _attachment_label(const Dictionary &p_attachment) {
	const String label = String(p_attachment.get("label", String())).strip_edges();
	if (!label.is_empty()) {
		return label;
	}

	const String source = String(p_attachment.get("source", String())).strip_edges();
	if (source == "clipboard") {
		return "clipboard";
	}

	const String path = String(p_attachment.get("path", String())).strip_edges();
	if (!path.is_empty()) {
		return path;
	}

	return "inline attachment";
}

String _format_text_attachment(const Dictionary &p_attachment, const String &p_text, bool p_truncated) {
	String section;
	section += "Referenced text from " + _attachment_label(p_attachment) + ":\n";
	section += "```text\n";
	section += p_text;
	if (!p_text.ends_with("\n")) {
		section += "\n";
	}
	section += "```";
	if (p_truncated) {
		section += "\n[reference truncated]";
	}
	return section;
}

} // namespace

String AIMultimodalMessageAdapter::_get_message_text(const Dictionary &p_message) {
	if (!p_message.has("content") || Variant(p_message["content"]).get_type() == Variant::NIL) {
		return String();
	}
	return String(p_message["content"]);
}

Array AIMultimodalMessageAdapter::_get_message_attachments(const Dictionary &p_message) {
	if (!p_message.has("metadata") || Variant(p_message["metadata"]).get_type() != Variant::DICTIONARY) {
		return Array();
	}

	Dictionary metadata = p_message["metadata"];
	if (!metadata.has("attachments") || Variant(metadata["attachments"]).get_type() != Variant::ARRAY) {
		return Array();
	}

	return metadata["attachments"];
}

String AIMultimodalMessageAdapter::_get_image_mime_type(const Dictionary &p_attachment) {
	const String explicit_mime_type = String(p_attachment.get("mime_type", String())).strip_edges();
	if (explicit_mime_type.begins_with("image/")) {
		return explicit_mime_type;
	}
	return _mime_type_from_path(String(p_attachment.get("path", String())));
}

String AIMultimodalMessageAdapter::_get_image_data_url(const Dictionary &p_attachment, const AIProviderConfig &p_config, String &r_error) {
	r_error = String();

	const String data_url = String(p_attachment.get("data_url", String())).strip_edges();
	if (data_url.begins_with("data:image/")) {
		return data_url;
	}

	const String path = String(p_attachment.get("path", String())).strip_edges();
	if (!_is_allowed_project_path(path)) {
		r_error = "Only res:// image paths without traversal are allowed.";
		return String();
	}
	if (!_is_supported_image_extension(path)) {
		r_error = "Attachment extension is not an allowlisted image type.";
		return String();
	}
	if (!FileAccess::exists(path)) {
		r_error = "Attachment image file does not exist.";
		return String();
	}

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		r_error = "Failed to open attachment image file.";
		return String();
	}

	const uint64_t length = file->get_length();
	if (length > (uint64_t)MAX(1, p_config.max_multimodal_file_bytes)) {
		r_error = "Attachment image exceeds the configured byte limit.";
		return String();
	}

	PackedByteArray bytes = file->get_buffer(length);
	Vector<uint8_t> raw;
	raw.resize(bytes.size());
	for (int i = 0; i < bytes.size(); i++) {
		raw.write[i] = bytes[i];
	}

	CoreBind::Marshalls *marshalls = CoreBind::Marshalls::get_singleton();
	if (!marshalls) {
		r_error = "Marshalls singleton is not available for base64 encoding.";
		return String();
	}

	const String mime_type = _get_image_mime_type(p_attachment);
	if (mime_type.is_empty()) {
		r_error = "Could not determine attachment image MIME type.";
		return String();
	}

	return "data:" + mime_type + ";base64," + marshalls->raw_to_base64(raw);
}

String AIMultimodalMessageAdapter::_build_text_attachment_context(const Array &p_attachments, const AIProviderConfig &p_config) {
	PackedStringArray sections;
	const int max_text_chars = MAX(1024, MIN(262144, p_config.max_context_chars));

	for (int i = 0; i < p_attachments.size(); i++) {
		if (Variant(p_attachments[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary attachment = p_attachments[i];
		const String type = String(attachment.get("type", String()));
		if (type == "text") {
			String text = String(attachment.get("text", String()));
			String error;
			bool truncated = false;
			if (text.is_empty()) {
				const String path = String(attachment.get("path", String())).strip_edges();
				if (!_is_allowed_project_path(path)) {
					error = "Only res:// text file paths without traversal are allowed.";
				} else if (!_is_supported_text_extension(path)) {
					error = "Referenced file extension is not in the text allowlist.";
				} else if (!FileAccess::exists(path)) {
					error = "Referenced text file does not exist.";
				} else {
					Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
					if (file.is_null()) {
						error = "Failed to open referenced text file.";
					} else {
						text = file->get_as_text();
					}
				}
			}

			if (!error.is_empty()) {
				sections.push_back("[Referenced text not sent from " + _attachment_label(attachment) + ": " + error + "]");
				continue;
			}

			if (text.length() > max_text_chars) {
				text = text.substr(0, max_text_chars);
				truncated = true;
			}
			sections.push_back(_format_text_attachment(attachment, text, truncated));
		} else if (type == "file") {
			sections.push_back("[Referenced file path only; content was not sent because it is not a supported text or image type: " + _attachment_label(attachment) + "]");
		}
	}

	return String("\n\n").join(sections);
}

String AIMultimodalMessageAdapter::_build_text_only_attachment_note(const Array &p_attachments) {
	String note;
	for (int i = 0; i < p_attachments.size(); i++) {
		if (Variant(p_attachments[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary attachment = p_attachments[i];
		if (String(attachment.get("type", String())) != "image") {
			continue;
		}

		const String path = String(attachment.get("path", String("<inline image>")));
		if (!note.is_empty()) {
			note += "\n";
		}
		note += "[Attached image not sent because current model is text-only: " + path + "]";
	}
	return note;
}

Variant AIMultimodalMessageAdapter::_build_chat_completions_user_content(const Dictionary &p_message, const AIProviderConfig &p_config) {
	const String text = _get_message_text(p_message);
	const Array attachments = _get_message_attachments(p_message);
	if (attachments.is_empty()) {
		return text;
	}

	const String reference_text = _build_text_attachment_context(attachments, p_config);
	String combined_text = text;
	if (!reference_text.is_empty()) {
		combined_text = combined_text.is_empty() ? reference_text : combined_text + "\n\n" + reference_text;
	}

	if (!p_config.supports_multimodal) {
		const String note = _build_text_only_attachment_note(attachments);
		if (note.is_empty()) {
			return combined_text;
		}
		return combined_text.is_empty() ? note : combined_text + "\n\n" + note;
	}

	Array content;
	if (!combined_text.is_empty()) {
		Dictionary text_part;
		text_part["type"] = "text";
		text_part["text"] = combined_text;
		content.push_back(text_part);
	}

	int image_count = 0;
	for (int i = 0; i < attachments.size(); i++) {
		if (Variant(attachments[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary attachment = attachments[i];
		if (String(attachment.get("type", String())) != "image") {
			continue;
		}
		if (image_count >= MAX(1, p_config.max_multimodal_files)) {
			break;
		}

		String error;
		const String data_url = _get_image_data_url(attachment, p_config, error);
		if (data_url.is_empty()) {
			Dictionary error_part;
			error_part["type"] = "text";
			error_part["text"] = "[Attached image not sent: " + error + "]";
			content.push_back(error_part);
			continue;
		}

		Dictionary image_url;
		image_url["url"] = data_url;
		image_url["detail"] = _normalize_detail(String(attachment.get("detail", "auto")));

		Dictionary image_part;
		image_part["type"] = "image_url";
		image_part["image_url"] = image_url;
		content.push_back(image_part);
		image_count++;
	}

	if (content.is_empty()) {
		return text;
	}
	return content;
}

Variant AIMultimodalMessageAdapter::build_chat_message_content(const Dictionary &p_message, const AIProviderConfig &p_config) {
	const String role = String(p_message.get("role", "user"));
	if (role == "user") {
		return _build_chat_completions_user_content(p_message, p_config);
	}
	return _get_message_text(p_message);
}

void AIMultimodalMessageAdapter::append_tool_attachment_user_message(Array &r_chat_messages, const Dictionary &p_tool_message, const AIProviderConfig &p_config) {
	const Array attachments = _get_message_attachments(p_tool_message);
	if (attachments.is_empty()) {
		return;
	}

	Dictionary user_metadata;
	user_metadata["attachments"] = attachments;
	user_metadata["source_tool_call_id"] = String();
	if (p_tool_message.has("metadata") && Variant(p_tool_message["metadata"]).get_type() == Variant::DICTIONARY) {
		Dictionary tool_metadata = p_tool_message["metadata"];
		user_metadata["source_tool_call_id"] = String(tool_metadata.get("tool_call_id", String()));
		user_metadata["source_tool_name"] = String(tool_metadata.get("tool_name", String()));
	}

	Dictionary user_message;
	user_message["role"] = "user";
	user_message["content"] = "Multimodal attachment added to the conversation context.";
	user_message["metadata"] = user_metadata;

	Dictionary chat_message;
	chat_message["role"] = "user";
	chat_message["content"] = build_chat_message_content(user_message, p_config);
	r_chat_messages.push_back(chat_message);
}

Array AIMultimodalMessageAdapter::build_chat_messages(const Array &p_messages, const AIProviderConfig &p_config) {
	Array chat_messages;
	for (int i = 0; i < p_messages.size(); i++) {
		if (Variant(p_messages[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary message = p_messages[i];
		const String role = String(message.get("role", "user"));
		if (role != "system" && role != "user" && role != "assistant" && role != "tool") {
			continue;
		}

		Dictionary chat_message;
		chat_message["role"] = role;
		chat_message["content"] = build_chat_message_content(message, p_config);
		if (role == "tool" && message.has("metadata") && Variant(message["metadata"]).get_type() == Variant::DICTIONARY) {
			Dictionary metadata = message["metadata"];
			chat_message["tool_call_id"] = String(metadata.get("tool_call_id", ""));
		}
		chat_messages.push_back(chat_message);
		if (role == "tool") {
			append_tool_attachment_user_message(chat_messages, message, p_config);
		}
	}
	return chat_messages;
}
