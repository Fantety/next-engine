/**************************************************************************/
/*  auth_client.h                                                         */
/**************************************************************************/

#pragma once

#include "core/io/http_client.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"

struct AuthSessionData {
	String user_id;
	String token;
	String refresh_token;
	String device_id;
	String phone;
};

struct AuthUserInfo {
	String user_id;
	String nickname;
	String phone;
	String email;
	String score;
};

struct AuthResult {
	bool success = false;
	String error;
	AuthSessionData session;
	AuthUserInfo user;
};

class AuthTransport : public RefCounted {
	GDCLASS(AuthTransport, RefCounted);

protected:
	static void _bind_methods();

public:
	virtual bool request_json(HTTPClient::Method p_method, const String &p_path, const String &p_body, const Vector<String> &p_headers, String &r_response, int &r_http_code, String &r_error);
};

class AuthHTTPTransport : public AuthTransport {
	GDCLASS(AuthHTTPTransport, AuthTransport);

protected:
	static void _bind_methods();

public:
	virtual bool request_json(HTTPClient::Method p_method, const String &p_path, const String &p_body, const Vector<String> &p_headers, String &r_response, int &r_http_code, String &r_error) override;
};

class AuthClient : public RefCounted {
	GDCLASS(AuthClient, RefCounted);

	Ref<AuthTransport> transport;

	static Dictionary _build_send_phone_code_body(const String &p_phone);
	static Dictionary _build_phone_code_login_body(const String &p_phone, const String &p_code, const String &p_device_id);
	static Dictionary _build_password_login_body(const String &p_phone, const String &p_password, const String &p_device_id);
	static Dictionary _build_refresh_token_body(const AuthSessionData &p_session);
	static Dictionary _build_logout_body(const AuthSessionData &p_session, bool p_all);
	static AuthResult _parse_auth_response(const String &p_json);
	static AuthResult _parse_user_response(const String &p_json);
	static AuthResult _parse_simple_response(const String &p_json);
	AuthResult _send_request(HTTPClient::Method p_method, const String &p_path, const Dictionary &p_body, bool p_auth_response, const String &p_token = String());

protected:
	static void _bind_methods();

public:
	static String get_default_scene();
	static String get_default_service();
	static String get_base_url_for_test();
	static String get_default_scene_for_test();
	static String get_default_service_for_test();
	static Dictionary build_phone_code_login_body_for_test(const String &p_phone, const String &p_code, const String &p_device_id);
	static bool parse_auth_response_for_test(const String &p_json, AuthResult &r_result);
	static bool parse_user_response_for_test(const String &p_json, AuthResult &r_result);

	void set_transport(const Ref<AuthTransport> &p_transport);
	Ref<AuthTransport> get_transport() const;

	AuthResult send_phone_code(const String &p_phone);
	AuthResult login_with_phone_code(const String &p_phone, const String &p_code, const String &p_device_id);
	AuthResult login_with_password(const String &p_phone, const String &p_password, const String &p_device_id);
	AuthResult refresh_token(const AuthSessionData &p_session);
	AuthResult logout(const AuthSessionData &p_session, bool p_all = false);
	AuthResult get_user(const String &p_user_id, const String &p_token = String());

	AuthClient();
};
