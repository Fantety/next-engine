/**************************************************************************/
/*  ai_attachment_bar.cpp                                                 */
/**************************************************************************/

#include "ai_attachment_bar.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "servers/text/text_server.h"

namespace {

String _mime_type_for_path(const String &p_path) {
	const String extension = p_path.get_extension().to_lower();
	if (extension == "jpg" || extension == "jpeg") {
		return "image/jpeg";
	}
	if (extension == "webp") {
		return "image/webp";
	}
	return "image/png";
}

} // namespace

void AIAttachmentBar::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_attachments"), &AIAttachmentBar::get_attachments);
	ClassDB::bind_method(D_METHOD("set_attachments", "attachments"), &AIAttachmentBar::set_attachments);
	ClassDB::bind_method(D_METHOD("clear_attachments"), &AIAttachmentBar::clear_attachments);
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

	HBoxContainer *row = memnew(HBoxContainer);
	row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row->add_theme_constant_override("separation", 4 * EDSCALE);
	add_child(row);

	add_button = memnew(Button);
	add_button->set_button_icon(get_editor_theme_icon(SNAME("Attachment")));
	add_button->set_tooltip_text(TTR("Attach image"));
	add_button->connect(SceneStringName(pressed), callable_mp(this, &AIAttachmentBar::_add_pressed));
	row->add_child(add_button);

	chips = memnew(HBoxContainer);
	chips->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	chips->add_theme_constant_override("separation", 4 * EDSCALE);
	row->add_child(chips);

	file_dialog = memnew(EditorFileDialog);
	file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	file_dialog->add_filter("*.png", "PNG Image");
	file_dialog->add_filter("*.jpg,*.jpeg", "JPEG Image");
	file_dialog->add_filter("*.webp", "WebP Image");
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
	if (!file_dialog) {
		return;
	}
	file_dialog->popup_file_dialog();
}

void AIAttachmentBar::_file_selected(const String &p_path) {
	const Dictionary attachment = _make_image_attachment(p_path);
	const String path = String(attachment.get("path", String()));
	if (path.is_empty()) {
		return;
	}
	for (int i = 0; i < attachments.size(); i++) {
		if (Variant(attachments[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary existing = attachments[i];
		if (String(existing.get("path", String())) == path) {
			return;
		}
	}
	attachments.push_back(attachment);
	_refresh();
	emit_signal(SNAME("attachments_changed"), get_attachments());
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
		memdelete(child);
	}

	if (attachments.is_empty()) {
		return;
	}

	for (int i = 0; i < attachments.size(); i++) {
		if (Variant(attachments[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary attachment = attachments[i];
		Button *chip = memnew(Button);
		chip->set_text(_make_attachment_label(attachment));
		chip->set_button_icon(get_editor_theme_icon(SNAME("Close")));
		chip->set_tooltip_text(String(attachment.get("path", String())));
		chip->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		chip->set_clip_text(true);
		chip->connect(SceneStringName(pressed), callable_mp(this, &AIAttachmentBar::_remove_pressed).bind(i));
		chips->add_child(chip);
	}
}

String AIAttachmentBar::_make_attachment_label(const Dictionary &p_attachment) const {
	const String path = String(p_attachment.get("path", String()));
	if (path.is_empty()) {
		return TTR("Image");
	}
	return path.get_file();
}

Dictionary AIAttachmentBar::_make_image_attachment(const String &p_path) const {
	Dictionary attachment;
	const String path = p_path.strip_edges();
	if (path.is_empty()) {
		return attachment;
	}
	attachment["type"] = "image";
	attachment["path"] = path;
	attachment["mime_type"] = _mime_type_for_path(path);
	attachment["detail"] = "auto";
	return attachment;
}
