/**************************************************************************/
/*  ai_attachment_bar.h                                                   */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"

class Button;
class EditorFileDialog;
class HFlowContainer;

class AIAttachmentBar : public HBoxContainer {
	GDCLASS(AIAttachmentBar, HBoxContainer);

	Button *add_button = nullptr;
	HFlowContainer *chips = nullptr;
	EditorFileDialog *file_dialog = nullptr;
	Array attachments;

	void _add_pressed();
	void _file_selected(const String &p_path);
	void _remove_pressed(int p_index);
	void _refresh();
	String _make_attachment_label(const Dictionary &p_attachment) const;
	Dictionary _make_reference_attachment(const String &p_path) const;
	Dictionary _make_clipboard_reference_attachment() const;
	bool _append_attachment(const Dictionary &p_attachment);
	void _append_inline_references(const String &p_text, Array &r_attachments) const;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIAttachmentBar();

	Array get_attachments() const;
	void set_attachments(const Array &p_attachments);
	void clear_attachments();
	void popup_reference_file_dialog();
	bool add_reference_path(const String &p_path);
	bool add_clipboard_reference();
	Array get_attachments_with_inline_references(const String &p_text) const;
};
