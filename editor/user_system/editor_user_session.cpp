/**************************************************************************/
/*  editor_user_session.cpp                                               */
/**************************************************************************/

#include "editor_user_session.h"

#include "core/error/error_macros.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "editor/settings/editor_settings.h"

namespace {

static const char *SESSION_PATH = "user_system/session";

} // namespace

Dictionary EditorUserSession::_session_to_dictionary(const AuthSessionData &p_session) {
	Dictionary storage;
	storage["userId"] = p_session.user_id;
	storage["token"] = p_session.token;
	storage["refreshToken"] = p_session.refresh_token;
	storage["deviceId"] = p_session.device_id;
	storage["phone"] = p_session.phone;
	return storage;
}

AuthSessionData EditorUserSession::_session_from_dictionary(const Dictionary &p_storage) {
	AuthSessionData session;
	session.user_id = String(p_storage.get("userId", String()));
	session.token = String(p_storage.get("token", String()));
	session.refresh_token = String(p_storage.get("refreshToken", String()));
	session.device_id = String(p_storage.get("deviceId", String()));
	session.phone = String(p_storage.get("phone", String()));
	return session;
}

String EditorUserSession::_make_device_id() {
	return "editor:" + OS::get_singleton()->get_unique_id() + ":" + itos(Time::get_singleton()->get_unix_time_from_system()) + ":" + itos(Math::rand());
}

AuthSessionData EditorUserSession::load_session() {
	EditorSettings *settings = EditorSettings::get_singleton();
	if (!settings || !settings->has_setting(SESSION_PATH)) {
		return AuthSessionData();
	}

	Variant value = settings->get(SESSION_PATH);
	if (value.get_type() != Variant::DICTIONARY) {
		return AuthSessionData();
	}
	return _session_from_dictionary(value);
}

void EditorUserSession::save_session(const AuthSessionData &p_session) {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);
	settings->set_setting(SESSION_PATH, _session_to_dictionary(p_session));
	EditorSettings::save();
}

void EditorUserSession::clear_session(bool p_keep_device_id) {
	AuthSessionData session;
	if (p_keep_device_id) {
		session.device_id = get_or_create_device_id();
	}
	save_session(session);
}

String EditorUserSession::get_or_create_device_id() {
	AuthSessionData session = load_session();
	if (!session.device_id.is_empty()) {
		return session.device_id;
	}
	session.device_id = _make_device_id();
	save_session(session);
	return session.device_id;
}

Dictionary EditorUserSession::get_session_storage_for_test() {
	EditorSettings *settings = EditorSettings::get_singleton();
	if (!settings || !settings->has_setting(SESSION_PATH)) {
		return Dictionary();
	}
	Variant value = settings->get(SESSION_PATH);
	return value.get_type() == Variant::DICTIONARY ? Dictionary(value) : Dictionary();
}

void EditorUserSession::set_session_storage_for_test(const Dictionary &p_storage) {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);
	settings->set_setting(SESSION_PATH, p_storage);
	EditorSettings::save();
}
