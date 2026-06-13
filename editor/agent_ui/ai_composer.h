/**************************************************************************/
/*  ai_composer.h                                                          */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/popup_menu.h"

class AIComposerInput;
class EditorFileDialog;
class TextEdit;

class AIComposer : public VBoxContainer {
	GDCLASS(AIComposer, VBoxContainer);

	AIComposerInput *input = nullptr;
	OptionButton *model_selector = nullptr;
	OptionButton *mode_selector = nullptr;
	Button *send_button = nullptr;
	PopupMenu *reference_menu = nullptr;
	EditorFileDialog *reference_file_dialog = nullptr;
	bool has_model = false;
	bool running = false;
	bool action_button_theme_ready = false;
	int reference_trigger_line = -1;
	int reference_trigger_column = -1;

	void _mode_selected(int p_index);
	void _action_pressed();
	void _input_text_changed();
	void _input_gui_input(const Ref<InputEvent> &p_event);
	void _reference_menu_id_pressed(int p_id);
	void _reference_file_selected(const String &p_path);
	void _reference_file_dialog_canceled();
	void _show_reference_menu();
	void _clear_reference_trigger();
	void _remove_reference_trigger();
	void _add_clipboard_reference();
	void _add_canvas_reference();
	void _add_reference_path(const String &p_path);
	void _update_action_button();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIComposer();
	String get_input_text() const;
	String get_selected_model() const;
	String get_selected_agent_profile_id() const;
	Array get_attachments_for_send() const;
	void clear_input();
	void set_running(bool p_running);
	void reload_models();
};
