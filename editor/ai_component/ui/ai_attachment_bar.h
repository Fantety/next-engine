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
	Dictionary _make_image_attachment(const String &p_path) const;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIAttachmentBar();

	Array get_attachments() const;
	void set_attachments(const Array &p_attachments);
	void clear_attachments();
};
