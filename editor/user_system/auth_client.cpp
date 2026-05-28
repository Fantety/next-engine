/**************************************************************************/
/*  auth_client.cpp                                                       */
/**************************************************************************/

#include "auth_client.h"

#include "core/error/error_macros.h"
#include "core/io/json.h"
#include "core/os/os.h"

namespace {

static const char *NEXUS_SERVER_BASE_URL = "https://skillhub.xin/";
static const char *AUTH_SCENE = "client";
static const char *AUTH_SERVICE = "user";
static const uint64_t HTTP_TIMEOUT_MSEC = 30000;

String _message_or_default(const Dictionary &p_root, const String &p_default) {
	const String message = String(p_root.get("message", String())).strip_edges();
	return message.is_empty() ? p_default : message;
}

bool _is_success_code(const Dictionary &p_root) {
	if (!p_root.has("code")) {
		return true;
	}

	const Variant code_value = p_root.get("code", 0);
	if (code_value.get_type() == Variant::STRING || code_value.get_type() == Variant::STRING_NAME) {
		const String code = String(code_value).strip_edges().to_lower();
		return code == "0" || code == "1" || code == "200" || code == "ok" || code == "success";
	}

	const int code = (int)code_value;
	return code == 0 || code == 1 || code == 200;
}

Dictionary _parse_root_dictionary(const String &p_json, String &r_error) {
	Ref<JSON> parser;
	parser.instantiate();
	const Error err = parser->parse(p_json);
	if (err != OK || parser->get_data().get_type() != Variant::DICTIONARY) {
		r_error = "Authentication server returned invalid JSON.";
		return Dictionary();
	}
	return parser->get_data();
}

String _join_base_path(const String &p_base_path, const String &p_path) {
	String base_path = p_base_path.strip_edges();
	if (base_path.is_empty() || base_path == "/") {
		base_path = "";
	}
	if (!base_path.begins_with("/") && !base_path.is_empty()) {
		base_path = "/" + base_path;
	}
	while (base_path.ends_with("/")) {
		base_path = base_path.substr(0, base_path.length() - 1);
	}

	String path = p_path.strip_edges();
	if (!path.begins_with("/")) {
		path = "/" + path;
	}
	return base_path + path;
}

void _append_security_headers(Vector<String> &r_headers, const String &p_token) {
	if (p_token.is_empty()) {
		return;
	}

	r_headers.push_back("sec-token: " + p_token);
	r_headers.push_back("sec-sign: 1");
}

} // namespace

void AuthTransport::_bind_methods() {
}

bool AuthTransport::request_json(HTTPClient::Method p_method, const String &p_path, const String &p_body, const Vector<String> &p_headers, String &r_response, int &r_http_code, String &r_error) {
	(void)p_method;
	(void)p_path;
	(void)p_body;
	(void)p_headers;
	r_response.clear();
	r_http_code = 0;
	r_error = "Authentication transport is not configured.";
	return false;
}

void AuthHTTPTransport::_bind_methods() {
}

bool AuthHTTPTransport::request_json(HTTPClient::Method p_method, const String &p_path, const String &p_body, const Vector<String> &p_headers, String &r_response, int &r_http_code, String &r_error) {
	String scheme;
	String host;
	String base_path;
	String fragment;
	int port = 0;
	Error err = String(NEXUS_SERVER_BASE_URL).parse_url(scheme, host, port, base_path, fragment);
	if (err != OK || (scheme != "https://" && scheme != "http://") || host.is_empty()) {
		r_error = "Authentication server URL is invalid.";
		return false;
	}

	const bool use_tls = scheme == "https://";
	if (port == 0) {
		port = use_tls ? 443 : 80;
	}

	Ref<HTTPClient> client = HTTPClient::create();
	ERR_FAIL_COND_V(client.is_null(), false);
	client->set_blocking_mode(true);

	err = client->connect_to_host(host, port, use_tls ? Ref<TLSOptions>(TLSOptions::client()) : Ref<TLSOptions>());
	if (err != OK) {
		r_error = vformat("Failed to connect to authentication server: %s:%d", host, port);
		return false;
	}

	const uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	while (client->get_status() == HTTPClient::STATUS_CONNECTING || client->get_status() == HTTPClient::STATUS_RESOLVING) {
		client->poll();
		if (OS::get_singleton()->get_ticks_msec() - start_time > HTTP_TIMEOUT_MSEC) {
			r_error = "Authentication server connection timed out.";
			return false;
		}
		OS::get_singleton()->delay_usec(1000);
	}

	if (client->get_status() != HTTPClient::STATUS_CONNECTED) {
		r_error = vformat("Authentication server connection failed: status=%d", (int)client->get_status());
		return false;
	}

	Vector<String> headers = p_headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: application/json");

	PackedByteArray body = p_body.to_utf8_buffer();
	const String request_path = _join_base_path(base_path, p_path);
	err = client->request(p_method, request_path, headers, body.size() > 0 ? body.ptr() : nullptr, body.size());
	if (err != OK) {
		r_error = "Failed to send authentication request.";
		return false;
	}

	PackedByteArray response_body;
	bool received_response = false;
	while (true) {
		client->poll();
		HTTPClient::Status status = client->get_status();
		if (status == HTTPClient::STATUS_BODY) {
			received_response = true;
			PackedByteArray chunk = client->read_response_body_chunk();
			if (!chunk.is_empty()) {
				response_body.append_array(chunk);
			}
		} else if (status == HTTPClient::STATUS_CONNECTED && client->has_response() && !received_response) {
			received_response = true;
			break;
		} else if (status == HTTPClient::STATUS_CONNECTED && received_response) {
			break;
		} else if (status == HTTPClient::STATUS_DISCONNECTED) {
			break;
		} else if (status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
			r_error = "Authentication server TLS handshake failed.";
			return false;
		} else if (status >= HTTPClient::STATUS_CONNECTION_ERROR) {
			r_error = vformat("Authentication server connection error: status=%d", (int)status);
			return false;
		}

		if (OS::get_singleton()->get_ticks_msec() - start_time > HTTP_TIMEOUT_MSEC) {
			r_error = "Authentication request timed out.";
			return false;
		}
		OS::get_singleton()->delay_usec(1000);
	}

	r_http_code = client->get_response_code();
	r_response = response_body.is_empty() ? String() : String::utf8(reinterpret_cast<const char *>(response_body.ptr()), response_body.size());
	if (r_http_code < 200 || r_http_code >= 300) {
		r_error = vformat("Authentication server returned HTTP %d.", r_http_code);
		return false;
	}
	return true;
}

void AuthClient::_bind_methods() {
}

Dictionary AuthClient::_build_send_phone_code_body(const String &p_phone) {
	Dictionary body;
	body["phone"] = p_phone.strip_edges();
	return body;
}

Dictionary AuthClient::_build_phone_code_login_body(const String &p_phone, const String &p_code, const String &p_device_id) {
	Dictionary body;
	body["phone"] = p_phone.strip_edges();
	body["code"] = p_code.strip_edges();
	body["scene"] = get_default_scene();
	body["deviceId"] = p_device_id;
	return body;
}

Dictionary AuthClient::_build_password_login_body(const String &p_phone, const String &p_password, const String &p_device_id) {
	Dictionary body;
	body["phone"] = p_phone.strip_edges();
	body["password"] = p_password;
	body["twoFactorAuth"] = "";
	body["scene"] = get_default_scene();
	body["deviceId"] = p_device_id;
	return body;
}

Dictionary AuthClient::_build_refresh_token_body(const AuthSessionData &p_session) {
	Dictionary body;
	body["refreshToken"] = p_session.refresh_token;
	body["token"] = p_session.token;
	body["scene"] = get_default_scene();
	body["service"] = get_default_service();
	body["deviceId"] = p_session.device_id;
	return body;
}

Dictionary AuthClient::_build_logout_body(const AuthSessionData &p_session, bool p_all) {
	Dictionary body;
	body["scene"] = get_default_scene();
	body["service"] = get_default_service();
	body["all"] = p_all;
	body["deviceId"] = p_session.device_id;
	return body;
}

AuthResult AuthClient::_parse_auth_response(const String &p_json) {
	AuthResult result;
	String parse_error;
	Dictionary root = _parse_root_dictionary(p_json, parse_error);
	if (!parse_error.is_empty()) {
		result.error = parse_error;
		return result;
	}

	if (!_is_success_code(root)) {
		result.error = _message_or_default(root, "Authentication request failed.");
		return result;
	}

	Variant data_value = root.get("data", Dictionary());
	if (data_value.get_type() != Variant::DICTIONARY) {
		result.error = "Authentication server returned no user data.";
		return result;
	}

	Dictionary data = data_value;
	result.session.user_id = String(data.get("userId", data.get("id", String()))).strip_edges();
	result.session.token = String(data.get("token", String()));
	result.session.refresh_token = String(data.get("refreshToken", String()));
	result.success = !result.session.user_id.is_empty() && !result.session.token.is_empty() && !result.session.refresh_token.is_empty();
	if (!result.success) {
		result.error = "Authentication server response is missing credentials.";
	}
	return result;
}

AuthResult AuthClient::_parse_user_response(const String &p_json) {
	AuthResult result;
	String parse_error;
	Dictionary root = _parse_root_dictionary(p_json, parse_error);
	if (!parse_error.is_empty()) {
		result.error = parse_error;
		return result;
	}

	if (!_is_success_code(root)) {
		result.error = _message_or_default(root, "Failed to load account information.");
		return result;
	}

	Variant data_value = root.get("data", Dictionary());
	if (data_value.get_type() != Variant::DICTIONARY) {
		result.error = "Account information response is empty.";
		return result;
	}

	Dictionary data = data_value;
	result.user.user_id = String(data.get("id", data.get("userId", String()))).strip_edges();
	result.user.nickname = String(data.get("nickname", data.get("nickName", String()))).strip_edges();
	result.user.phone = String(data.get("phone", String())).strip_edges();
	result.user.email = String(data.get("email", String())).strip_edges();
	result.user.score = String(data.get("score", String())).strip_edges();
	result.success = !result.user.user_id.is_empty();
	if (!result.success) {
		result.error = "Account information response is missing user id.";
	}
	return result;
}

AuthResult AuthClient::_parse_simple_response(const String &p_json) {
	AuthResult result;
	if (p_json.strip_edges().is_empty()) {
		result.success = true;
		return result;
	}

	String parse_error;
	Dictionary root = _parse_root_dictionary(p_json, parse_error);
	if (!parse_error.is_empty()) {
		result.error = parse_error;
		return result;
	}
	if (!_is_success_code(root)) {
		result.error = _message_or_default(root, "Authentication request failed.");
		return result;
	}
	result.success = true;
	return result;
}

AuthResult AuthClient::_send_request(HTTPClient::Method p_method, const String &p_path, const Dictionary &p_body, bool p_auth_response, const String &p_token) {
	AuthResult result;
	if (transport.is_null()) {
		Ref<AuthHTTPTransport> http_transport;
		http_transport.instantiate();
		transport = http_transport;
	}

	Vector<String> headers;
	_append_security_headers(headers, p_token);

	String response;
	String error;
	int http_code = 0;
	const String body = JSON::stringify(p_body);
	if (!transport->request_json(p_method, p_path, body, headers, response, http_code, error)) {
		result.error = error.is_empty() ? "Authentication request failed." : error;
		return result;
	}

	return p_auth_response ? _parse_auth_response(response) : _parse_simple_response(response);
}

String AuthClient::get_default_scene() {
	return AUTH_SCENE;
}

String AuthClient::get_default_service() {
	return AUTH_SERVICE;
}

String AuthClient::get_base_url_for_test() {
	return NEXUS_SERVER_BASE_URL;
}

String AuthClient::get_default_scene_for_test() {
	return get_default_scene();
}

String AuthClient::get_default_service_for_test() {
	return get_default_service();
}

Dictionary AuthClient::build_phone_code_login_body_for_test(const String &p_phone, const String &p_code, const String &p_device_id) {
	return _build_phone_code_login_body(p_phone, p_code, p_device_id);
}

bool AuthClient::parse_auth_response_for_test(const String &p_json, AuthResult &r_result) {
	r_result = _parse_auth_response(p_json);
	return r_result.success;
}

bool AuthClient::parse_user_response_for_test(const String &p_json, AuthResult &r_result) {
	r_result = _parse_user_response(p_json);
	return r_result.success;
}

void AuthClient::set_transport(const Ref<AuthTransport> &p_transport) {
	transport = p_transport;
}

Ref<AuthTransport> AuthClient::get_transport() const {
	return transport;
}

AuthResult AuthClient::send_phone_code(const String &p_phone) {
	return _send_request(HTTPClient::METHOD_POST, "/v1/auth/send/phone/code", _build_send_phone_code_body(p_phone), false);
}

AuthResult AuthClient::login_with_phone_code(const String &p_phone, const String &p_code, const String &p_device_id) {
	AuthResult result = _send_request(HTTPClient::METHOD_POST, "/v1/auth/validate/phone/code", _build_phone_code_login_body(p_phone, p_code, p_device_id), true);
	if (result.success) {
		result.session.phone = p_phone.strip_edges();
		result.session.device_id = p_device_id;
	}
	return result;
}

AuthResult AuthClient::login_with_password(const String &p_phone, const String &p_password, const String &p_device_id) {
	AuthResult result = _send_request(HTTPClient::METHOD_POST, "/v1/auth/validate/password", _build_password_login_body(p_phone, p_password, p_device_id), true);
	if (result.success) {
		result.session.phone = p_phone.strip_edges();
		result.session.device_id = p_device_id;
	}
	return result;
}

AuthResult AuthClient::refresh_token(const AuthSessionData &p_session) {
	AuthResult result = _send_request(HTTPClient::METHOD_POST, "/v1/auth/token/refresh", _build_refresh_token_body(p_session), true, p_session.token);
	if (result.success) {
		result.session.phone = p_session.phone;
		result.session.device_id = p_session.device_id;
	}
	return result;
}

AuthResult AuthClient::logout(const AuthSessionData &p_session, bool p_all) {
	return _send_request(HTTPClient::METHOD_POST, "/v1/auth/logout", _build_logout_body(p_session, p_all), false, p_session.token);
}

AuthResult AuthClient::get_user(const String &p_user_id, const String &p_token) {
	AuthResult result;
	if (p_user_id.strip_edges().is_empty()) {
		result.error = "User id is empty.";
		return result;
	}
	if (transport.is_null()) {
		Ref<AuthHTTPTransport> http_transport;
		http_transport.instantiate();
		transport = http_transport;
	}

	Vector<String> headers;
	_append_security_headers(headers, p_token);

	String response;
	String error;
	int http_code = 0;
	const String path = "/user/" + p_user_id.uri_encode();
	if (!transport->request_json(HTTPClient::METHOD_GET, path, String(), headers, response, http_code, error)) {
		result.error = error.is_empty() ? "Failed to load account information." : error;
		return result;
	}
	return _parse_user_response(response);
}

AuthClient::AuthClient() {
	Ref<AuthHTTPTransport> http_transport;
	http_transport.instantiate();
	transport = http_transport;
}
