/**************************************************************************/
/*  ai_reference_resolver.cpp                                              */
/**************************************************************************/

#include "ai_reference_resolver.h"

#include "core/core_bind.h"
#include "core/io/image.h"
#include "core/string/translation.h"
#include "editor/editor_node.h"
#include "editor/scene/canvas_item_editor_plugin.h"
#include "scene/main/viewport.h"
#include "servers/display/display_server.h"

namespace {

bool _is_image_path(const String &p_path) {
	const String extension = p_path.get_extension().to_lower();
	return extension == "png" || extension == "jpg" || extension == "jpeg" || extension == "webp" || extension == "gif";
}

bool _is_text_path(const String &p_path) {
	const String extension = p_path.get_extension().to_lower();
	static const char *text_extensions[] = {
		"gd", "cs", "tscn", "tres", "md", "txt", "json", "cfg", "shader", "gdshader",
		"xml", "yaml", "yml", "csv", "ini", "toml", "h", "hpp", "hh", "c",
		"cpp", "cc", "cxx", "py", "js", "ts", "tsx", "jsx", "html", "css",
		"scss", "rs", "go"
	};
	for (const char *text_extension : text_extensions) {
		if (extension == text_extension) {
			return true;
		}
	}
	return false;
}

String _mime_type_for_path(const String &p_path) {
	const String extension = p_path.get_extension().to_lower();
	if (extension == "png") {
		return "image/png";
	}
	if (extension == "jpg" || extension == "jpeg") {
		return "image/jpeg";
	}
	if (extension == "webp") {
		return "image/webp";
	}
	if (extension == "gif") {
		return "image/gif";
	}
	if (extension == "json") {
		return "application/json";
	}
	if (extension == "md") {
		return "text/markdown";
	}
	if (_is_text_path(p_path)) {
		return "text/plain";
	}
	return "application/octet-stream";
}

String _image_data_url(const Ref<Image> &p_image) {
	if (p_image.is_null() || p_image->is_empty()) {
		return String();
	}

	Ref<Image> image = p_image->duplicate();
	if (image.is_null() || image->is_empty()) {
		return String();
	}
	image->convert(Image::FORMAT_RGBA8);

	Vector<uint8_t> png = image->save_png_to_buffer();
	if (png.is_empty()) {
		return String();
	}

	CoreBind::Marshalls *marshalls = CoreBind::Marshalls::get_singleton();
	if (!marshalls) {
		return String();
	}

	return "data:image/png;base64," + marshalls->raw_to_base64(png);
}

String _clipboard_image_data_url() {
	DisplayServer *display_server = DisplayServer::get_singleton();
	if (!display_server) {
		return String();
	}

	return _image_data_url(display_server->clipboard_get_image());
}

String _canvas_image_data_url(Size2i &r_canvas_size) {
	EditorNode *editor = EditorNode::get_singleton();
	if (!editor) {
		return String();
	}

	SubViewport *scene_root = editor->get_scene_root();
	if (!scene_root) {
		return String();
	}

	Ref<ViewportTexture> texture = scene_root->get_texture();
	if (texture.is_null()) {
		return String();
	}

	Ref<Image> image = texture->get_image();
	if (image.is_null() || image->is_empty()) {
		return String();
	}

	CanvasItemEditor *canvas_editor = CanvasItemEditor::get_singleton();
	Control *canvas_control = canvas_editor ? canvas_editor->get_viewport_control() : nullptr;
	if (canvas_control) {
		const Size2i canvas_control_size = canvas_control->get_size();
		if (canvas_control_size.width > 0 && canvas_control_size.height > 0 && canvas_control_size != image->get_size()) {
			const Size2i image_size = image->get_size();
			const Size2i crop_size(MIN(canvas_control_size.width, image_size.width), MIN(canvas_control_size.height, image_size.height));
			if (crop_size.width > 0 && crop_size.height > 0) {
				image = image->get_region(Rect2i(Point2i(), crop_size));
			}
		}
	}

	r_canvas_size = image->get_size();
	return _image_data_url(image);
}

bool _is_reference_start_boundary(const String &p_text, int p_index) {
	if (p_index < 0 || p_index >= p_text.length()) {
		return true;
	}

	const char32_t c = p_text[p_index];
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '(' || c == '[' || c == '{' || c == '<';
}

bool _is_reference_end_boundary(const String &p_text, int p_index) {
	if (p_index < 0 || p_index >= p_text.length()) {
		return true;
	}

	const char32_t c = p_text[p_index];
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '.' || c == ',' || c == ';' || c == ')' || c == ']' || c == '}' || c == '>';
}

bool _is_plain_path_terminator(char32_t p_char) {
	return p_char == ' ' || p_char == '\t' || p_char == '\n' || p_char == '\r';
}

bool _is_trailing_token_punctuation(char32_t p_char) {
	return p_char == '.' || p_char == ',' || p_char == ';' || p_char == ')' || p_char == ']' || p_char == '}' || p_char == '>';
}

bool _needs_quoted_reference_token(const String &p_path) {
	for (int i = 0; i < p_path.length(); i++) {
		const char32_t c = p_path[i];
		if (_is_plain_path_terminator(c)) {
			return true;
		}
	}
	return !p_path.is_empty() && _is_trailing_token_punctuation(p_path[p_path.length() - 1]);
}

void _append_unique_attachment(Array &r_attachments, const Dictionary &p_attachment) {
	if (p_attachment.is_empty()) {
		return;
	}

	for (int i = 0; i < r_attachments.size(); i++) {
		if (Variant(r_attachments[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary existing = r_attachments[i];
		if (AIReferenceResolver::attachments_equivalent(existing, p_attachment)) {
			return;
		}
	}
	r_attachments.push_back(p_attachment);
}

} // namespace

Vector<AIReferenceResolver::ReferenceToken> AIReferenceResolver::find_reference_tokens(const String &p_text) {
	Vector<ReferenceToken> tokens;

	int from = 0;
	while (from < p_text.length()) {
		const int pos = p_text.find("@", from);
		if (pos < 0) {
			break;
		}

		if (!_is_reference_start_boundary(p_text, pos - 1)) {
			from = pos + 1;
			continue;
		}

		if (p_text.substr(pos, 10) == "@clipboard" && _is_reference_end_boundary(p_text, pos + 10)) {
			ReferenceToken token;
			token.start_column = pos;
			token.end_column = pos + 10;
			token.clipboard = true;
			tokens.push_back(token);
			from = token.end_column;
			continue;
		}

		if (p_text.substr(pos, 7) == "@canvas" && _is_reference_end_boundary(p_text, pos + 7)) {
			ReferenceToken token;
			token.start_column = pos;
			token.end_column = pos + 7;
			token.canvas = true;
			tokens.push_back(token);
			from = token.end_column;
			continue;
		}

		if (p_text.substr(pos, 8) == "@\"res://") {
			const int path_start = pos + 2;
			const int path_end = p_text.find("\"", path_start);
			if (path_end < 0) {
				from = pos + 1;
				continue;
			}

			ReferenceToken token;
			token.start_column = pos;
			token.end_column = path_end + 1;
			token.path = p_text.substr(path_start, path_end - path_start);
			if (!token.path.is_empty()) {
				tokens.push_back(token);
			}
			from = path_end + 1;
			continue;
		}

		if (p_text.substr(pos, 7) == "@res://") {
			const int path_start = pos + 1;
			int path_end = path_start;
			while (path_end < p_text.length() && !_is_plain_path_terminator(p_text[path_end])) {
				path_end++;
			}
			while (path_end > path_start && _is_trailing_token_punctuation(p_text[path_end - 1])) {
				path_end--;
			}

			ReferenceToken token;
			token.start_column = pos;
			token.end_column = path_end;
			token.path = p_text.substr(path_start, path_end - path_start);
			if (!token.path.is_empty()) {
				tokens.push_back(token);
			}
			from = MAX(pos + 1, path_end);
			continue;
		}

		from = pos + 1;
	}

	return tokens;
}

Vector<AIReferenceResolver::ReferenceToken> AIReferenceResolver::find_reference_tokens_in_line(const String &p_line) {
	return find_reference_tokens(p_line);
}

Array AIReferenceResolver::resolve_attachments(const String &p_text) {
	Array attachments;
	const Vector<ReferenceToken> tokens = find_reference_tokens(p_text);
	bool clipboard_resolved = false;
	bool canvas_resolved = false;

	for (const ReferenceToken &token : tokens) {
		if (token.clipboard) {
			if (clipboard_resolved) {
				continue;
			}
			clipboard_resolved = true;
			_append_unique_attachment(attachments, make_clipboard_reference_attachment());
			continue;
		}
		if (token.canvas) {
			if (canvas_resolved) {
				continue;
			}
			canvas_resolved = true;
			_append_unique_attachment(attachments, make_canvas_reference_attachment());
			continue;
		}
		_append_unique_attachment(attachments, make_reference_attachment(token.path));
	}

	return attachments;
}

Dictionary AIReferenceResolver::make_reference_attachment(const String &p_path) {
	Dictionary attachment;
	const String path = p_path.strip_edges();
	if (path.is_empty()) {
		return attachment;
	}

	if (_is_image_path(path)) {
		attachment["type"] = "image";
		attachment["detail"] = "auto";
	} else if (_is_text_path(path)) {
		attachment["type"] = "text";
	} else {
		attachment["type"] = "file";
	}
	attachment["source"] = "file";
	attachment["path"] = path;
	attachment["mime_type"] = _mime_type_for_path(path);
	attachment["inline_reference"] = true;
	return attachment;
}

Dictionary AIReferenceResolver::make_clipboard_reference_attachment() {
	Dictionary attachment;

	const String image_data_url = _clipboard_image_data_url();
	if (!image_data_url.is_empty()) {
		attachment["type"] = "image";
		attachment["source"] = "clipboard";
		attachment["label"] = TTR("Clipboard Image");
		attachment["mime_type"] = "image/png";
		attachment["data_url"] = image_data_url;
		attachment["detail"] = "auto";
		attachment["inline_reference"] = true;
		return attachment;
	}

	DisplayServer *display_server = DisplayServer::get_singleton();
	if (!display_server) {
		return attachment;
	}

	const String text = display_server->clipboard_get();
	if (text.is_empty()) {
		return attachment;
	}

	attachment["type"] = "text";
	attachment["source"] = "clipboard";
	attachment["label"] = TTR("Clipboard");
	attachment["mime_type"] = "text/plain";
	attachment["text"] = text;
	attachment["inline_reference"] = true;
	return attachment;
}

Dictionary AIReferenceResolver::make_canvas_reference_attachment() {
	Dictionary attachment;
	Size2i canvas_size;
	const String image_data_url = _canvas_image_data_url(canvas_size);
	if (image_data_url.is_empty()) {
		return attachment;
	}

	attachment["type"] = "image";
	attachment["source"] = "canvas";
	attachment["label"] = TTR("Canvas");
	attachment["mime_type"] = "image/png";
	attachment["data_url"] = image_data_url;
	attachment["detail"] = "auto";
	attachment["inline_reference"] = true;
	attachment["width"] = canvas_size.width;
	attachment["height"] = canvas_size.height;
	return attachment;
}

String AIReferenceResolver::make_reference_token_for_path(const String &p_path) {
	const String path = p_path.strip_edges();
	if (path.is_empty()) {
		return String();
	}

	if (_needs_quoted_reference_token(path)) {
		return "@\"" + path + "\"";
	}
	return "@" + path;
}

String AIReferenceResolver::make_attachment_label(const Dictionary &p_attachment) {
	const String label = String(p_attachment.get("label", String()));
	if (!label.is_empty()) {
		return label;
	}

	const String source = String(p_attachment.get("source", String()));
	if (source == "canvas") {
		return TTR("Canvas");
	}
	if (source == "clipboard") {
		const String type = String(p_attachment.get("type", String()));
		if (type == "image") {
			return TTR("Clipboard Image");
		}
		return TTR("Clipboard");
	}

	const String path = String(p_attachment.get("path", String()));
	if (path.is_empty()) {
		const String type = String(p_attachment.get("type", String()));
		if (type == "image") {
			return TTR("Image");
		}
		if (type == "text") {
			return TTR("Text");
		}
		return TTR("File");
	}
	return path.get_file();
}

bool AIReferenceResolver::attachments_equivalent(const Dictionary &p_a, const Dictionary &p_b) {
	const String a_path = String(p_a.get("path", String()));
	const String b_path = String(p_b.get("path", String()));
	if (!a_path.is_empty() || !b_path.is_empty()) {
		return a_path == b_path && String(p_a.get("type", String())) == String(p_b.get("type", String()));
	}

	return String(p_a.get("type", String())) == String(p_b.get("type", String())) &&
			String(p_a.get("source", String())) == String(p_b.get("source", String())) &&
			String(p_a.get("text", String())) == String(p_b.get("text", String())) &&
			String(p_a.get("data_url", String())) == String(p_b.get("data_url", String()));
}
