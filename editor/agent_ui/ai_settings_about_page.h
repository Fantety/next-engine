/**************************************************************************/
/*  ai_settings_about_page.h                                              */
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

#pragma once

#include "editor/next_engine_update_checker.h"
#include "scene/gui/margin_container.h"

class Button;
class HTTPRequest;
class Label;

class AISettingsAboutPage : public MarginContainer {
	GDCLASS(AISettingsAboutPage, MarginContainer);

	Label *current_version_label = nullptr;
	Label *latest_version_label = nullptr;
	Label *status_label = nullptr;
	Button *check_updates_button = nullptr;
	Button *download_button = nullptr;
	HTTPRequest *http = nullptr;
	NextEngineUpdateCheckStatus update_status = NextEngineUpdateCheckStatus::NOT_CHECKED;

	void _build_ui();
	void _check_updates_pressed();
	void _download_pressed();
	void _http_request_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _apply_update_result(const NextEngineUpdateCheckResult &p_result);
	void _set_status_text(const String &p_text, bool p_error);
	void _set_current_version_label();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AISettingsAboutPage();

	void build_for_test();
	String get_current_version_text_for_test() const;
	String get_latest_version_text_for_test() const;
	NextEngineUpdateCheckStatus get_update_status_for_test() const;
	bool is_download_button_visible_for_test() const;
	void apply_update_response_for_test(int p_result, int p_response_code, const String &p_body, const String &p_current_version = String());
};
