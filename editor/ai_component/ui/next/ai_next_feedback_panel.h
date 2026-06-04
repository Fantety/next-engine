/**************************************************************************/
/*  ai_next_feedback_panel.h                                              */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"

class AIAgentNextSession;
class AIAttachmentBar;
class Button;
class TextEdit;

class AINextFeedbackPanel : public VBoxContainer {
	GDCLASS(AINextFeedbackPanel, VBoxContainer);

	AIAgentNextSession *next_session = nullptr;
	TextEdit *feedback_input = nullptr;
	AIAttachmentBar *attachment_bar = nullptr;
	Button *generate_button = nullptr;
	Button *lock_button = nullptr;

	void _generate_pressed();
	void _lock_pressed();

protected:
	static void _bind_methods();

public:
	AINextFeedbackPanel();
	void set_next_session(AIAgentNextSession *p_session);
	void refresh();
};
