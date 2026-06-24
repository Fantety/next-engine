/**************************************************************************/
/*  ai_settings_custom_instructions_page.cpp                              */
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

#include "ai_settings_custom_instructions_page.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/text_edit.h"
#include "servers/text/text_server.h"

void AISettingsCustomInstructionsPage::_bind_methods() {
	ADD_SIGNAL(MethodInfo("settings_changed"));
}

void AISettingsCustomInstructionsPage::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build_ui();
	}
}

AISettingsCustomInstructionsPage::AISettingsCustomInstructionsPage() {
}

Ref<AIAgentV1UIBridge> AISettingsCustomInstructionsPage::_get_adapter() const {
	return AIAgentV1UIBridge::get_singleton();
}

void AISettingsCustomInstructionsPage::_build_ui() {
	if (instructions_edit) {
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
	title->set_text(TTR("Custom Instructions"));
	title->add_theme_font_size_override(SceneStringName(font_size), int(22 * EDSCALE));
	content->add_child(title);

	Label *section_title = memnew(Label);
	section_title->set_text(TTR("Additional System Instructions"));
	section_title->add_theme_font_size_override(SceneStringName(font_size), int(14 * EDSCALE));
	content->add_child(section_title);

	Label *description = memnew(Label);
	description->set_text(TTR("These instructions are appended to the built-in Agent system prompt. They do not replace default safety, project, or tool guidance."));
	description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	content->add_child(description);

	instructions_edit = memnew(TextEdit);
	instructions_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	instructions_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	instructions_edit->set_custom_minimum_size(Size2(0, 280) * EDSCALE);
	instructions_edit->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	instructions_edit->set_placeholder(TTR("Example: Prefer concise answers. Ask before making broad project changes."));
	content->add_child(instructions_edit);

	HBoxContainer *actions = memnew(HBoxContainer);
	actions->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	actions->add_theme_constant_override("separation", 8 * EDSCALE);
	content->add_child(actions);

	save_button = memnew(Button);
	save_button->set_text(TTR("Save Instructions"));
	save_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsCustomInstructionsPage::_save_pressed));
	actions->add_child(save_button);

	status_label = memnew(Label);
	status_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	status_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	status_label->hide();
	actions->add_child(status_label);

	Ref<AIAgentV1UIBridge> bridge = _get_adapter();
	if (bridge.is_valid()) {
		const Callable config_changed = callable_mp(this, &AISettingsCustomInstructionsPage::_config_changed);
		if (!bridge->is_connected(SNAME("config_changed"), config_changed)) {
			bridge->connect(SNAME("config_changed"), config_changed);
		}
	}

	_load_custom_instructions();
}

void AISettingsCustomInstructionsPage::_load_custom_instructions() {
	if (!instructions_edit) {
		return;
	}

	loading = true;
	instructions_edit->set_text(_get_custom_instructions());
	loading = false;
}

String AISettingsCustomInstructionsPage::_get_custom_instructions() const {
	Ref<AIAgentV1UIBridge> bridge = _get_adapter();
	if (bridge.is_null()) {
		return String();
	}
	const Dictionary snapshot = bridge->get_settings_snapshot();
	return String(snapshot.get("custom_instructions", String()));
}

bool AISettingsCustomInstructionsPage::_patch_custom_instructions(const String &p_instructions, const String &p_scope) {
	Ref<AIAgentV1UIBridge> bridge = _get_adapter();
	if (bridge.is_null()) {
		return false;
	}

	Dictionary main_patch;
	main_patch["custom_instructions"] = p_instructions.strip_edges();
	Dictionary agents_patch;
	agents_patch["main"] = main_patch;
	Dictionary patch;
	patch["agents"] = agents_patch;
	const Dictionary result = bridge->patch_settings(patch, p_scope);
	return bool(result.get("success", false));
}

void AISettingsCustomInstructionsPage::_save_pressed() {
	if (!instructions_edit || loading) {
		return;
	}

	if (!_patch_custom_instructions(instructions_edit->get_text())) {
		_set_status(TTR("Could not save custom instructions."), true);
		return;
	}

	_set_status(TTR("Custom instructions saved."), false);
	emit_signal(SNAME("settings_changed"));
}

void AISettingsCustomInstructionsPage::_config_changed(const String &p_scope, const Dictionary &p_config) {
	(void)p_scope;
	(void)p_config;
	_load_custom_instructions();
}

void AISettingsCustomInstructionsPage::_set_status(const String &p_status, bool p_error) {
	if (!status_label) {
		return;
	}
	status_label->set_text(p_status);
	status_label->set_visible(!p_status.is_empty());
	status_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(p_error ? SNAME("error_color") : SNAME("success_color"), EditorStringName(Editor)));
}

void AISettingsCustomInstructionsPage::build_for_test() {
	_build_ui();
}

void AISettingsCustomInstructionsPage::set_custom_instructions_for_test(const String &p_instructions) {
	_build_ui();
	if (instructions_edit) {
		instructions_edit->set_text(p_instructions);
	}
	(void)_patch_custom_instructions(p_instructions, "runtime");
	_load_custom_instructions();
}

String AISettingsCustomInstructionsPage::get_custom_instructions_for_test() const {
	return _get_custom_instructions();
}
