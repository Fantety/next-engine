/**************************************************************************/
/*  editor_user_manager.cpp                                               */
/**************************************************************************/

#include "editor_user_manager.h"

#include "core/error/error_macros.h"
#include "core/object/class_db.h"
#include "editor/user_system/editor_user_session.h"

void EditorUserManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_state"), &EditorUserManager::get_state);
	ClassDB::bind_method(D_METHOD("get_display_name"), &EditorUserManager::get_display_name);
	ClassDB::bind_method(D_METHOD("get_score_text"), &EditorUserManager::get_score_text);

	ADD_SIGNAL(MethodInfo("state_changed", PropertyInfo(Variant::INT, "state")));
	ADD_SIGNAL(MethodInfo("profile_changed"));
	ADD_SIGNAL(MethodInfo("request_failed", PropertyInfo(Variant::STRING, "message")));

	BIND_ENUM_CONSTANT(STATE_LOGGED_OUT);
	BIND_ENUM_CONSTANT(STATE_REFRESHING);
	BIND_ENUM_CONSTANT(STATE_LOGGING_IN);
	BIND_ENUM_CONSTANT(STATE_LOGGED_IN);
	BIND_ENUM_CONSTANT(STATE_PROFILE_UNAVAILABLE);
}

void EditorUserManager::_set_state(State p_state) {
	if (state == p_state) {
		return;
	}
	state = p_state;
	emit_signal(SNAME("state_changed"), (int)state);
}

void EditorUserManager::_set_error(const String &p_error) {
	last_error = p_error;
	if (!p_error.is_empty()) {
		emit_signal(SNAME("request_failed"), p_error);
	}
}

void EditorUserManager::_clear_error() {
	last_error.clear();
}

void EditorUserManager::_apply_auth_success(const AuthResult &p_result, const String &p_phone) {
	session = p_result.session;
	if (!p_phone.strip_edges().is_empty()) {
		session.phone = p_phone.strip_edges();
	}
	if (session.device_id.is_empty()) {
		session.device_id = EditorUserSession::get_or_create_device_id();
	}
	EditorUserSession::save_session(session);
	_clear_error();
	(void)refresh_profile();
	if (state != STATE_PROFILE_UNAVAILABLE) {
		_set_state(STATE_LOGGED_IN);
	}
}

void EditorUserManager::initialize() {
	if (auth_client.is_null()) {
		auth_client.instantiate();
	}

	session = EditorUserSession::load_session();
	if (session.device_id.is_empty()) {
		session.device_id = EditorUserSession::get_or_create_device_id();
	}

	if (session.refresh_token.is_empty() || session.token.is_empty()) {
		_set_state(STATE_LOGGED_OUT);
		return;
	}

	_set_state(STATE_REFRESHING);
	AuthResult refreshed = auth_client->refresh_token(session);
	if (!refreshed.success) {
		EditorUserSession::clear_session();
		session = EditorUserSession::load_session();
		user_info = AuthUserInfo();
		_set_error(refreshed.error);
		_set_state(STATE_LOGGED_OUT);
		emit_signal(SNAME("profile_changed"));
		return;
	}

	session = refreshed.session;
	if (session.device_id.is_empty()) {
		session.device_id = EditorUserSession::get_or_create_device_id();
	}
	EditorUserSession::save_session(session);
	(void)refresh_profile();
	if (state != STATE_PROFILE_UNAVAILABLE) {
		_set_state(STATE_LOGGED_IN);
	}
}

AuthResult EditorUserManager::send_phone_code(const String &p_phone) {
	ERR_FAIL_COND_V(auth_client.is_null(), AuthResult());

	AuthResult result = auth_client->send_phone_code(p_phone);
	if (!result.success) {
		_set_error(result.error);
	} else {
		_clear_error();
	}
	return result;
}

AuthResult EditorUserManager::login_with_phone_code(const String &p_phone, const String &p_code) {
	ERR_FAIL_COND_V(auth_client.is_null(), AuthResult());

	_set_state(STATE_LOGGING_IN);
	AuthResult result = auth_client->login_with_phone_code(p_phone, p_code, EditorUserSession::get_or_create_device_id());
	if (!result.success) {
		_set_error(result.error);
		_set_state(STATE_LOGGED_OUT);
		return result;
	}
	_apply_auth_success(result, p_phone);
	return result;
}

AuthResult EditorUserManager::login_with_password(const String &p_phone, const String &p_password) {
	ERR_FAIL_COND_V(auth_client.is_null(), AuthResult());

	_set_state(STATE_LOGGING_IN);
	AuthResult result = auth_client->login_with_password(p_phone, p_password, EditorUserSession::get_or_create_device_id());
	if (!result.success) {
		_set_error(result.error);
		_set_state(STATE_LOGGED_OUT);
		return result;
	}
	_apply_auth_success(result, p_phone);
	return result;
}

AuthResult EditorUserManager::refresh_profile() {
	ERR_FAIL_COND_V(auth_client.is_null(), AuthResult());

	AuthResult result = auth_client->get_user(session.user_id, session.token);
	if (!result.success) {
		user_info = AuthUserInfo();
		_set_error(result.error);
		if (!session.user_id.is_empty()) {
			_set_state(STATE_PROFILE_UNAVAILABLE);
		}
		emit_signal(SNAME("profile_changed"));
		return result;
	}

	user_info = result.user;
	if (user_info.phone.is_empty()) {
		user_info.phone = session.phone;
	}
	_clear_error();
	_set_state(STATE_LOGGED_IN);
	emit_signal(SNAME("profile_changed"));
	return result;
}

AuthResult EditorUserManager::logout(bool p_all) {
	AuthResult result;
	if (!session.token.is_empty() && auth_client.is_valid()) {
		result = auth_client->logout(session, p_all);
	}
	EditorUserSession::clear_session();
	session = EditorUserSession::load_session();
	user_info = AuthUserInfo();
	_clear_error();
	_set_state(STATE_LOGGED_OUT);
	emit_signal(SNAME("profile_changed"));
	result.success = true;
	return result;
}

EditorUserManager::State EditorUserManager::get_state() const {
	return state;
}

AuthSessionData EditorUserManager::get_session() const {
	return session;
}

AuthUserInfo EditorUserManager::get_user_info() const {
	return user_info;
}

String EditorUserManager::get_last_error() const {
	return last_error;
}

String EditorUserManager::get_display_name() const {
	if (!user_info.nickname.is_empty()) {
		return user_info.nickname;
	}
	if (!user_info.phone.is_empty()) {
		return user_info.phone;
	}
	if (!session.phone.is_empty()) {
		return session.phone;
	}
	return session.user_id;
}

String EditorUserManager::get_score_text() const {
	return user_info.score.is_empty() ? "--" : user_info.score;
}

void EditorUserManager::set_auth_client_for_test(const Ref<AuthClient> &p_client) {
	auth_client = p_client;
}

AuthSessionData EditorUserManager::get_session_for_test() const {
	return session;
}

void EditorUserManager::set_session_for_test(const AuthSessionData &p_session) {
	session = p_session;
}

void EditorUserManager::set_user_info_for_test(const AuthUserInfo &p_user_info) {
	user_info = p_user_info;
}

EditorUserManager::EditorUserManager() {
	auth_client.instantiate();
}
