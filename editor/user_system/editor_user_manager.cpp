/**************************************************************************/
/*  editor_user_manager.cpp                                               */
/**************************************************************************/

#include "editor_user_manager.h"

#include "core/error/error_macros.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "editor/user_system/editor_user_session.h"

void EditorUserManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_state"), &EditorUserManager::get_state);
	ClassDB::bind_method(D_METHOD("is_request_pending"), &EditorUserManager::is_request_pending);
	ClassDB::bind_method(D_METHOD("get_display_name"), &EditorUserManager::get_display_name);
	ClassDB::bind_method(D_METHOD("get_credits_text"), &EditorUserManager::get_credits_text);

	ADD_SIGNAL(MethodInfo("state_changed", PropertyInfo(Variant::INT, "state")));
	ADD_SIGNAL(MethodInfo("profile_changed"));
	ADD_SIGNAL(MethodInfo("request_failed", PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("request_completed", PropertyInfo(Variant::INT, "request"), PropertyInfo(Variant::BOOL, "success"), PropertyInfo(Variant::STRING, "message")));

	BIND_ENUM_CONSTANT(STATE_LOGGED_OUT);
	BIND_ENUM_CONSTANT(STATE_REFRESHING);
	BIND_ENUM_CONSTANT(STATE_LOGGING_IN);
	BIND_ENUM_CONSTANT(STATE_LOGGED_IN);
	BIND_ENUM_CONSTANT(STATE_PROFILE_UNAVAILABLE);

	BIND_ENUM_CONSTANT(REQUEST_NONE);
	BIND_ENUM_CONSTANT(REQUEST_SEND_PHONE_CODE);
	BIND_ENUM_CONSTANT(REQUEST_LOGIN_PHONE_CODE);
	BIND_ENUM_CONSTANT(REQUEST_LOGIN_PASSWORD);
	BIND_ENUM_CONSTANT(REQUEST_REFRESH_TOKEN);
	BIND_ENUM_CONSTANT(REQUEST_REFRESH_PROFILE);
	BIND_ENUM_CONSTANT(REQUEST_LOGOUT);
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

void EditorUserManager::_apply_auth_success(const AuthResult &p_result, const String &p_phone, bool p_refresh_profile) {
	session = p_result.session;
	if (!p_phone.strip_edges().is_empty()) {
		session.phone = p_phone.strip_edges();
	}
	if (session.device_id.is_empty()) {
		session.device_id = EditorUserSession::get_or_create_device_id();
	}
	EditorUserSession::save_session(session);
	_clear_error();
	if (p_refresh_profile) {
		(void)refresh_profile();
		if (state != STATE_PROFILE_UNAVAILABLE) {
			_set_state(STATE_LOGGED_IN);
		}
	} else {
		_set_state(STATE_LOGGED_IN);
		emit_signal(SNAME("profile_changed"));
	}
}

bool EditorUserManager::_start_request(RequestType p_request, const AuthSessionData &p_session, const String &p_phone, const String &p_code, const String &p_password, bool p_logout_all, bool p_profile_refresh_can_refresh_token) {
	if (request_running.is_set()) {
		_set_error("An account request is already running.");
		return false;
	}

	if (request_thread.is_started()) {
		request_thread.wait_to_finish();
	}

	if (auth_client.is_null()) {
		auth_client.instantiate();
	}

	ThreadParams *params = memnew(ThreadParams);
	params->manager = Ref<EditorUserManager>(this);
	params->transport = auth_client->get_transport();
	params->request = p_request;
	params->session = p_session;
	params->phone = p_phone;
	params->code = p_code;
	params->password = p_password;
	params->logout_all = p_logout_all;
	params->profile_refresh_can_refresh_token = p_profile_refresh_can_refresh_token;
	params->session_generation = session_generation;

	request_running.set();
#ifdef THREADS_ENABLED
	if (request_thread.start(_thread_func, params) == Thread::UNASSIGNED_ID) {
		memdelete(params);
		request_running.clear();
		_set_error("Failed to start account request worker.");
		return false;
	}
#else
	_thread_func(params);
#endif
	return true;
}

bool EditorUserManager::_request_refresh_profile(bool p_allow_token_refresh_on_unauthorized) {
	if (session.user_id.is_empty() || session.token.is_empty()) {
		_set_error("Account session is not available.");
		return false;
	}
	print_line(vformat("[User Auth] request_refresh_profile user_id=%s token_present=%s", session.user_id, session.token.is_empty() ? "false" : "true"));
	return _start_request(REQUEST_REFRESH_PROFILE, session, String(), String(), String(), false, p_allow_token_refresh_on_unauthorized);
}

void EditorUserManager::_set_completed_request(RequestType p_request, const AuthResult &p_result, uint64_t p_session_generation, bool p_profile_refresh_can_refresh_token) {
	MutexLock lock(request_result_mutex);
	completed_request = p_request;
	completed_result = p_result;
	completed_session_generation = p_session_generation;
	completed_profile_refresh_can_refresh_token = p_profile_refresh_can_refresh_token;
}

void EditorUserManager::_complete_async_request() {
	RequestType request = REQUEST_NONE;
	AuthResult result;
	uint64_t result_session_generation = 0;
	bool profile_refresh_can_refresh_token = false;
	{
		MutexLock lock(request_result_mutex);
		request = completed_request;
		result = completed_result;
		result_session_generation = completed_session_generation;
		profile_refresh_can_refresh_token = completed_profile_refresh_can_refresh_token;
		completed_request = REQUEST_NONE;
		completed_result = AuthResult();
		completed_session_generation = 0;
		completed_profile_refresh_can_refresh_token = false;
	}

	if (request_thread.is_started()) {
		request_thread.wait_to_finish();
	}
	request_running.clear();

	if (request == REQUEST_NONE) {
		return;
	}

	if (result_session_generation != session_generation) {
		emit_signal(SNAME("request_completed"), (int)request, false, String());
		return;
	}

	bool start_profile_refresh = false;
	bool start_token_refresh = false;
	bool completed_success = result.success;
	String completed_message = result.error;

	switch (request) {
		case REQUEST_SEND_PHONE_CODE: {
			if (result.success) {
				_clear_error();
			} else {
				_set_error(result.error);
			}
		} break;

		case REQUEST_LOGIN_PHONE_CODE:
		case REQUEST_LOGIN_PASSWORD: {
			if (result.success) {
				_apply_auth_success(result, result.session.phone, false);
				start_profile_refresh = true;
			} else {
				_set_error(result.error);
				_set_state(STATE_LOGGED_OUT);
			}
		} break;

		case REQUEST_REFRESH_TOKEN: {
			if (result.success) {
				session = result.session;
				if (session.device_id.is_empty()) {
					session.device_id = EditorUserSession::get_or_create_device_id();
				}
				EditorUserSession::save_session(session);
				_clear_error();
				_set_state(STATE_LOGGED_IN);
				emit_signal(SNAME("profile_changed"));
				start_profile_refresh = true;
			} else if (profile_refresh_can_refresh_token) {
				EditorUserSession::clear_session();
				session = EditorUserSession::load_session();
				user_info = AuthUserInfo();
				_set_error("Account session expired. Please sign in again.");
				_set_state(STATE_LOGGED_OUT);
				emit_signal(SNAME("profile_changed"));
			} else {
				user_info = AuthUserInfo();
				_set_error(result.error);
				_set_state(session.user_id.is_empty() ? STATE_LOGGED_OUT : STATE_PROFILE_UNAVAILABLE);
				emit_signal(SNAME("profile_changed"));
				if (!session.user_id.is_empty() && !session.token.is_empty()) {
					start_profile_refresh = true;
				}
			}
		} break;

		case REQUEST_REFRESH_PROFILE: {
			if (result.success) {
				user_info = result.user;
				if (user_info.phone.is_empty()) {
					user_info.phone = session.phone;
				}
				_clear_error();
				_set_state(STATE_LOGGED_IN);
			} else if (result.http_code == 401 && profile_refresh_can_refresh_token && !session.refresh_token.is_empty()) {
				_set_state(STATE_REFRESHING);
				_clear_error();
				start_token_refresh = true;
			} else if (result.http_code == 401) {
				EditorUserSession::clear_session();
				session = EditorUserSession::load_session();
				user_info = AuthUserInfo();
				_set_error("Account session expired. Please sign in again.");
				_set_state(STATE_LOGGED_OUT);
			} else {
				user_info = AuthUserInfo();
				_set_error(result.error);
				_set_state(session.user_id.is_empty() ? STATE_LOGGED_OUT : STATE_PROFILE_UNAVAILABLE);
			}
			emit_signal(SNAME("profile_changed"));
		} break;

		case REQUEST_LOGOUT: {
			completed_success = true;
			completed_message.clear();
		} break;

		case REQUEST_NONE: {
		} break;
	}

	bool profile_refresh_started = false;
	if (start_profile_refresh) {
		print_line(vformat("[User Auth] scheduling profile refresh user_id=%s token_present=%s", session.user_id, session.token.is_empty() ? "false" : "true"));
		profile_refresh_started = _request_refresh_profile(false);
		if (!profile_refresh_started) {
			print_line(vformat("[User Auth] profile refresh was not started: %s", last_error));
		}
	}

	bool token_refresh_started = false;
	if (start_token_refresh) {
		print_line(vformat("[User Auth] scheduling token refresh after profile 401 user_id=%s refresh_token_present=%s", session.user_id, session.refresh_token.is_empty() ? "false" : "true"));
		token_refresh_started = _start_request(REQUEST_REFRESH_TOKEN, session, String(), String(), String(), false, true);
		if (!token_refresh_started) {
			print_line(vformat("[User Auth] token refresh was not started: %s", last_error));
		}
	}

	print_line(vformat("[User Auth] completed request=%d success=%s message=%s user_id=%s start_profile_refresh=%s profile_refresh_started=%s start_token_refresh=%s token_refresh_started=%s", (int)request, completed_success ? "true" : "false", completed_message, session.user_id, start_profile_refresh ? "true" : "false", profile_refresh_started ? "true" : "false", start_token_refresh ? "true" : "false", token_refresh_started ? "true" : "false"));
	emit_signal(SNAME("request_completed"), (int)request, completed_success, completed_message);
}

void EditorUserManager::_thread_func(void *p_userdata) {
	ThreadParams *params = static_cast<ThreadParams *>(p_userdata);
	Ref<EditorUserManager> manager = params->manager;
	Ref<AuthTransport> transport = params->transport;
	const RequestType request = params->request;
	const AuthSessionData session = params->session;
	const String phone = params->phone;
	const String code = params->code;
	const String password = params->password;
	const bool logout_all = params->logout_all;
	const bool profile_refresh_can_refresh_token = params->profile_refresh_can_refresh_token;
	const uint64_t request_session_generation = params->session_generation;
	memdelete(params);

	Ref<AuthClient> client;
	client.instantiate();
	if (transport.is_valid()) {
		client->set_transport(transport);
	}

	AuthResult result;
	switch (request) {
		case REQUEST_SEND_PHONE_CODE:
			result = client->send_phone_code(phone);
			break;
		case REQUEST_LOGIN_PHONE_CODE:
			result = client->login_with_phone_code(phone, code, session.device_id);
			break;
		case REQUEST_LOGIN_PASSWORD:
			result = client->login_with_password(phone, password, session.device_id);
			break;
		case REQUEST_REFRESH_TOKEN:
			result = client->refresh_token(session);
			break;
		case REQUEST_REFRESH_PROFILE:
			print_line(vformat("[User Auth] worker loading profile user_id=%s token_present=%s", session.user_id, session.token.is_empty() ? "false" : "true"));
			result = client->get_user(session.token);
			break;
		case REQUEST_LOGOUT:
			result = client->logout(session, logout_all);
			result.success = true;
			result.error.clear();
			break;
		case REQUEST_NONE:
			result.error = "Account request is not configured.";
			break;
	}

	if (manager.is_valid()) {
		manager->_set_completed_request(request, result, request_session_generation, profile_refresh_can_refresh_token);
		callable_mp(manager.ptr(), &EditorUserManager::_complete_async_request).call_deferred();
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
	_clear_error();
	if (!_start_request(REQUEST_REFRESH_TOKEN, session)) {
		_set_state(session.user_id.is_empty() ? STATE_LOGGED_OUT : STATE_PROFILE_UNAVAILABLE);
	}
}

bool EditorUserManager::request_send_phone_code(const String &p_phone) {
	const String phone = p_phone.strip_edges();
	if (phone.is_empty()) {
		_set_error("Phone number is empty.");
		return false;
	}

	return _start_request(REQUEST_SEND_PHONE_CODE, AuthSessionData(), phone);
}

bool EditorUserManager::request_login_with_phone_code(const String &p_phone, const String &p_code) {
	AuthSessionData request_session;
	request_session.device_id = EditorUserSession::get_or_create_device_id();
	if (!_start_request(REQUEST_LOGIN_PHONE_CODE, request_session, p_phone, p_code)) {
		return false;
	}
	_clear_error();
	_set_state(STATE_LOGGING_IN);
	return true;
}

bool EditorUserManager::request_login_with_password(const String &p_phone, const String &p_password) {
	AuthSessionData request_session;
	request_session.device_id = EditorUserSession::get_or_create_device_id();
	if (!_start_request(REQUEST_LOGIN_PASSWORD, request_session, p_phone, String(), p_password)) {
		return false;
	}
	_clear_error();
	_set_state(STATE_LOGGING_IN);
	return true;
}

bool EditorUserManager::request_refresh_profile() {
	return _request_refresh_profile(true);
}

bool EditorUserManager::request_logout(bool p_all) {
	AuthSessionData logout_session = session;
	session_generation++;

	EditorUserSession::clear_session();
	session = EditorUserSession::load_session();
	user_info = AuthUserInfo();
	_clear_error();
	_set_state(STATE_LOGGED_OUT);
	emit_signal(SNAME("profile_changed"));

	if (logout_session.token.is_empty() || request_running.is_set()) {
		emit_signal(SNAME("request_completed"), (int)REQUEST_LOGOUT, true, String());
		return true;
	}

	return _start_request(REQUEST_LOGOUT, logout_session, String(), String(), String(), p_all);
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

	AuthResult result = auth_client->get_user(session.token);
	if (!result.success && result.http_code == 401 && !session.refresh_token.is_empty()) {
		AuthResult refresh_result = auth_client->refresh_token(session);
		if (refresh_result.success) {
			session = refresh_result.session;
			if (session.device_id.is_empty()) {
				session.device_id = EditorUserSession::get_or_create_device_id();
			}
			EditorUserSession::save_session(session);
			result = auth_client->get_user(session.token);
		} else {
			result = refresh_result;
		}
	}

	if (!result.success) {
		user_info = AuthUserInfo();
		if (result.http_code == 401) {
			EditorUserSession::clear_session();
			session = EditorUserSession::load_session();
			_set_error("Account session expired. Please sign in again.");
			_set_state(STATE_LOGGED_OUT);
		} else {
			_set_error(result.error);
			if (!session.user_id.is_empty()) {
				_set_state(STATE_PROFILE_UNAVAILABLE);
			}
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
	session_generation++;
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

bool EditorUserManager::is_request_pending() const {
	return request_running.is_set();
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
	return user_info.nickname.strip_edges();
}

String EditorUserManager::get_credits_text() const {
	const String credits = user_info.credits.strip_edges();
	return credits.is_empty() ? "--" : credits;
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

void EditorUserManager::wait_for_request_for_test() {
	if (request_thread.is_started()) {
		request_thread.wait_to_finish();
	}
}

EditorUserManager::EditorUserManager() {
	auth_client.instantiate();
	request_running.clear();
}

EditorUserManager::~EditorUserManager() {
	wait_for_request_for_test();
}
