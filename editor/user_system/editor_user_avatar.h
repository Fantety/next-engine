/**************************************************************************/
/*  editor_user_avatar.h                                                  */
/**************************************************************************/

#pragma once

#include "editor/user_system/editor_user_login_dialog.h"

#include "scene/gui/box_container.h"

class AcceptDialog;
class Button;
class Label;
class TextureRect;

class EditorUserAvatar : public HBoxContainer {
	GDCLASS(EditorUserAvatar, HBoxContainer);

	Ref<EditorUserManager> manager;
	Button *account_button = nullptr;
	Label *name_label = nullptr;
	TextureRect *score_icon = nullptr;
	Label *score_label = nullptr;
	AcceptDialog *details_dialog = nullptr;
	Label *details_name_value = nullptr;
	Label *details_phone_value = nullptr;
	Label *details_user_id_value = nullptr;
	Label *details_score_value = nullptr;
	EditorUserLoginDialog *login_dialog = nullptr;

	void _build_ui();
	void _refresh_ui();
	void _account_pressed();
	void _details_action(const String &p_action);
	void _refresh_details_ui();
	void _show_details();
	void _update_account_visual();
	void _update_score_icon();
	bool _is_logged_in() const;
	void _state_changed(int p_state);
	void _profile_changed();
	void _request_failed(const String &p_message);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	static String get_display_name(const AuthUserInfo &p_user, const AuthSessionData &p_session);
	static String get_user_id(const AuthUserInfo &p_user, const AuthSessionData &p_session);
	static String get_avatar_initial(const String &p_display_name);
	static String format_user_id(const String &p_user_id);
	static String format_score(const String &p_score);
	static String format_score_value(const String &p_score);

	static String get_display_name_for_test(const AuthUserInfo &p_user, const AuthSessionData &p_session);
	static String get_user_id_for_test(const AuthUserInfo &p_user, const AuthSessionData &p_session);
	static String get_avatar_initial_for_test(const String &p_display_name);
	static String format_user_id_for_test(const String &p_user_id);
	static String format_score_for_test(const String &p_score);
	static String format_score_value_for_test(const String &p_score);

	void set_manager_for_test(const Ref<EditorUserManager> &p_manager);
	void build_for_test();

	EditorUserAvatar();
};
