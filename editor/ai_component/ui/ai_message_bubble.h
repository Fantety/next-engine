/**************************************************************************/
/*  ai_message_bubble.h                                                    */
/**************************************************************************/

#pragma once

#include "scene/gui/panel_container.h"
#include "scene/gui/rich_text_label.h"

class AIMessageBubble : public PanelContainer {
	GDCLASS(AIMessageBubble, PanelContainer);

	RichTextLabel *label = nullptr;

protected:
	static void _bind_methods();

public:
	AIMessageBubble();
	void set_message(const Dictionary &p_message);
};
