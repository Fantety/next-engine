/**************************************************************************/
/*  editor_user_manager.h                                                 */
/**************************************************************************/

#pragma once

#include "editor/user_system/auth_client.h"

#include "core/os/mutex.h"
#include "core/os/safe_binary_mutex.h"
#include "core/os/thread.h"
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

	enum RequestType {
		REQUEST_NONE,
		REQUEST_SEND_PHONE_CODE,
		REQUEST_LOGIN_PHONE_CODE,
		REQUEST_LOGIN_PASSWORD,
		REQUEST_REFRESH_TOKEN,
		REQUEST_REFRESH_PROFILE,
		REQUEST_LOGOUT,
	};

private:
	struct ThreadParams {
		Ref<EditorUserManager> manager;
		Ref<AuthTransport> transport;
		RequestType request = REQUEST_NONE;
		AuthSessionData session;
		String phone;
		String code;
		String password;
		bool logout_all = false;
		bool profile_refresh_can_refresh_token = false;
		uint64_t session_generation = 0;
	};

	Ref<AuthClient> auth_client;
	AuthSessionData session;
	AuthUserInfo user_info;
	State state = STATE_LOGGED_OUT;
	String last_error;
	Thread request_thread;
	SafeFlag request_running;
	mutable Mutex request_result_mutex;
	RequestType completed_request = REQUEST_NONE;
	AuthResult completed_result;
	uint64_t completed_session_generation = 0;
	bool completed_profile_refresh_can_refresh_token = false;
	uint64_t session_generation = 0;

	void _set_state(State p_state);
	void _set_error(const String &p_error);
	void _clear_error();
	void _apply_auth_success(const AuthResult &p_result, const String &p_phone, bool p_refresh_profile = true);
	bool _start_request(RequestType p_request, const AuthSessionData &p_session = AuthSessionData(), const String &p_phone = String(), const String &p_code = String(), const String &p_password = String(), bool p_logout_all = false, bool p_profile_refresh_can_refresh_token = false);
	bool _request_refresh_profile(bool p_allow_token_refresh_on_unauthorized);
	void _set_completed_request(RequestType p_request, const AuthResult &p_result, uint64_t p_session_generation, bool p_profile_refresh_can_refresh_token);
	void _complete_async_request();
	static void _thread_func(void *p_userdata);

protected:
	static void _bind_methods();

public:
	void initialize();
	bool request_send_phone_code(const String &p_phone);
	bool request_login_with_phone_code(const String &p_phone, const String &p_code);
	bool request_login_with_password(const String &p_phone, const String &p_password);
	bool request_refresh_profile();
	bool request_logout(bool p_all = false);
	AuthResult send_phone_code(const String &p_phone);
	AuthResult login_with_phone_code(const String &p_phone, const String &p_code);
	AuthResult login_with_password(const String &p_phone, const String &p_password);
	AuthResult refresh_profile();
	AuthResult logout(bool p_all = false);

	State get_state() const;
	bool is_request_pending() const;
	AuthSessionData get_session() const;
	AuthUserInfo get_user_info() const;
	String get_last_error() const;
	String get_display_name() const;
	String get_credits_text() const;

	void set_auth_client_for_test(const Ref<AuthClient> &p_client);
	AuthSessionData get_session_for_test() const;
	void set_session_for_test(const AuthSessionData &p_session);
	void set_user_info_for_test(const AuthUserInfo &p_user_info);
	void wait_for_request_for_test();

	EditorUserManager();
	~EditorUserManager();
};

VARIANT_ENUM_CAST(EditorUserManager::State);
VARIANT_ENUM_CAST(EditorUserManager::RequestType);
