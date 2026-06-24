/**************************************************************************/
/*  ai_settings_models_page.cpp                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "ai_settings_models_page.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/agent_ui/component/ai_model_profile_dialog.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/texture_rect.h"

namespace {

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
	if (p_width > 0) {
		label->set_custom_minimum_size(Size2(p_width, 0) * EDSCALE);
	} else {
		label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	}
	return label;
}

} // namespace

void AISettingsModelsPage::_bind_methods() {
	ADD_SIGNAL(MethodInfo("settings_changed"));
}

void AISettingsModelsPage::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build_ui();
	}
}

AISettingsModelsPage::AISettingsModelsPage() {
}

Ref<AIAgentV1UIBridge> AISettingsModelsPage::_get_adapter() {
	return AIAgentV1UIBridge::get_singleton();
}

void AISettingsModelsPage::_build_ui() {
	if (model_table) {
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
	title->set_text(TTR("Models"));
	title->add_theme_font_size_override(SceneStringName(font_size), int(22 * EDSCALE));
	content->add_child(title);

	Label *section_title = memnew(Label);
	section_title->set_text(TTR("Model Management"));
	section_title->add_theme_font_size_override(SceneStringName(font_size), int(14 * EDSCALE));
	content->add_child(section_title);

	Label *description = memnew(Label);
	description->set_text(TTR("Configure API keys and add available models. Preset models use stable versions by default."));
	description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	content->add_child(description);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	toolbar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(toolbar);

	add_model_button = memnew(Button);
	add_model_button->set_text(TTR("Add Model"));
	add_model_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsModelsPage::_popup_add_model_dialog));
	toolbar->add_child(add_model_button);

	PanelContainer *table_panel = memnew(PanelContainer);
	table_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	table_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(table_panel);

	model_table = memnew(VBoxContainer);
	model_table->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	model_table->add_theme_constant_override("separation", 0);
	table_panel->add_child(model_table);

	profile_dialog = memnew(AIModelProfileDialog);
	profile_dialog->set_model_provider_presets(_get_adapter()->list_model_provider_presets());
	profile_dialog->connect("profile_submitted", callable_mp(this, &AISettingsModelsPage::_profile_submitted));
	add_child(profile_dialog);

	_refresh_model_table();
}

void AISettingsModelsPage::_refresh_model_table() {
	ERR_FAIL_NULL(model_table);

	_clear_children(model_table);
	model_table_rows.clear();

	HBoxContainer *header = memnew(HBoxContainer);
	header->set_custom_minimum_size(Size2(0, 32) * EDSCALE);
	header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header->add_theme_constant_override("separation", 8 * EDSCALE);
	model_table->add_child(header);

	Label *name_header = _make_table_label(TTR("Name"));
	name_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	header->add_child(name_header);

	Label *provider_header = _make_table_label(TTR("Provider"), 170);
	provider_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	header->add_child(provider_header);

	Label *model_header = _make_table_label(TTR("Model"), 180);
	model_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	header->add_child(model_header);

	Label *action_header = _make_table_label(TTR("Actions"), 150);
	action_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	header->add_child(action_header);

	HSeparator *header_separator = memnew(HSeparator);
	model_table->add_child(header_separator);

	const Array profiles = _get_adapter()->list_model_profiles(true);
	int model_count = 0;
	for (int i = 0; i < profiles.size(); i++) {
		if (profiles[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		_add_model_table_row(profiles[i]);
		model_count++;
	}

	if (model_count == 0) {
		MarginContainer *empty_margin = memnew(MarginContainer);
		empty_margin->add_theme_constant_override("margin_left", 28 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_bottom", 10 * EDSCALE);
		model_table->add_child(empty_margin);

		Label *empty_label = memnew(Label);
		empty_label->set_text(TTR("No models yet. Add a model to make it available to the AI Agent."));
		empty_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
		empty_margin->add_child(empty_label);
	}
}

void AISettingsModelsPage::_add_model_table_row(const Dictionary &p_profile) {
	ERR_FAIL_NULL(model_table);

	ModelTableRow row_data;
	row_data.profile_id = String(p_profile.get("id", String()));
	row_data.display_name = String(p_profile.get("display_name", String()));
	row_data.provider_id = String(p_profile.get("provider_id", String()));
	row_data.model = String(p_profile.get("model", String()));
	row_data.custom = bool(p_profile.get("custom", false));
	model_table_rows.push_back(row_data);

	HBoxContainer *row = memnew(HBoxContainer);
	row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row->set_custom_minimum_size(Size2(0, 36) * EDSCALE);
	row->add_theme_constant_override("separation", 8 * EDSCALE);
	model_table->add_child(row);

	HBoxContainer *model_cell = memnew(HBoxContainer);
	model_cell->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	model_cell->add_theme_constant_override("separation", 8 * EDSCALE);
	row->add_child(model_cell);

	TextureRect *icon = memnew(TextureRect);
	icon->set_texture(get_editor_theme_icon(SNAME("LLM")));
	icon->set_custom_minimum_size(Size2(14, 14) * EDSCALE);
	icon->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	model_cell->add_child(icon);

	Label *name_label = memnew(Label);
	name_label->set_text(row_data.display_name);
	name_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	name_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	model_cell->add_child(name_label);

	Label *provider_label = _make_table_label(String(p_profile.get("provider_name", row_data.provider_id)), 170);
	row->add_child(provider_label);

	Label *model_label = _make_table_label(row_data.model, 180);
	row->add_child(model_label);

	HBoxContainer *action_cell = memnew(HBoxContainer);
	action_cell->set_custom_minimum_size(Size2(150, 0) * EDSCALE);
	action_cell->add_theme_constant_override("separation", 6 * EDSCALE);
	row->add_child(action_cell);

	Button *edit_button = memnew(Button);
	edit_button->set_text(TTR("Edit"));
	edit_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsModelsPage::_edit_model_pressed).bind(row_data.profile_id), CONNECT_DEFERRED);
	action_cell->add_child(edit_button);

	Button *remove_button = memnew(Button);
	remove_button->set_text(TTR("Remove"));
	remove_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsModelsPage::_remove_model_pressed).bind(row_data.profile_id), CONNECT_DEFERRED);
	action_cell->add_child(remove_button);

	HSeparator *row_separator = memnew(HSeparator);
	model_table->add_child(row_separator);
}

void AISettingsModelsPage::_popup_add_model_dialog() {
	ERR_FAIL_NULL(profile_dialog);
	profile_dialog->popup_add_model();
}

void AISettingsModelsPage::_edit_model_pressed(const String &p_profile_id) {
	ERR_FAIL_NULL(profile_dialog);

	Dictionary profile = _get_adapter()->get_model_profile(p_profile_id);
	if (profile.is_empty()) {
		return;
	}

	profile_dialog->popup_edit_model(profile);
}

void AISettingsModelsPage::_profile_submitted() {
	ERR_FAIL_NULL(profile_dialog);

	Dictionary profile = profile_dialog->get_submitted_profile();
	if (String(profile.get("provider_id", String())).is_empty() || String(profile.get("model", String())).is_empty()) {
		return;
	}

	if (profile_dialog->is_editing_model()) {
		(void)_get_adapter()->update_model_profile(profile_dialog->get_editing_profile_id(), profile, model_profile_scope);
	} else {
		(void)_get_adapter()->add_model_profile(profile, model_profile_scope);
	}

	_refresh_model_table();
	emit_signal(SNAME("settings_changed"));
	profile_dialog->hide();
}

void AISettingsModelsPage::_remove_model_pressed(const String &p_profile_id) {
	(void)_get_adapter()->remove_model_profile(p_profile_id, model_profile_scope);
	_refresh_model_table();
	emit_signal(SNAME("settings_changed"));
}

Dictionary AISettingsModelsPage::_find_model_profile(const String &p_provider_id, const String &p_model, bool p_custom_only) const {
	Ref<AIAgentV1UIBridge> bridge = AIAgentV1UIBridge::get_singleton();
	const Array profiles = bridge->list_model_profiles(false);
	for (int i = 0; i < profiles.size(); i++) {
		if (profiles[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary profile = profiles[i];
		if (String(profile.get("provider_id", String())) != p_provider_id || String(profile.get("model", String())) != p_model) {
			continue;
		}
		if (!p_custom_only || bool(profile.get("custom", false))) {
			return profile;
		}
	}
	return Dictionary();
}

void AISettingsModelsPage::build_for_test() {
	_build_ui();
}

int AISettingsModelsPage::get_model_table_row_count_for_test() const {
	return model_table_rows.size();
}

int AISettingsModelsPage::get_custom_model_table_row_count_for_test() const {
	int count = 0;
	for (int i = 0; i < model_table_rows.size(); i++) {
		if (model_table_rows[i].custom) {
			count++;
		}
	}
	return count;
}

void AISettingsModelsPage::add_provider_model_for_test(const String &p_provider_id, const String &p_model, const String &p_api_key) {
	Dictionary profile;
	profile["provider_id"] = p_provider_id;
	profile["model"] = p_model;
	profile["api_key"] = p_api_key;
	profile["custom"] = false;
	(void)_get_adapter()->add_model_profile(profile, "runtime");
	_refresh_model_table();
}

void AISettingsModelsPage::add_custom_model_for_test(const String &p_model, const String &p_base_url, const String &p_api_key) {
	Dictionary profile;
	profile["provider_id"] = "compatible";
	profile["model"] = p_model;
	profile["base_url"] = p_base_url;
	profile["api_key"] = p_api_key;
	profile["custom"] = true;
	(void)_get_adapter()->add_model_profile(profile, "runtime");
	_refresh_model_table();
}

void AISettingsModelsPage::edit_provider_model_for_test(const String &p_provider_id, const String &p_model, const String &p_api_key) {
	Dictionary existing = _find_model_profile(p_provider_id, p_model, false);
	Dictionary profile;
	profile["provider_id"] = p_provider_id;
	profile["model"] = p_model;
	profile["api_key"] = p_api_key;
	profile["custom"] = false;
	if (!existing.is_empty() && !bool(existing.get("custom", false))) {
		(void)_get_adapter()->update_model_profile(String(existing.get("id", String())), profile, "runtime");
	} else {
		(void)_get_adapter()->add_model_profile(profile, "runtime");
	}
	_refresh_model_table();
}

void AISettingsModelsPage::edit_custom_model_for_test(const String &p_current_model, const String &p_new_model, const String &p_base_url, const String &p_api_key) {
	Dictionary existing = _find_model_profile("compatible", p_current_model, true);
	Dictionary profile;
	profile["provider_id"] = "compatible";
	profile["model"] = p_new_model;
	profile["base_url"] = p_base_url;
	profile["api_key"] = p_api_key;
	profile["custom"] = true;
	if (!existing.is_empty()) {
		(void)_get_adapter()->update_model_profile(String(existing.get("id", String())), profile, "runtime");
	} else {
		(void)_get_adapter()->add_model_profile(profile, "runtime");
	}
	_refresh_model_table();
}

void AISettingsModelsPage::remove_custom_model_for_test(const String &p_provider_id, const String &p_model) {
	Dictionary profile = _find_model_profile(p_provider_id, p_model, true);
	if (!profile.is_empty()) {
		(void)_get_adapter()->remove_model_profile(String(profile.get("id", String())), "runtime");
		_refresh_model_table();
	}
}
