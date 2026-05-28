/**************************************************************************/
/*  editor_user_login_dialog.h                                            */
/**************************************************************************/

#pragma once

#include "editor/user_system/editor_user_manager.h"

#include "scene/gui/dialogs.h"

class Button;
class Label;
class LineEdit;
class OptionButton;
class TabContainer;
class TextureRect;
class Timer;

class EditorUserLoginDialog : public ConfirmationDialog {
	GDCLASS(EditorUserLoginDialog, ConfirmationDialog);

	Ref<EditorUserManager> manager;

	TextureRect *header_icon = nullptr;
	Label *title_label = nullptr;
	Label *subtitle_label = nullptr;
	TabContainer *tabs = nullptr;
	OptionButton *phone_code_country = nullptr;
	LineEdit *phone_code_phone = nullptr;
	LineEdit *phone_code_code = nullptr;
	Button *send_code_button = nullptr;
	Button *phone_code_login_button = nullptr;
	OptionButton *password_country = nullptr;
	LineEdit *password_phone = nullptr;
	LineEdit *password_password = nullptr;
	Button *password_login_button = nullptr;
	Label *status_label = nullptr;
	Label *provider_label = nullptr;
	Timer *cooldown_timer = nullptr;
	int send_code_cooldown = 0;

	void _build_ui();
	void _update_theme();
	void _update_actions();
	void _set_status(const String &p_text, bool p_error);
	void _fields_changed(const String &p_text);
	void _send_code_pressed();
	void _phone_code_login_pressed();
	void _password_login_pressed();
	void _cooldown_timeout();
	String _get_phone_code_phone() const;
	String _get_password_phone() const;
	bool _can_submit_phone_code() const;
	bool _can_submit_password() const;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_manager(const Ref<EditorUserManager> &p_manager);
	Ref<EditorUserManager> get_manager() const;
	void popup_login();

	void build_for_test();
	void set_phone_code_fields_for_test(const String &p_phone, const String &p_code);
	void send_phone_code_for_test();
	bool can_submit_phone_code_for_test() const;
	String get_phone_code_phone_for_test() const;
	String get_send_code_button_text_for_test() const;
	bool is_send_code_button_disabled_for_test() const;
	int get_send_code_cooldown_for_test() const;
	static String format_phone_for_test(const String &p_country_code, const String &p_phone);

	EditorUserLoginDialog();
};
