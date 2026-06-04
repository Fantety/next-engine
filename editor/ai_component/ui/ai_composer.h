/**************************************************************************/
/*  ai_composer.h                                                          */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/text_edit.h"

class AIAttachmentBar;
class AIPlanPanel;

class AIComposer : public VBoxContainer {
	GDCLASS(AIComposer, VBoxContainer);

	TextEdit *input = nullptr;
	AIAttachmentBar *attachment_bar = nullptr;
	AIPlanPanel *plan_panel = nullptr;
	OptionButton *model_selector = nullptr;
	OptionButton *mode_selector = nullptr;
	Button *send_button = nullptr;
	bool has_model = false;
	bool running = false;

	void _action_pressed();
	void _input_text_changed();
	void _input_gui_input(const Ref<InputEvent> &p_event);
	void _update_action_button();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIComposer();
	String get_input_text() const;
	String get_selected_model() const;
	String get_selected_agent_profile_id() const;
	Array get_attachments() const;
	void clear_input();
	void set_running(bool p_running);
	void reload_models();
};
