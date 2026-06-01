/**************************************************************************/
/*  ai_settings_next_marquee_page.cpp                                     */
/**************************************************************************/

#include "ai_settings_next_marquee_page.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/text_edit.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"
#include "servers/text/text_server.h"

namespace {

Ref<ShaderMaterial> _make_marquee_material(const String &p_shader_code) {
	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(p_shader_code);

	Ref<ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);
	return material;
}

} // namespace

void AISettingsNextMarqueePage::_bind_methods() {
	ADD_SIGNAL(MethodInfo("settings_changed"));
}

void AISettingsNextMarqueePage::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build_ui();
	}
}

AISettingsNextMarqueePage::AISettingsNextMarqueePage() {
}

void AISettingsNextMarqueePage::_build_ui() {
	if (preset_selector) {
		return;
	}

	add_theme_constant_override("margin_left", 8 * EDSCALE);
	add_theme_constant_override("margin_right", 8 * EDSCALE);
	add_theme_constant_override("margin_top", 8 * EDSCALE);
	add_theme_constant_override("margin_bottom", 8 * EDSCALE);

	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(scroll);

	VBoxContainer *content = memnew(VBoxContainer);
	content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_theme_constant_override("separation", 12 * EDSCALE);
	scroll->add_child(content);

	Label *title = memnew(Label);
	title->set_text(TTR("NEXT Marquee"));
	title->add_theme_font_size_override(SceneStringName(font_size), int(22 * EDSCALE));
	content->add_child(title);

	Label *description = memnew(Label);
	description->set_text(TTR("Choose the loading marquee shown while the AI Agent is working. Presets are preview-only; Custom can be edited separately."));
	description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	content->add_child(description);

	HBoxContainer *selector_row = memnew(HBoxContainer);
	selector_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	selector_row->add_theme_constant_override("separation", 8 * EDSCALE);
	content->add_child(selector_row);

	Label *selector_label = memnew(Label);
	selector_label->set_text(TTR("Appearance"));
	selector_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	selector_label->set_custom_minimum_size(Size2(110, 0) * EDSCALE);
	selector_row->add_child(selector_label);

	preset_selector = memnew(OptionButton);
	preset_selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	preset_selector->set_fit_to_longest_item(false);
	preset_selector->connect(SceneStringName(item_selected), callable_mp(this, &AISettingsNextMarqueePage::_preset_selected));
	selector_row->add_child(preset_selector);

	PanelContainer *preview_panel = memnew(PanelContainer);
	preview_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(preview_panel);

	MarginContainer *preview_margin = memnew(MarginContainer);
	preview_margin->add_theme_constant_override("margin_left", 16 * EDSCALE);
	preview_margin->add_theme_constant_override("margin_right", 16 * EDSCALE);
	preview_margin->add_theme_constant_override("margin_top", 18 * EDSCALE);
	preview_margin->add_theme_constant_override("margin_bottom", 18 * EDSCALE);
	preview_panel->add_child(preview_margin);

	preview_rect = memnew(ColorRect);
	preview_rect->set_color(Color(1, 1, 1, 1));
	preview_rect->set_custom_minimum_size(Size2(0, 12) * EDSCALE);
	preview_rect->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	preview_margin->add_child(preview_rect);

	HBoxContainer *custom_toolbar = memnew(HBoxContainer);
	custom_toolbar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	custom_toolbar->add_theme_constant_override("separation", 8 * EDSCALE);
	content->add_child(custom_toolbar);

	edit_custom_button = memnew(Button);
	edit_custom_button->set_text(TTR("Edit Custom Shader"));
	edit_custom_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsNextMarqueePage::_edit_custom_pressed));
	custom_toolbar->add_child(edit_custom_button);

	save_custom_button = memnew(Button);
	save_custom_button->set_text(TTR("Save Custom"));
	save_custom_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsNextMarqueePage::_save_custom_pressed));
	custom_toolbar->add_child(save_custom_button);

	cancel_custom_button = memnew(Button);
	cancel_custom_button->set_text(TTR("Cancel"));
	cancel_custom_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsNextMarqueePage::_cancel_custom_pressed));
	custom_toolbar->add_child(cancel_custom_button);

	reset_custom_button = memnew(Button);
	reset_custom_button->set_text(TTR("Reset Custom"));
	reset_custom_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsNextMarqueePage::_reset_custom_pressed));
	custom_toolbar->add_child(reset_custom_button);

	status_label = memnew(Label);
	status_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	status_label->hide();
	content->add_child(status_label);

	shader_editor = memnew(TextEdit);
	shader_editor->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	shader_editor->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	shader_editor->set_custom_minimum_size(Size2(0, 260) * EDSCALE);
	shader_editor->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	shader_editor->connect("text_changed", callable_mp(this, &AISettingsNextMarqueePage::_refresh_editor_state));
	shader_editor->hide();
	content->add_child(shader_editor);

	_populate_presets();
	_select_current_preset();
	_refresh_editor_state();
}

void AISettingsNextMarqueePage::_populate_presets() {
	ERR_FAIL_NULL(preset_selector);

	preset_selector->clear();
	Vector<AINextMarqueePreset> presets = AINextMarqueeSettings::get_presets();
	for (int i = 0; i < presets.size(); i++) {
		preset_selector->add_item(presets[i].display_name);
		preset_selector->set_item_metadata(i, presets[i].id);
	}
}

void AISettingsNextMarqueePage::_select_current_preset() {
	ERR_FAIL_NULL(preset_selector);

	const String preset_id = AINextMarqueeSettings::get_current_preset_id();
	for (int i = 0; i < preset_selector->get_item_count(); i++) {
		if (String(preset_selector->get_item_metadata(i)) == preset_id) {
			preset_selector->select(i);
			return;
		}
	}
	if (preset_selector->get_item_count() > 0) {
		preset_selector->select(0);
	}
}

void AISettingsNextMarqueePage::_preset_selected(int p_index) {
	if (!preset_selector || p_index < 0) {
		return;
	}

	const String preset_id = String(preset_selector->get_item_metadata(p_index));
	if (!AINextMarqueeSettings::set_current_preset_id(preset_id)) {
		_set_status(TTR("Could not apply marquee preset."), true);
		return;
	}

	editing_custom = false;
	_set_status(String(), false);
	_refresh_editor_state();
	emit_signal(SNAME("settings_changed"));
}

void AISettingsNextMarqueePage::_edit_custom_pressed() {
	if (!preset_selector || !shader_editor) {
		return;
	}

	AINextMarqueeSettings::set_current_preset_id(AINextMarqueeSettings::CUSTOM_PRESET_ID);
	_select_current_preset();
	shader_editor->set_text(AINextMarqueeSettings::get_custom_shader_code());
	editing_custom = true;
	_set_status(String(), false);
	_refresh_editor_state();
}

void AISettingsNextMarqueePage::_save_custom_pressed() {
	if (!shader_editor) {
		return;
	}

	const String shader_code = shader_editor->get_text();
	if (!AINextMarqueeSettings::set_custom_shader_code(shader_code)) {
		_set_status(TTR("Custom shader cannot be empty."), true);
		return;
	}

	AINextMarqueeSettings::set_current_preset_id(AINextMarqueeSettings::CUSTOM_PRESET_ID);
	editing_custom = false;
	_set_status(String(), false);
	_refresh_editor_state();
	emit_signal(SNAME("settings_changed"));
}

void AISettingsNextMarqueePage::_cancel_custom_pressed() {
	editing_custom = false;
	_set_status(String(), false);
	_refresh_editor_state();
}

void AISettingsNextMarqueePage::_reset_custom_pressed() {
	AINextMarqueePreset default_preset = AINextMarqueeSettings::get_preset("aurora");
	if (default_preset.id.is_empty() || !AINextMarqueeSettings::set_custom_shader_code(default_preset.shader_code)) {
		_set_status(TTR("Could not reset the custom marquee."), true);
		return;
	}

	AINextMarqueeSettings::set_current_preset_id(AINextMarqueeSettings::CUSTOM_PRESET_ID);
	_select_current_preset();
	editing_custom = false;
	_set_status(String(), false);
	_refresh_editor_state();
	emit_signal(SNAME("settings_changed"));
}

void AISettingsNextMarqueePage::_refresh_editor_state() {
	const String preset_id = AINextMarqueeSettings::get_current_preset_id();
	const bool custom_selected = preset_id == AINextMarqueeSettings::CUSTOM_PRESET_ID;

	if (edit_custom_button) {
		edit_custom_button->set_visible(custom_selected && !editing_custom);
	}
	if (save_custom_button) {
		save_custom_button->set_visible(custom_selected && editing_custom);
	}
	if (cancel_custom_button) {
		cancel_custom_button->set_visible(custom_selected && editing_custom);
	}
	if (reset_custom_button) {
		reset_custom_button->set_visible(custom_selected && !editing_custom);
	}
	if (shader_editor) {
		shader_editor->set_visible(custom_selected && editing_custom);
	}

	_apply_preview_shader(editing_custom && shader_editor ? shader_editor->get_text() : AINextMarqueeSettings::get_effective_shader_code());
}

void AISettingsNextMarqueePage::_apply_preview_shader(const String &p_shader_code) {
	if (!preview_rect) {
		return;
	}
	preview_rect->set_material(_make_marquee_material(p_shader_code));
}

void AISettingsNextMarqueePage::_set_status(const String &p_status, bool p_error) {
	if (!status_label) {
		return;
	}
	status_label->set_text(p_status);
	status_label->set_visible(!p_status.is_empty());
	status_label->add_theme_color_override(SceneStringName(font_color), p_error ? get_theme_color(SNAME("error_color"), EditorStringName(Editor)) : get_theme_color(SNAME("success_color"), EditorStringName(Editor)));
}

void AISettingsNextMarqueePage::build_for_test() {
	_build_ui();
}

int AISettingsNextMarqueePage::get_preset_count_for_test() const {
	return preset_selector ? preset_selector->get_item_count() : 0;
}

bool AISettingsNextMarqueePage::is_shader_editor_visible_for_test() const {
	return shader_editor && shader_editor->is_visible();
}

void AISettingsNextMarqueePage::select_preset_for_test(const String &p_preset_id) {
	_build_ui();
	for (int i = 0; preset_selector && i < preset_selector->get_item_count(); i++) {
		if (String(preset_selector->get_item_metadata(i)) == p_preset_id) {
			preset_selector->select(i);
			_preset_selected(i);
			return;
		}
	}
}

void AISettingsNextMarqueePage::edit_custom_for_test() {
	_build_ui();
	_edit_custom_pressed();
}
