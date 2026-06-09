/**************************************************************************/
/*  ai_attachment_bar.cpp                                                 */
/**************************************************************************/

#include "ai_attachment_bar.h"

#include "core/core_bind.h"
#include "core/io/image.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/label.h"
#include "servers/display/display_server.h"
#include "servers/text/text_server.h"

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

String _clipboard_image_data_url() {
	DisplayServer *display_server = DisplayServer::get_singleton();
	if (!display_server) {
		return String();
	}

	Ref<Image> image = display_server->clipboard_get_image();
	if (image.is_null() || image->is_empty()) {
		return String();
	}

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

String _strip_trailing_token_punctuation(const String &p_token) {
	String token = p_token.strip_edges();
	while (!token.is_empty()) {
		const char32_t c = token[token.length() - 1];
		if (c == '.' || c == ',' || c == ';' || c == ')' || c == ']' || c == '}') {
			token = token.substr(0, token.length() - 1);
			continue;
		}
		break;
	}
	return token;
}

bool _attachment_equivalent(const Dictionary &p_a, const Dictionary &p_b) {
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

bool _is_token_boundary(const String &p_text, int p_index) {
	if (p_index < 0 || p_index >= p_text.length()) {
		return true;
	}

	const char32_t c = p_text[p_index];
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '.' || c == ',' || c == ';' || c == ')' || c == ']' || c == '}';
}

bool _contains_clipboard_token(const String &p_text) {
	const String token = "@clipboard";
	int from = 0;
	while (true) {
		const int pos = p_text.find(token, from);
		if (pos < 0) {
			return false;
		}
		if (_is_token_boundary(p_text, pos - 1) && _is_token_boundary(p_text, pos + token.length())) {
			return true;
		}
		from = pos + token.length();
	}
}

} // namespace

void AIAttachmentBar::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_attachments"), &AIAttachmentBar::get_attachments);
	ClassDB::bind_method(D_METHOD("set_attachments", "attachments"), &AIAttachmentBar::set_attachments);
	ClassDB::bind_method(D_METHOD("clear_attachments"), &AIAttachmentBar::clear_attachments);
	ClassDB::bind_method(D_METHOD("popup_reference_file_dialog"), &AIAttachmentBar::popup_reference_file_dialog);
	ClassDB::bind_method(D_METHOD("add_reference_path", "path"), &AIAttachmentBar::add_reference_path);
	ClassDB::bind_method(D_METHOD("add_clipboard_reference"), &AIAttachmentBar::add_clipboard_reference);
	ClassDB::bind_method(D_METHOD("get_attachments_with_inline_references", "text"), &AIAttachmentBar::get_attachments_with_inline_references);
	ADD_SIGNAL(MethodInfo("attachments_changed", PropertyInfo(Variant::ARRAY, "attachments")));
}

void AIAttachmentBar::_notification(int p_what) {
	if (p_what == NOTIFICATION_THEME_CHANGED || p_what == NOTIFICATION_ENTER_TREE) {
		if (add_button) {
			add_button->set_button_icon(get_editor_theme_icon(SNAME("Attachment")));
		}
		_refresh();
	}
}

AIAttachmentBar::AIAttachmentBar() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 3 * EDSCALE);

	add_button = memnew(Button);
	add_button->set_tooltip_text(TTR("Reference file"));
	add_button->connect(SceneStringName(pressed), callable_mp(this, &AIAttachmentBar::_add_pressed));
	add_child(add_button);

	chips = memnew(HFlowContainer);
	chips->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	chips->add_theme_constant_override("h_separation", 4 * EDSCALE);
	chips->add_theme_constant_override("v_separation", 3 * EDSCALE);
	add_child(chips);

	file_dialog = memnew(EditorFileDialog);
	file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	file_dialog->add_filter("*", "All Files");
	file_dialog->connect("file_selected", callable_mp(this, &AIAttachmentBar::_file_selected));
	add_child(file_dialog);
	_refresh();
}

Array AIAttachmentBar::get_attachments() const {
	return attachments.duplicate(true);
}

void AIAttachmentBar::set_attachments(const Array &p_attachments) {
	attachments = p_attachments.duplicate(true);
	_refresh();
	emit_signal(SNAME("attachments_changed"), get_attachments());
}

void AIAttachmentBar::clear_attachments() {
	if (attachments.is_empty()) {
		return;
	}
	attachments.clear();
	_refresh();
	emit_signal(SNAME("attachments_changed"), get_attachments());
}

void AIAttachmentBar::_add_pressed() {
	popup_reference_file_dialog();
}

void AIAttachmentBar::popup_reference_file_dialog() {
	if (!file_dialog) {
		return;
	}
	file_dialog->popup_file_dialog();
}

void AIAttachmentBar::_file_selected(const String &p_path) {
	add_reference_path(p_path);
}

void AIAttachmentBar::_remove_pressed(int p_index) {
	if (p_index < 0 || p_index >= attachments.size()) {
		return;
	}
	attachments.remove_at(p_index);
	_refresh();
	emit_signal(SNAME("attachments_changed"), get_attachments());
}

void AIAttachmentBar::_refresh() {
	if (!chips) {
		return;
	}
	while (chips->get_child_count() > 0) {
		Node *child = chips->get_child(0);
		chips->remove_child(child);
		child->queue_free();
	}

	if (attachments.is_empty()) {
		return;
	}

	for (int i = 0; i < attachments.size(); i++) {
		if (Variant(attachments[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary attachment = attachments[i];
		const String label_text = _make_attachment_label(attachment);
		HBoxContainer *chip = memnew(HBoxContainer);
		chip->set_name(SNAME("AIAttachmentChip"));
		chip->add_theme_constant_override("separation", 2 * EDSCALE);

		Label *label = memnew(Label);
		label->set_text(label_text);
		label->set_tooltip_text(String(attachment.get("path", String())));
		label->set_custom_minimum_size(Size2(72, 0) * EDSCALE);
		label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		label->set_clip_text(true);
		chip->add_child(label);

		Button *remove = memnew(Button);
		remove->set_button_icon(get_editor_theme_icon(SNAME("Close")));
		remove->set_tooltip_text(vformat(TTR("Remove %s"), label_text));
		remove->connect(SceneStringName(pressed), callable_mp(this, &AIAttachmentBar::_remove_pressed).bind(i), CONNECT_DEFERRED);
		chip->add_child(remove);
		chips->add_child(chip);
	}
}

String AIAttachmentBar::_make_attachment_label(const Dictionary &p_attachment) const {
	const String label = String(p_attachment.get("label", String()));
	if (!label.is_empty()) {
		return label;
	}

	const String source = String(p_attachment.get("source", String()));
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

Dictionary AIAttachmentBar::_make_reference_attachment(const String &p_path) const {
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
	return attachment;
}

Dictionary AIAttachmentBar::_make_clipboard_reference_attachment() const {
	Dictionary attachment;

	const String image_data_url = _clipboard_image_data_url();
	if (!image_data_url.is_empty()) {
		attachment["type"] = "image";
		attachment["source"] = "clipboard";
		attachment["label"] = TTR("Clipboard Image");
		attachment["mime_type"] = "image/png";
		attachment["data_url"] = image_data_url;
		attachment["detail"] = "auto";
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
	return attachment;
}

bool AIAttachmentBar::_append_attachment(const Dictionary &p_attachment) {
	if (p_attachment.is_empty()) {
		return false;
	}

	for (int i = 0; i < attachments.size(); i++) {
		if (Variant(attachments[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary existing = attachments[i];
		if (_attachment_equivalent(existing, p_attachment)) {
			return false;
		}
	}

	attachments.push_back(p_attachment);
	_refresh();
	emit_signal(SNAME("attachments_changed"), get_attachments());
	return true;
}

bool AIAttachmentBar::add_reference_path(const String &p_path) {
	return _append_attachment(_make_reference_attachment(p_path));
}

bool AIAttachmentBar::add_clipboard_reference() {
	return _append_attachment(_make_clipboard_reference_attachment());
}

void AIAttachmentBar::_append_inline_references(const String &p_text, Array &r_attachments) const {
	if (_contains_clipboard_token(p_text)) {
		const Dictionary clipboard_attachment = _make_clipboard_reference_attachment();
		if (!clipboard_attachment.is_empty()) {
			bool found = false;
			for (int i = 0; i < r_attachments.size(); i++) {
				if (Variant(r_attachments[i]).get_type() != Variant::DICTIONARY) {
					continue;
				}
				Dictionary existing = r_attachments[i];
				if (_attachment_equivalent(existing, clipboard_attachment)) {
					found = true;
					break;
				}
			}
			if (!found) {
				r_attachments.push_back(clipboard_attachment);
			}
		}
	}

	int from = 0;
	while (true) {
		const int quoted_pos = p_text.find("@\"res://", from);
		const int plain_pos = p_text.find("@res://", from);
		int pos = -1;
		bool quoted = false;
		if (quoted_pos >= 0 && (plain_pos < 0 || quoted_pos < plain_pos)) {
			pos = quoted_pos;
			quoted = true;
		} else {
			pos = plain_pos;
		}

		if (pos < 0) {
			break;
		}

		String path;
		if (quoted) {
			const int start = pos + 2;
			const int end = p_text.find("\"", start);
			if (end < 0) {
				break;
			}
			path = p_text.substr(start, end - start);
			from = end + 1;
		} else {
			const int start = pos + 1;
			int end = start;
			while (end < p_text.length()) {
				const char32_t c = p_text[end];
				if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
					break;
				}
				end++;
			}
			path = _strip_trailing_token_punctuation(p_text.substr(start, end - start));
			from = end;
		}

		const Dictionary attachment = _make_reference_attachment(path);
		if (attachment.is_empty()) {
			continue;
		}

		bool found = false;
		for (int i = 0; i < r_attachments.size(); i++) {
			if (Variant(r_attachments[i]).get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary existing = r_attachments[i];
			if (_attachment_equivalent(existing, attachment)) {
				found = true;
				break;
			}
		}
		if (!found) {
			r_attachments.push_back(attachment);
		}
	}
}

Array AIAttachmentBar::get_attachments_with_inline_references(const String &p_text) const {
	Array result = get_attachments();
	_append_inline_references(p_text, result);
	return result;
}
