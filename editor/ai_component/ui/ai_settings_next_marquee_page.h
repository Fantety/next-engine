/**************************************************************************/
/*  ai_settings_next_marquee_page.h                                       */
/**************************************************************************/

#pragma once

#include "editor/ai_component/ui/ai_next_marquee_settings.h"
#include "scene/gui/margin_container.h"

class Button;
class ColorRect;
class Label;
class OptionButton;
class TextEdit;
class VBoxContainer;

class AISettingsNextMarqueePage : public MarginContainer {
	GDCLASS(AISettingsNextMarqueePage, MarginContainer);

	OptionButton *preset_selector = nullptr;
	ColorRect *preview_rect = nullptr;
	Button *edit_custom_button = nullptr;
	Button *save_custom_button = nullptr;
	Button *cancel_custom_button = nullptr;
	Button *reset_custom_button = nullptr;
	TextEdit *shader_editor = nullptr;
	Label *status_label = nullptr;
	bool editing_custom = false;

	void _build_ui();
	void _populate_presets();
	void _select_current_preset();
	void _preset_selected(int p_index);
	void _edit_custom_pressed();
	void _save_custom_pressed();
	void _cancel_custom_pressed();
	void _reset_custom_pressed();
	void _refresh_editor_state();
	void _apply_preview_shader(const String &p_shader_code);
	void _set_status(const String &p_status, bool p_error);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AISettingsNextMarqueePage();

	void build_for_test();
	int get_preset_count_for_test() const;
	bool is_shader_editor_visible_for_test() const;
	void select_preset_for_test(const String &p_preset_id);
	void edit_custom_for_test();
};
