/**************************************************************************/
/*  ai_message_bubble.h                                                    */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/link_button.h"
#include "scene/gui/panel_container.h"

class AIMarkdownLabel;
class HFlowContainer;

class AIMessageBubble : public PanelContainer {
	GDCLASS(AIMessageBubble, PanelContainer);

	VBoxContainer *content_box = nullptr;
	HBoxContainer *header_box = nullptr;
	Label *title_label = nullptr;
	AIMarkdownLabel *label = nullptr;
	LinkButton *details_button = nullptr;
	HFlowContainer *attachments_box = nullptr;
	Dictionary current_message;
	bool details_expanded = false;
	bool details_available = false;
	bool layout_update_queued = false;

	void _render_message();
	void _render_attachments(const Dictionary &p_metadata);
	void _toggle_details();
	void _queue_layout_update();
	void _flush_layout_update();

protected:
	static void _bind_methods();

public:
	AIMessageBubble();
	void set_message(const Dictionary &p_message);
};
