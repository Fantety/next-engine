/**************************************************************************/
/*  ai_composer.h                                                          */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/text_edit.h"

class AIComposer : public VBoxContainer {
	GDCLASS(AIComposer, VBoxContainer);

	TextEdit *input = nullptr;
	OptionButton *model_selector = nullptr;
	Button *send_button = nullptr;
	Button *cancel_button = nullptr;

	void _send_pressed();
	void _cancel_pressed();

protected:
	static void _bind_methods();

public:
	AIComposer();
	String get_input_text() const;
	String get_selected_model() const;
	void clear_input();
	void set_running(bool p_running);
	void reload_models();
};
