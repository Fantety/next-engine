/**************************************************************************/
/*  ai_settings_about_page.cpp                                            */
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

#include "ai_settings_about_page.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/version.h"
#include "editor/editor_string_names.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "scene/main/http_request.h"
#include "servers/text/text_server.h"

void AISettingsAboutPage::_bind_methods() {
}

void AISettingsAboutPage::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build_ui();
	}
}

AISettingsAboutPage::AISettingsAboutPage() {
	http = memnew(HTTPRequest);
	http->set_timeout(10.0);
	add_child(http);
	http->connect("request_completed", callable_mp(this, &AISettingsAboutPage::_http_request_completed));
}

void AISettingsAboutPage::_build_ui() {
	if (current_version_label) {
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
	title->set_text(TTR("About"));
	title->add_theme_font_size_override(SceneStringName(font_size), int(22 * EDSCALE));
	content->add_child(title);

	Label *section_title = memnew(Label);
	section_title->set_text(TTR("NEXT Engine"));
	section_title->add_theme_font_size_override(SceneStringName(font_size), int(14 * EDSCALE));
	content->add_child(section_title);

	Label *description = memnew(Label);
	description->set_text(TTR("NEXT Engine is built on Godot Engine and adds integrated AI Agent workflows."));
	description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	content->add_child(description);

	PanelContainer *version_panel = memnew(PanelContainer);
	version_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(version_panel);

	VBoxContainer *version_content = memnew(VBoxContainer);
	version_content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	version_content->add_theme_constant_override("separation", 8 * EDSCALE);
	version_panel->add_child(version_content);

	current_version_label = memnew(Label);
	current_version_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	version_content->add_child(current_version_label);
	_set_current_version_label();

	latest_version_label = memnew(Label);
	latest_version_label->set_text(TTR("Latest version: Not checked"));
	latest_version_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	version_content->add_child(latest_version_label);

	status_label = memnew(Label);
	status_label->set_text(TTR("Update status: Not checked."));
	status_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	status_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	status_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	version_content->add_child(status_label);

	HSeparator *separator = memnew(HSeparator);
	version_content->add_child(separator);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	toolbar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	toolbar->add_theme_constant_override("separation", 8 * EDSCALE);
	version_content->add_child(toolbar);

	check_updates_button = memnew(Button);
	check_updates_button->set_text(TTR("Check for Updates"));
	check_updates_button->set_button_icon(get_editor_theme_icon(SNAME("HTTPRequest")));
	check_updates_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsAboutPage::_check_updates_pressed));
	toolbar->add_child(check_updates_button);

	download_button = memnew(Button);
	download_button->set_text(TTR("Open Download Page"));
	download_button->set_button_icon(get_editor_theme_icon(SNAME("ExternalLink")));
	download_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsAboutPage::_download_pressed));
	download_button->hide();
	toolbar->add_child(download_button);

	if (EditorSettings::get_singleton()) {
		http->set_https_proxy(EDITOR_GET("network/http_proxy/host"), EDITOR_GET("network/http_proxy/port"));
	}
}

void AISettingsAboutPage::_set_current_version_label() {
	if (current_version_label) {
		current_version_label->set_text(vformat(TTR("Current version: %s"), String(NEXT_VERSION_FULL_CONFIG)));
	}
}

void AISettingsAboutPage::_set_status_text(const String &p_text, bool p_error) {
	ERR_FAIL_NULL(status_label);
	status_label->set_text(p_text);
	status_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(p_error ? SNAME("error_color") : SNAME("disabled_font_color"), EditorStringName(Editor)));
}

void AISettingsAboutPage::_check_updates_pressed() {
	_build_ui();
	download_button->hide();
	if (EditorSettings::get_singleton() && int(EDITOR_GET("network/connection/network_mode")) == EditorSettings::NETWORK_OFFLINE) {
		update_status = NextEngineUpdateCheckStatus::ERROR;
		latest_version_label->set_text(TTR("Latest version: Unknown"));
		_set_status_text(TTR("Offline mode, update checks disabled."), true);
		return;
	}

	check_updates_button->set_disabled(true);
	latest_version_label->set_text(TTR("Latest version: Checking..."));
	_set_status_text(TTR("Checking for updates..."), false);

	const Error err = request_next_engine_latest_release(http);
	if (err != OK) {
		NextEngineUpdateCheckResult result;
		result.status = NextEngineUpdateCheckStatus::ERROR;
		result.message = vformat(TTR("Failed to check for updates. Error: %d."), err);
		_apply_update_result(result);
	}
}

void AISettingsAboutPage::_download_pressed() {
	OS::get_singleton()->shell_open(get_next_engine_download_url());
}

void AISettingsAboutPage::_http_request_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	_apply_update_result(parse_next_engine_update_response(p_result, p_response_code, p_body));
}

void AISettingsAboutPage::_apply_update_result(const NextEngineUpdateCheckResult &p_result) {
	_build_ui();
	check_updates_button->set_disabled(false);
	update_status = p_result.status;

	switch (p_result.status) {
		case NextEngineUpdateCheckStatus::UPDATE_AVAILABLE: {
			latest_version_label->set_text(vformat(TTR("Latest version: %s"), p_result.latest_version));
			_set_status_text(vformat(TTR("Update available: %s."), p_result.latest_version), false);
			download_button->show();
		} break;

		case NextEngineUpdateCheckStatus::UP_TO_DATE: {
			latest_version_label->set_text(p_result.latest_version.is_empty() ? TTR("Latest version: Current") : vformat(TTR("Latest version: %s"), p_result.latest_version));
			_set_status_text(TTR("You are using the latest version."), false);
			download_button->hide();
		} break;

		case NextEngineUpdateCheckStatus::ERROR: {
			latest_version_label->set_text(TTR("Latest version: Unknown"));
			_set_status_text(p_result.message.is_empty() ? TTR("Failed to check for updates.") : p_result.message, true);
			download_button->hide();
		} break;

		case NextEngineUpdateCheckStatus::NOT_CHECKED: {
			latest_version_label->set_text(TTR("Latest version: Not checked"));
			_set_status_text(TTR("Update status: Not checked."), false);
			download_button->hide();
		} break;
	}
}

void AISettingsAboutPage::build_for_test() {
	_build_ui();
}

String AISettingsAboutPage::get_current_version_text_for_test() const {
	return current_version_label ? current_version_label->get_text() : String();
}

String AISettingsAboutPage::get_latest_version_text_for_test() const {
	return latest_version_label ? latest_version_label->get_text() : String();
}

NextEngineUpdateCheckStatus AISettingsAboutPage::get_update_status_for_test() const {
	return update_status;
}

bool AISettingsAboutPage::is_download_button_visible_for_test() const {
	return download_button && download_button->is_visible();
}

void AISettingsAboutPage::apply_update_response_for_test(int p_result, int p_response_code, const String &p_body, const String &p_current_version) {
	_build_ui();
	_apply_update_result(parse_next_engine_update_response(p_result, p_response_code, p_body.to_utf8_buffer(), p_current_version));
}
