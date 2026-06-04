/**************************************************************************/
/*  ai_settings_next_marquee_page.h                                       */
/**************************************************************************/

#pragma once

#include "editor/ai_component/ui/ai_next_marquee_settings.h"
#include "scene/gui/margin_container.h"

class Button;
class ColorRect;
class ConfirmationDialog;
class Label;
class LineEdit;
class TextEdit;
class VBoxContainer;

class AISettingsNextMarqueePage : public MarginContainer {
	GDCLASS(AISettingsNextMarqueePage, MarginContainer);

	Button *add_marquee_button = nullptr;
	ColorRect *selected_preview_rect = nullptr;
	VBoxContainer *marquee_table = nullptr;
	ColorRect *preview_rect = nullptr;
	ConfirmationDialog *marquee_dialog = nullptr;
	LineEdit *name_edit = nullptr;
	TextEdit *shader_editor = nullptr;
	Label *status_label = nullptr;
	Vector<AINextMarqueePreset> marquee_rows;

	void _build_ui();
	void _build_add_dialog();
	void _refresh_marquee_table();
	void _add_marquee_table_row(const AINextMarqueePreset &p_marquee, const String &p_current_id);
	void _select_current_marquee();
	void _marquee_selected(const String &p_marquee_id);
	void _popup_add_marquee_dialog();
	void _add_marquee_confirmed();
	void _dialog_shader_changed();
	void _apply_selected_preview_shader(const String &p_shader_code);
	void _apply_dialog_preview_shader(const String &p_shader_code);
	void _set_status(const String &p_status, bool p_error);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AISettingsNextMarqueePage();

	virtual Size2 get_minimum_size() const override;
	void build_for_test();
	int get_preset_count_for_test() const;
	void select_preset_for_test(const String &p_preset_id);
	String add_marquee_for_test(const String &p_display_name, const String &p_shader_code);
};
