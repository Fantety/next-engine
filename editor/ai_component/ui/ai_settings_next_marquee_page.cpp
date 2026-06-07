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
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/margin_container.h"
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

void _clear_children(Node *p_node) {
	ERR_FAIL_NULL(p_node);
	while (p_node->get_child_count() > 0) {
		Node *child = p_node->get_child(0);
		p_node->remove_child(child);
		memdelete(child);
	}
}

Label *_make_table_label(const String &p_text, int p_width = 0) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	label->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	label->set_clip_text(true);
	if (p_width > 0) {
		label->set_custom_minimum_size(Size2(p_width, 0) * EDSCALE);
	} else {
		label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	}
	return label;
}

} // namespace

void AISettingsNextMarqueePage::_bind_methods() {
	ADD_SIGNAL(MethodInfo("settings_changed"));
}

void AISettingsNextMarqueePage::_notification(int p_what) {
	if (p_what == NOTIFICATION_EXIT_TREE || p_what == NOTIFICATION_PREDELETE) {
		if (selected_preview_rect) {
			selected_preview_rect->set_material(Ref<Material>());
		}
		if (preview_rect) {
			preview_rect->set_material(Ref<Material>());
		}
	}

	if (p_what == NOTIFICATION_READY) {
		_build_ui();
	}
}

AISettingsNextMarqueePage::AISettingsNextMarqueePage() {
}

Size2 AISettingsNextMarqueePage::get_minimum_size() const {
	return Size2();
}

void AISettingsNextMarqueePage::_build_ui() {
	if (marquee_table) {
		return;
	}

	add_theme_constant_override("margin_left", 8 * EDSCALE);
	add_theme_constant_override("margin_right", 8 * EDSCALE);
	add_theme_constant_override("margin_top", 8 * EDSCALE);
	add_theme_constant_override("margin_bottom", 8 * EDSCALE);

	ScrollContainer *page_scroll = memnew(ScrollContainer);
	page_scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	add_child(page_scroll);

	VBoxContainer *content = memnew(VBoxContainer);
	content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	content->add_theme_constant_override("separation", 12 * EDSCALE);
	page_scroll->add_child(content);

	Label *title = memnew(Label);
	title->set_text(TTR("NEXT Marquee"));
	title->add_theme_font_size_override(SceneStringName(font_size), int(22 * EDSCALE));
	content->add_child(title);

	Label *description = memnew(Label);
	description->set_text(TTR("Choose the loading marquee shown while the AI Agent is working. Presets are read-only; custom marquees are added from the button above the list."));
	description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	content->add_child(description);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	toolbar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	toolbar->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	toolbar->add_theme_constant_override("separation", 8 * EDSCALE);
	content->add_child(toolbar);

	add_marquee_button = memnew(Button);
	add_marquee_button->set_text(TTR("Add Marquee"));
	add_marquee_button->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	add_marquee_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsNextMarqueePage::_popup_add_marquee_dialog));
	toolbar->add_child(add_marquee_button);

	PanelContainer *preview_panel = memnew(PanelContainer);
	preview_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	preview_panel->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	content->add_child(preview_panel);

	MarginContainer *preview_margin = memnew(MarginContainer);
	preview_margin->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	preview_margin->add_theme_constant_override("margin_left", 16 * EDSCALE);
	preview_margin->add_theme_constant_override("margin_right", 16 * EDSCALE);
	preview_margin->add_theme_constant_override("margin_top", 18 * EDSCALE);
	preview_margin->add_theme_constant_override("margin_bottom", 18 * EDSCALE);
	preview_panel->add_child(preview_margin);

	selected_preview_rect = memnew(ColorRect);
	selected_preview_rect->set_color(Color(1, 1, 1, 1));
	selected_preview_rect->set_custom_minimum_size(Size2(0, 12) * EDSCALE);
	selected_preview_rect->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	selected_preview_rect->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	preview_margin->add_child(selected_preview_rect);

	status_label = memnew(Label);
	status_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	status_label->hide();
	content->add_child(status_label);

	PanelContainer *table_panel = memnew(PanelContainer);
	table_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	table_panel->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	content->add_child(table_panel);

	marquee_table = memnew(VBoxContainer);
	marquee_table->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	marquee_table->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	marquee_table->add_theme_constant_override("separation", 0);
	table_panel->add_child(marquee_table);

	_refresh_marquee_table();
	_select_current_marquee();
}

void AISettingsNextMarqueePage::_build_add_dialog() {
	if (marquee_dialog) {
		return;
	}

	marquee_dialog = memnew(ConfirmationDialog);
	marquee_dialog->set_title(TTR("Add NEXT Marquee"));
	marquee_dialog->set_ok_button_text(TTR("Save"));
	marquee_dialog->set_hide_on_ok(false);
	marquee_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AISettingsNextMarqueePage::_add_marquee_confirmed));
	add_child(marquee_dialog);

	VBoxContainer *dialog_content = memnew(VBoxContainer);
	dialog_content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	dialog_content->add_theme_constant_override("separation", 10 * EDSCALE);
	marquee_dialog->add_child(dialog_content);

	name_edit = memnew(LineEdit);
	name_edit->set_placeholder(TTR("Marquee name"));
	name_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	dialog_content->add_child(name_edit);

	PanelContainer *dialog_preview_panel = memnew(PanelContainer);
	dialog_preview_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	dialog_content->add_child(dialog_preview_panel);

	MarginContainer *dialog_preview_margin = memnew(MarginContainer);
	dialog_preview_margin->add_theme_constant_override("margin_left", 12 * EDSCALE);
	dialog_preview_margin->add_theme_constant_override("margin_right", 12 * EDSCALE);
	dialog_preview_margin->add_theme_constant_override("margin_top", 14 * EDSCALE);
	dialog_preview_margin->add_theme_constant_override("margin_bottom", 14 * EDSCALE);
	dialog_preview_panel->add_child(dialog_preview_margin);

	preview_rect = memnew(ColorRect);
	preview_rect->set_color(Color(1, 1, 1, 1));
	preview_rect->set_custom_minimum_size(Size2(0, 12) * EDSCALE);
	preview_rect->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	dialog_preview_margin->add_child(preview_rect);

	shader_editor = memnew(TextEdit);
	shader_editor->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	shader_editor->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	shader_editor->set_custom_minimum_size(Size2(620, 300) * EDSCALE);
	shader_editor->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	shader_editor->connect("text_changed", callable_mp(this, &AISettingsNextMarqueePage::_dialog_shader_changed));
	dialog_content->add_child(shader_editor);
}

void AISettingsNextMarqueePage::_refresh_marquee_table() {
	ERR_FAIL_NULL(marquee_table);

	_clear_children(marquee_table);
	marquee_rows.clear();

	HBoxContainer *header = memnew(HBoxContainer);
	header->set_custom_minimum_size(Size2(0, 32) * EDSCALE);
	header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	header->add_theme_constant_override("separation", 8 * EDSCALE);
	marquee_table->add_child(header);

	Label *name_header = _make_table_label(TTR("Name"), 190);
	name_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(name_header);

	Label *preview_header = _make_table_label(TTR("Preview"));
	preview_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(preview_header);

	Label *type_header = _make_table_label(TTR("Type"), 110);
	type_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(type_header);

	Label *action_header = _make_table_label(TTR("Actions"), 110);
	action_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(action_header);

	HSeparator *header_separator = memnew(HSeparator);
	marquee_table->add_child(header_separator);

	const String current_id = AINextMarqueeSettings::get_current_preset_id();
	Vector<AINextMarqueePreset> marquees = AINextMarqueeSettings::get_presets();
	for (int i = 0; i < marquees.size(); i++) {
		_add_marquee_table_row(marquees[i], current_id);
	}
}

void AISettingsNextMarqueePage::_add_marquee_table_row(const AINextMarqueePreset &p_marquee, const String &p_current_id) {
	ERR_FAIL_NULL(marquee_table);

	marquee_rows.push_back(p_marquee);
	const bool current = p_marquee.id == p_current_id;

	HBoxContainer *row = memnew(HBoxContainer);
	row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	row->set_custom_minimum_size(Size2(0, 42) * EDSCALE);
	row->add_theme_constant_override("separation", 8 * EDSCALE);
	marquee_table->add_child(row);

	Button *name_button = memnew(Button);
	name_button->set_text(p_marquee.display_name);
	name_button->set_tooltip_text(p_marquee.display_name);
	name_button->set_custom_minimum_size(Size2(190, 0) * EDSCALE);
	name_button->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	name_button->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	name_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsNextMarqueePage::_marquee_selected).bind(p_marquee.id), CONNECT_DEFERRED);
	row->add_child(name_button);

	MarginContainer *preview_margin = memnew(MarginContainer);
	preview_margin->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	preview_margin->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	preview_margin->add_theme_constant_override("margin_top", 13 * EDSCALE);
	preview_margin->add_theme_constant_override("margin_bottom", 13 * EDSCALE);
	row->add_child(preview_margin);

	ColorRect *row_preview = memnew(ColorRect);
	row_preview->set_color(Color(1, 1, 1, 1));
	row_preview->set_custom_minimum_size(Size2(0, 8) * EDSCALE);
	row_preview->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row_preview->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	row_preview->set_material(_make_marquee_material(p_marquee.shader_code));
	preview_margin->add_child(row_preview);

	Label *type_label = _make_table_label(p_marquee.custom ? TTR("Custom") : TTR("Preset"), 110);
	row->add_child(type_label);

	Button *use_button = memnew(Button);
	use_button->set_text(current ? TTR("Current") : TTR("Use"));
	use_button->set_disabled(current);
	use_button->set_custom_minimum_size(Size2(110, 0) * EDSCALE);
	use_button->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	use_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsNextMarqueePage::_marquee_selected).bind(p_marquee.id), CONNECT_DEFERRED);
	row->add_child(use_button);

	HSeparator *row_separator = memnew(HSeparator);
	marquee_table->add_child(row_separator);
}

void AISettingsNextMarqueePage::_select_current_marquee() {
	const String current_id = AINextMarqueeSettings::get_current_preset_id();
	for (int i = 0; i < marquee_rows.size(); i++) {
		if (marquee_rows[i].id == current_id) {
			_apply_selected_preview_shader(AINextMarqueeSettings::get_preset(current_id).shader_code);
			return;
		}
	}

	if (!marquee_rows.is_empty()) {
		const String first_id = marquee_rows[0].id;
		AINextMarqueeSettings::set_current_preset_id(first_id);
		_apply_selected_preview_shader(AINextMarqueeSettings::get_preset(first_id).shader_code);
		_refresh_marquee_table();
	}
}

void AISettingsNextMarqueePage::_marquee_selected(const String &p_marquee_id) {
	if (!AINextMarqueeSettings::set_current_preset_id(p_marquee_id)) {
		_set_status(TTR("Could not apply marquee."), true);
		return;
	}

	_set_status(String(), false);
	_apply_selected_preview_shader(AINextMarqueeSettings::get_preset(p_marquee_id).shader_code);
	_refresh_marquee_table();
	emit_signal(SNAME("settings_changed"));
}

void AISettingsNextMarqueePage::_popup_add_marquee_dialog() {
	if (!marquee_dialog) {
		_build_add_dialog();
	}
	ERR_FAIL_NULL(marquee_dialog);
	ERR_FAIL_NULL(name_edit);
	ERR_FAIL_NULL(shader_editor);

	name_edit->clear();
	shader_editor->set_text(AINextMarqueeSettings::get_preset("aurora").shader_code);
	_apply_dialog_preview_shader(shader_editor->get_text());
	marquee_dialog->popup_centered(Size2(760, 520) * EDSCALE);
}

void AISettingsNextMarqueePage::_add_marquee_confirmed() {
	ERR_FAIL_NULL(name_edit);
	ERR_FAIL_NULL(shader_editor);

	const String marquee_id = AINextMarqueeSettings::add_custom_marquee(name_edit->get_text(), shader_editor->get_text());
	if (marquee_id.is_empty()) {
		_set_status(TTR("Custom marquee shader cannot be empty."), true);
		return;
	}

	AINextMarqueeSettings::set_current_preset_id(marquee_id);
	_refresh_marquee_table();
	_select_current_marquee();
	if (marquee_dialog) {
		marquee_dialog->hide();
	}
	_set_status(String(), false);
	emit_signal(SNAME("settings_changed"));
}

void AISettingsNextMarqueePage::_dialog_shader_changed() {
	if (!shader_editor) {
		return;
	}
	_apply_dialog_preview_shader(shader_editor->get_text());
}

void AISettingsNextMarqueePage::_apply_selected_preview_shader(const String &p_shader_code) {
	if (!selected_preview_rect) {
		return;
	}
	selected_preview_rect->set_material(_make_marquee_material(p_shader_code));
}

void AISettingsNextMarqueePage::_apply_dialog_preview_shader(const String &p_shader_code) {
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
	return marquee_rows.size();
}

void AISettingsNextMarqueePage::select_preset_for_test(const String &p_preset_id) {
	_build_ui();
	for (int i = 0; i < marquee_rows.size(); i++) {
		if (marquee_rows[i].id == p_preset_id) {
			_marquee_selected(p_preset_id);
			return;
		}
	}
}

String AISettingsNextMarqueePage::add_marquee_for_test(const String &p_display_name, const String &p_shader_code) {
	_build_ui();
	const String marquee_id = AINextMarqueeSettings::add_custom_marquee(p_display_name, p_shader_code);
	if (!marquee_id.is_empty()) {
		AINextMarqueeSettings::set_current_preset_id(marquee_id);
		_refresh_marquee_table();
		_select_current_marquee();
		emit_signal(SNAME("settings_changed"));
	}
	return marquee_id;
}
