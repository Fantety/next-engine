/**************************************************************************/
/*  ai_settings_about_page.h                                               */
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
