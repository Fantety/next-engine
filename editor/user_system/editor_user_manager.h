/**************************************************************************/
/*  editor_user_manager.h                                                 */
/**************************************************************************/

#pragma once

#include "editor/user_system/auth_client.h"

#include "core/object/ref_counted.h"

class EditorUserManager : public RefCounted {
	GDCLASS(EditorUserManager, RefCounted);

public:
	enum State {
		STATE_LOGGED_OUT,
		STATE_REFRESHING,
		STATE_LOGGING_IN,
		STATE_LOGGED_IN,
		STATE_PROFILE_UNAVAILABLE,
	};

private:
	Ref<AuthClient> auth_client;
	AuthSessionData session;
	AuthUserInfo user_info;
	State state = STATE_LOGGED_OUT;
	String last_error;

	void _set_state(State p_state);
	void _set_error(const String &p_error);
	void _clear_error();
	void _apply_auth_success(const AuthResult &p_result, const String &p_phone);

protected:
	static void _bind_methods();

public:
	void initialize();
	AuthResult send_phone_code(const String &p_phone);
	AuthResult login_with_phone_code(const String &p_phone, const String &p_code);
	AuthResult login_with_password(const String &p_phone, const String &p_password);
	AuthResult refresh_profile();
	AuthResult logout(bool p_all = false);

	State get_state() const;
	AuthSessionData get_session() const;
	AuthUserInfo get_user_info() const;
	String get_last_error() const;
	String get_display_name() const;
	String get_score_text() const;

	void set_auth_client_for_test(const Ref<AuthClient> &p_client);
	AuthSessionData get_session_for_test() const;
	void set_session_for_test(const AuthSessionData &p_session);
	void set_user_info_for_test(const AuthUserInfo &p_user_info);

	EditorUserManager();
};

VARIANT_ENUM_CAST(EditorUserManager::State);
