/**************************************************************************/
/*  test_user_system.cpp                                                  */
/**************************************************************************/

#include "tests/test_macros.h"

#include "core/io/json.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "editor/user_system/auth_client.h"
#include "editor/user_system/editor_user_avatar.h"
#include "editor/user_system/editor_user_login_dialog.h"
#include "editor/user_system/editor_user_manager.h"
#include "editor/user_system/editor_user_session.h"
#include "editor/settings/editor_settings.h"

TEST_FORCE_LINK(test_user_system);

namespace TestEditorUserSystem {

static const char *AUTH_BASE_URL_SETTING_FOR_TEST = "user_system/authentication/base_url";
static const char *AUTH_BASE_URL_ENV_FOR_TEST = "NEXT_ENGINE_AUTH_BASE_URL";

class ScopedAuthBaseUrlReset {
	bool had_env = false;
	String old_env;
	bool had_setting = false;
	Variant old_setting;

public:
	ScopedAuthBaseUrlReset() {
		OS *os = OS::get_singleton();
		if (os) {
			had_env = os->has_environment(AUTH_BASE_URL_ENV_FOR_TEST);
			if (had_env) {
				old_env = os->get_environment(AUTH_BASE_URL_ENV_FOR_TEST);
			}
			os->unset_environment(AUTH_BASE_URL_ENV_FOR_TEST);
		}

		EditorSettings *settings = EditorSettings::get_singleton();
		if (settings) {
			had_setting = settings->has_setting(AUTH_BASE_URL_SETTING_FOR_TEST);
			if (had_setting) {
				old_setting = settings->get_setting(AUTH_BASE_URL_SETTING_FOR_TEST);
			}
			settings->set_setting(AUTH_BASE_URL_SETTING_FOR_TEST, String());
		}
	}

	~ScopedAuthBaseUrlReset() {
		EditorSettings *settings = EditorSettings::get_singleton();
		if (settings) {
			if (had_setting) {
				settings->set_setting(AUTH_BASE_URL_SETTING_FOR_TEST, old_setting);
			} else {
				settings->erase(AUTH_BASE_URL_SETTING_FOR_TEST);
			}
		}

		OS *os = OS::get_singleton();
		if (os) {
			if (had_env) {
				os->set_environment(AUTH_BASE_URL_ENV_FOR_TEST, old_env);
			} else {
				os->unset_environment(AUTH_BASE_URL_ENV_FOR_TEST);
			}
		}
	}
};

class FakeAuthTransport : public AuthTransport {
	GDCLASS(FakeAuthTransport, AuthTransport);

public:
	struct Response {
		bool ok = true;
		String body;
		int http_code = 200;
		String error;
	};

	int request_count = 0;
	HTTPClient::Method last_method = HTTPClient::METHOD_GET;
	String last_path;
	String last_body;
	Vector<String> last_headers;
	Vector<String> request_paths;
	Vector<String> request_bodies;
	Vector<Vector<String>> request_headers;
	Vector<Response> responses;

	void push_json(const String &p_body) {
		Response response;
		response.body = p_body;
		responses.push_back(response);
	}

	void push_error(const String &p_error) {
		Response response;
		response.ok = false;
		response.error = p_error;
		response.http_code = 500;
		responses.push_back(response);
	}

	void push_http_error(int p_http_code, const String &p_error, const String &p_body = String()) {
		Response response;
		response.ok = false;
		response.error = p_error;
		response.http_code = p_http_code;
		response.body = p_body;
		responses.push_back(response);
	}

	virtual bool request_json(HTTPClient::Method p_method, const String &p_path, const String &p_body, const Vector<String> &p_headers, String &r_response, int &r_http_code, String &r_error) override {
		request_count++;
		last_method = p_method;
		last_path = p_path;
		last_body = p_body;
		last_headers = p_headers;
		request_paths.push_back(p_path);
		request_bodies.push_back(p_body);
		request_headers.push_back(p_headers);

		Response response;
		if (!responses.is_empty()) {
			response = responses[0];
			responses.remove_at(0);
		} else {
			response.body = "{\"code\":0}";
		}

		r_response = response.body;
		r_http_code = response.http_code;
		r_error = response.error;
		return response.ok;
	}
};

class BlockingAuthTransport : public FakeAuthTransport {
	GDCLASS(BlockingAuthTransport, FakeAuthTransport);

public:
	Semaphore request_started;
	Semaphore release_request;
	bool request_was_on_main_thread = true;

	virtual bool request_json(HTTPClient::Method p_method, const String &p_path, const String &p_body, const Vector<String> &p_headers, String &r_response, int &r_http_code, String &r_error) override {
		request_was_on_main_thread = Thread::is_main_thread();
		request_started.post();
		release_request.wait();
		return FakeAuthTransport::request_json(p_method, p_path, p_body, p_headers, r_response, r_http_code, r_error);
	}
};

static Dictionary parse_json_dictionary(const String &p_json) {
	Ref<JSON> parser;
	parser.instantiate();
	CHECK(parser->parse(p_json) == OK);
	CHECK(parser->get_data().get_type() == Variant::DICTIONARY);
	return parser->get_data();
}

static bool has_header(const Vector<String> &p_headers, const String &p_header) {
	for (int i = 0; i < p_headers.size(); i++) {
		if (p_headers[i] == p_header) {
			return true;
		}
	}
	return false;
}

static bool has_header_prefix(const Vector<String> &p_headers, const String &p_prefix) {
	for (int i = 0; i < p_headers.size(); i++) {
		if (p_headers[i].begins_with(p_prefix)) {
			return true;
		}
	}
	return false;
}

static void check_no_session_credentials_in_storage() {
	Dictionary storage = EditorUserSession::get_session_storage_for_test();
	CHECK_FALSE(storage.has("token"));
	CHECK_FALSE(storage.has("refreshToken"));
}

static bool wait_for_semaphore(const Semaphore &p_semaphore, uint64_t p_timeout_msec = 1000) {
	const uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	while (OS::get_singleton()->get_ticks_msec() - start_time < p_timeout_msec) {
		if (p_semaphore.try_wait()) {
			return true;
		}
		OS::get_singleton()->delay_usec(1000);
	}
	return false;
}

TEST_CASE("[Editor][UserSystem] Auth client defaults to HTTP transport") {
	Ref<AuthClient> client;
	client.instantiate();

	CHECK(client->get_transport().is_valid());
	CHECK(Object::cast_to<AuthHTTPTransport>(*client->get_transport()) != nullptr);
}

TEST_CASE("[Editor][UserSystem] Auth client has no built-in auth server URL") {
	ScopedAuthBaseUrlReset auth_base_url_reset;

	CHECK(AuthClient::get_base_url_for_test().is_empty());
}

TEST_CASE("[Editor][UserSystem] Phone-code login body uses backend auto-register endpoint fields") {
	Dictionary body = AuthClient::build_phone_code_login_body_for_test(" +8613800000000 ", " 2468 ", "device-1");

	CHECK(String(body["phone"]) == "+8613800000000");
	CHECK(String(body["code"]) == "2468");
	CHECK(AuthClient::get_default_scene_for_test() == "client");
	CHECK(String(body["scene"]) == AuthClient::get_default_scene_for_test());
	CHECK(String(body["service"]) == AuthClient::get_default_service_for_test());
	CHECK(String(body["deviceId"]) == "device-1");
	CHECK_FALSE(body.has("password"));
}

TEST_CASE("[Editor][UserSystem] Auth client sends phone-code login without register endpoint") {
	Ref<FakeAuthTransport> transport;
	transport.instantiate();
	transport->push_json("{\"code\":0,\"data\":{\"userId\":\"user-1\",\"token\":\"token-1\",\"refreshToken\":\"refresh-1\"}}");

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	AuthResult result = client->login_with_phone_code(" +8613800000000 ", " 2468 ", "device-1");

	CHECK(result.success);
	CHECK(result.session.user_id == "user-1");
	CHECK(result.session.token == "token-1");
	CHECK(result.session.refresh_token == "refresh-1");
	CHECK(result.session.phone == "+8613800000000");
	CHECK(result.session.device_id == "device-1");
	CHECK(transport->last_method == HTTPClient::METHOD_POST);
	CHECK(transport->last_path == "/v1/auth/validate/phone/code");
	CHECK_FALSE(transport->last_path.contains("/v1/auth/register"));

	Dictionary body = parse_json_dictionary(transport->last_body);
	CHECK(String(body["phone"]) == "+8613800000000");
	CHECK(String(body["code"]) == "2468");
	CHECK(String(body["scene"]) == AuthClient::get_default_scene_for_test());
	CHECK(String(body["service"]) == AuthClient::get_default_service_for_test());
	CHECK(String(body["deviceId"]) == "device-1");
}

TEST_CASE("[Editor][UserSystem] Auth client sends password login with client scene") {
	Ref<FakeAuthTransport> transport;
	transport.instantiate();
	transport->push_json("{\"code\":0,\"data\":{\"userId\":\"user-1\",\"token\":\"token-1\",\"refreshToken\":\"refresh-1\"}}");

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	AuthResult result = client->login_with_password(" +8613800000000 ", "pass-1", "device-1");

	CHECK(result.success);
	CHECK(transport->last_method == HTTPClient::METHOD_POST);
	CHECK(transport->last_path == "/v1/auth/validate/password");

	Dictionary body = parse_json_dictionary(transport->last_body);
	CHECK(String(body["phone"]) == "+8613800000000");
	CHECK(String(body["password"]) == "pass-1");
	CHECK(String(body["scene"]) == AuthClient::get_default_scene_for_test());
	CHECK(String(body["service"]) == AuthClient::get_default_service_for_test());
	CHECK(String(body["deviceId"]) == "device-1");
}

TEST_CASE("[Editor][UserSystem] Auth client redacts JWT diagnostics") {
	const String token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ.signature";

	const String description = AuthClient::describe_token_for_debug_for_test(token);

	CHECK(description.contains("jwt_parts=3"));
	CHECK(description.contains("token_length="));
	CHECK(description.contains("token_sha256_12="));
	CHECK_FALSE(description.contains("token_tail="));
	CHECK_FALSE(description.contains("jwt_header="));
	CHECK_FALSE(description.contains("jwt_payload="));
	CHECK_FALSE(description.contains("John Doe"));

	const String short_description = AuthClient::describe_token_for_debug_for_test("abc123");
	CHECK_FALSE(short_description.contains("token_tail="));
	CHECK_FALSE(short_description.contains("abc123"));
}

TEST_CASE("[Editor][UserSystem] Auth client accepts HTTP-style success codes from auth replies") {
	Ref<FakeAuthTransport> transport;
	transport.instantiate();
	transport->push_json("{\"code\":200}");
	transport->push_json("{\"code\":200,\"message\":\"ok\",\"data\":{\"userId\":\"user-1\",\"token\":\"token-1\",\"refreshToken\":\"refresh-1\"}}");

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	AuthResult send_result = client->send_phone_code("+8613800000000");
	CHECK(send_result.success);

	AuthResult login_result = client->login_with_phone_code("+8613800000000", "2468", "device-1");
	CHECK(login_result.success);
	CHECK(login_result.error.is_empty());
	CHECK(login_result.session.user_id == "user-1");
	CHECK(login_result.session.token == "token-1");
	CHECK(login_result.session.refresh_token == "refresh-1");
}

TEST_CASE("[Editor][UserSystem] Auth client accepts code 1 success replies from auth replies") {
	Ref<FakeAuthTransport> transport;
	transport.instantiate();
	transport->push_json("{\"code\":1}");
	transport->push_json("{\"code\":1,\"message\":\"ok\",\"data\":{\"userId\":\"user-1\",\"token\":\"token-1\",\"refreshToken\":\"refresh-1\"}}");

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	AuthResult send_result = client->send_phone_code("+8613800000000");
	CHECK(send_result.success);

	AuthResult login_result = client->login_with_phone_code("+8613800000000", "2468", "device-1");
	CHECK(login_result.success);
	CHECK(login_result.error.is_empty());
	CHECK(login_result.session.user_id == "user-1");
	CHECK(login_result.session.token == "token-1");
	CHECK(login_result.session.refresh_token == "refresh-1");
}

TEST_CASE("[Editor][UserSystem] Auth client loads user profile and credits with security headers") {
	Ref<FakeAuthTransport> transport;
	transport.instantiate();
	transport->push_json("{\"code\":0,\"data\":{\"id\":\"user-1\",\"nickname\":\"Alex\",\"tags\":[\"dev\"],\"phone\":\"+8613800000000\",\"email\":\"alex@example.test\",\"credits\":\"42\",\"giftCards\":{\"property1\":{\"cardId\":\"card-1\",\"quantity\":\"2\",\"expireAt\":\"2026-12-31\"}}}}");

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	AuthResult result = client->get_user("token-1");

	CHECK(result.success);
	CHECK(result.user.user_id == "user-1");
	CHECK(result.user.nickname == "Alex");
	CHECK(result.user.phone == "+8613800000000");
	CHECK(result.user.email == "alex@example.test");
	CHECK(result.user.credits == "42");
	CHECK(result.user.tags.size() == 1);
	CHECK(result.user.tags[0] == "dev");
	CHECK(result.user.gift_cards.has("property1"));
	Dictionary gift_card = result.user.gift_cards["property1"];
	CHECK(String(gift_card["cardId"]) == "card-1");
	CHECK(String(gift_card["quantity"]) == "2");
	CHECK(String(gift_card["expireAt"]) == "2026-12-31");
	CHECK(transport->last_method == HTTPClient::METHOD_GET);
	CHECK(transport->last_path == "/user/info");
	CHECK(transport->last_body.is_empty());
	CHECK(has_header(transport->last_headers, "sec-token: token-1"));
	CHECK(has_header(transport->last_headers, "sec-sign: 1"));
	CHECK_FALSE(has_header(transport->last_headers, "Authorization: Bearer token-1"));
	CHECK_FALSE(has_header_prefix(transport->last_headers, "Cookie:"));
}

TEST_CASE("[Editor][UserSystem] Auth client parses protobuf int64 JSON fields as strings") {
	AuthResult auth_result;
	CHECK(AuthClient::parse_auth_response_for_test("{\"code\":0,\"data\":{\"userId\":78,\"token\":\"token-1\",\"refreshToken\":\"refresh-1\"}}", auth_result));
	CHECK(auth_result.session.user_id == "78");

	AuthResult user_result;
	CHECK(AuthClient::parse_user_response_for_test("{\"code\":0,\"data\":{\"id\":78,\"nickname\":\"Alex\",\"phone\":\"+8613800000000\",\"email\":\"alex@example.test\",\"credits\":42}}", user_result));
	CHECK(user_result.user.user_id == "78");
	CHECK(user_result.user.nickname == "Alex");
	CHECK(user_result.user.credits == "42");
}

TEST_CASE("[Editor][UserSystem] Auth client refreshes token with body credentials without requiring expired auth headers") {
	Ref<FakeAuthTransport> transport;
	transport.instantiate();
	transport->push_json("{\"code\":0,\"data\":{\"userId\":\"user-1\",\"token\":\"token-2\",\"refreshToken\":\"refresh-2\"}}");

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	AuthSessionData session;
	session.user_id = "user-1";
	session.token = "token-1";
	session.refresh_token = "refresh-1";
	session.device_id = "device-1";
	session.phone = "+8613800000000";

	AuthResult result = client->refresh_token(session);

	CHECK(result.success);
	CHECK(result.session.user_id == "user-1");
	CHECK(result.session.token == "token-2");
	CHECK(result.session.refresh_token == "refresh-2");
	CHECK(result.session.device_id == "device-1");
	CHECK(result.session.phone == "+8613800000000");
	CHECK(transport->last_method == HTTPClient::METHOD_POST);
	CHECK(transport->last_path == "/v1/auth/token/refresh");
	CHECK_FALSE(has_header(transport->last_headers, "Authorization: Bearer token-1"));
	CHECK_FALSE(has_header(transport->last_headers, "sec-token: token-1"));
	CHECK_FALSE(has_header(transport->last_headers, "sec-sign: 1"));
	CHECK_FALSE(has_header_prefix(transport->last_headers, "Cookie:"));

	Dictionary body = parse_json_dictionary(transport->last_body);
	CHECK(String(body["refreshToken"]) == "refresh-1");
	CHECK(String(body["token"]) == "token-1");
	CHECK(String(body["scene"]) == AuthClient::get_default_scene_for_test());
	CHECK(String(body["service"]) == AuthClient::get_default_service_for_test());
	CHECK(String(body["deviceId"]) == "device-1");
}

TEST_CASE("[Editor][UserSystem] Auth client keeps session user id when refresh reply omits it") {
	Ref<FakeAuthTransport> transport;
	transport.instantiate();
	transport->push_json("{\"code\":0,\"data\":{\"token\":\"token-2\",\"refreshToken\":\"refresh-2\"}}");

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	AuthSessionData session;
	session.user_id = "user-1";
	session.token = "token-1";
	session.refresh_token = "refresh-1";
	session.device_id = "device-1";
	session.phone = "+8613800000000";

	AuthResult result = client->refresh_token(session);

	CHECK(result.success);
	CHECK(result.session.user_id == "user-1");
	CHECK(result.session.token == "token-2");
	CHECK(result.session.refresh_token == "refresh-2");
	CHECK(result.session.device_id == "device-1");
	CHECK(result.session.phone == "+8613800000000");
}

TEST_CASE("[Editor][UserSystem] Auth client parses server error messages") {
	AuthResult result;

	CHECK_FALSE(AuthClient::parse_auth_response_for_test("{\"code\":401,\"message\":\"invalid code\"}", result));
	CHECK(result.error == "invalid code");
}

TEST_CASE("[Editor][UserSystem] Session load removes legacy stored credentials") {
	Dictionary original_storage = EditorUserSession::get_session_storage_for_test();

	Dictionary legacy_storage;
	legacy_storage["userId"] = "user-legacy";
	legacy_storage["token"] = "token-legacy";
	legacy_storage["refreshToken"] = "refresh-legacy";
	legacy_storage["deviceId"] = "device-legacy";
	legacy_storage["phone"] = "+8613000000000";
	EditorUserSession::set_session_storage_for_test(legacy_storage);

	AuthSessionData session = EditorUserSession::load_session();

	CHECK(session.user_id == "user-legacy");
	CHECK(session.token.is_empty());
	CHECK(session.refresh_token.is_empty());
	CHECK(session.device_id == "device-legacy");
	CHECK(session.phone == "+8613000000000");
	check_no_session_credentials_in_storage();

	EditorUserSession::set_session_storage_for_test(original_storage);
}

TEST_CASE("[Editor][UserSystem] Manager saves phone-code login session and keeps nickname separate from phone") {
	Dictionary original_storage = EditorUserSession::get_session_storage_for_test();
	EditorUserSession::set_session_storage_for_test(Dictionary());

	Ref<FakeAuthTransport> transport;
	transport.instantiate();
	transport->push_json("{\"code\":0,\"data\":{\"userId\":\"user-1\",\"token\":\"token-1\",\"refreshToken\":\"refresh-1\"}}");
	transport->push_json("{\"code\":0,\"data\":{\"id\":\"user-1\",\"nickname\":\"\",\"phone\":\"\",\"email\":\"\",\"credits\":\"88\"}}");

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	Ref<EditorUserManager> manager;
	manager.instantiate();
	manager->set_auth_client_for_test(client);

	AuthResult result = manager->login_with_phone_code("+8613800000000", "2468");

	CHECK(result.success);
	CHECK(manager->get_state() == EditorUserManager::STATE_LOGGED_IN);
	CHECK(manager->get_display_name().is_empty());
	CHECK(manager->get_credits_text() == "88");

	AuthSessionData session = EditorUserSession::load_session();
	CHECK(session.user_id == "user-1");
	CHECK(session.token.is_empty());
	CHECK(session.refresh_token.is_empty());
	CHECK(session.phone == "+8613800000000");
	CHECK(!session.device_id.is_empty());
	AuthSessionData live_session = manager->get_session_for_test();
	CHECK(live_session.token == "token-1");
	CHECK(live_session.refresh_token == "refresh-1");
	check_no_session_credentials_in_storage();
	REQUIRE(transport->request_count == 2);
	CHECK(transport->request_paths[0] == "/v1/auth/validate/phone/code");
	CHECK(transport->request_paths[1] == "/user/info");
	CHECK(has_header(transport->request_headers[1], "sec-token: token-1"));

	EditorUserSession::set_session_storage_for_test(original_storage);
}

TEST_CASE("[Editor][UserSystem] Manager async requests run transport off the main thread") {
	Ref<BlockingAuthTransport> transport;
	transport.instantiate();
	transport->push_json("{\"code\":1}");

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	Ref<EditorUserManager> manager;
	manager.instantiate();
	manager->set_auth_client_for_test(client);

	CHECK(manager->request_send_phone_code("+8613800000000"));
	CHECK(wait_for_semaphore(transport->request_started));
	CHECK_FALSE(transport->request_was_on_main_thread);
	CHECK(manager->is_request_pending());

	transport->release_request.post();
	manager->wait_for_request_for_test();
	if (MessageQueue::get_main_singleton()) {
		MessageQueue::get_main_singleton()->flush();
	}

	CHECK_FALSE(manager->is_request_pending());
	CHECK(transport->last_path == "/v1/auth/send/phone/code");
}

TEST_CASE("[Editor][UserSystem] Manager async login refreshes profile nickname and credits") {
	Dictionary original_storage = EditorUserSession::get_session_storage_for_test();
	EditorUserSession::set_session_storage_for_test(Dictionary());

	Ref<FakeAuthTransport> transport;
	transport.instantiate();
	transport->push_json("{\"code\":0,\"data\":{\"userId\":\"user-8\",\"token\":\"token-8\",\"refreshToken\":\"refresh-8\"}}");
	transport->push_json("{\"code\":0,\"data\":{\"id\":\"user-8\",\"nickname\":\"Alex\",\"tags\":[\"dev\"],\"ban\":false,\"phone\":\"+8613800000000\",\"email\":\"alex@example.test\",\"credits\":\"42\",\"giftCards\":{}}}");

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	Ref<EditorUserManager> manager;
	manager.instantiate();
	manager->set_auth_client_for_test(client);

	CHECK(manager->request_login_with_phone_code("+8613800000000", "2468"));
	manager->wait_for_request_for_test();
	if (MessageQueue::get_main_singleton()) {
		MessageQueue::get_main_singleton()->flush();
	}
	manager->wait_for_request_for_test();
	if (MessageQueue::get_main_singleton()) {
		MessageQueue::get_main_singleton()->flush();
	}
	manager->wait_for_request_for_test();
	if (MessageQueue::get_main_singleton()) {
		MessageQueue::get_main_singleton()->flush();
	}

	CHECK(manager->get_state() == EditorUserManager::STATE_LOGGED_IN);
	CHECK(manager->get_display_name() == "Alex");
	CHECK(manager->get_credits_text() == "42");
	REQUIRE(transport->request_count == 2);
	CHECK(transport->request_paths[0] == "/v1/auth/validate/phone/code");
	CHECK(transport->request_paths[1] == "/user/info");
	CHECK(transport->last_path == "/user/info");
	CHECK(has_header(transport->request_headers[1], "sec-token: token-8"));

	EditorUserSession::set_session_storage_for_test(original_storage);
}

TEST_CASE("[Editor][UserSystem] Manager does not restore credentials from editor settings") {
	Dictionary original_storage = EditorUserSession::get_session_storage_for_test();

	AuthSessionData session;
	session.user_id = "user-9";
	session.token = "token-9";
	session.refresh_token = "refresh-9";
	session.device_id = "device-9";
	EditorUserSession::save_session(session);
	check_no_session_credentials_in_storage();

	Ref<FakeAuthTransport> transport;
	transport.instantiate();

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	Ref<EditorUserManager> manager;
	manager.instantiate();
	manager->set_auth_client_for_test(client);

	manager->initialize();

	CHECK(manager->get_state() == EditorUserManager::STATE_LOGGED_OUT);
	CHECK(manager->get_display_name().is_empty());
	CHECK(manager->get_credits_text() == "--");
	CHECK(transport->request_count == 0);
	AuthSessionData restored_session = EditorUserSession::load_session();
	CHECK(restored_session.user_id == "user-9");
	CHECK(restored_session.token.is_empty());
	CHECK(restored_session.refresh_token.is_empty());
	CHECK(restored_session.device_id == "device-9");

	EditorUserSession::set_session_storage_for_test(original_storage);
}

TEST_CASE("[Editor][UserSystem] Manager refreshes token and retries profile when profile token is expired") {
	Dictionary original_storage = EditorUserSession::get_session_storage_for_test();
	EditorUserSession::set_session_storage_for_test(Dictionary());

	AuthSessionData session;
	session.user_id = "78";
	session.token = "expired-token";
	session.refresh_token = "refresh-1";
	session.device_id = "device-1";
	session.phone = "+8613800000000";

	Ref<FakeAuthTransport> transport;
	transport.instantiate();
	transport->push_http_error(401, "Authentication server returned HTTP 401.", "{\"code\":401,\"reason\":\"INVALID_TOKEN\",\"message\":\"token is expired\"}");
	transport->push_json("{\"code\":0,\"data\":{\"userId\":\"78\",\"token\":\"token-2\",\"refreshToken\":\"refresh-2\"}}");
	transport->push_json("{\"code\":0,\"data\":{\"id\":\"78\",\"nickname\":\"Fantety\",\"phone\":\"+8613800000000\",\"email\":\"\",\"credits\":\"66\"}}");

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	Ref<EditorUserManager> manager;
	manager.instantiate();
	manager->set_auth_client_for_test(client);
	manager->set_session_for_test(session);

	CHECK(manager->request_refresh_profile());
	for (int i = 0; i < 6; i++) {
		manager->wait_for_request_for_test();
		if (MessageQueue::get_main_singleton()) {
			MessageQueue::get_main_singleton()->flush();
		}
	}

	CHECK(manager->get_state() == EditorUserManager::STATE_LOGGED_IN);
	CHECK(manager->get_display_name() == "Fantety");
	CHECK(manager->get_credits_text() == "66");
	AuthSessionData refreshed_session = manager->get_session_for_test();
	CHECK(refreshed_session.token == "token-2");
	CHECK(refreshed_session.refresh_token == "refresh-2");
	CHECK(refreshed_session.user_id == "78");
	CHECK(refreshed_session.device_id == "device-1");
	check_no_session_credentials_in_storage();
	CHECK(transport->request_count == 3);
	if (transport->request_count == 3) {
		CHECK(transport->request_paths[0] == "/user/info");
		CHECK(transport->request_paths[1] == "/v1/auth/token/refresh");
		CHECK(transport->request_paths[2] == "/user/info");
		CHECK_FALSE(has_header(transport->request_headers[1], "sec-token: expired-token"));
		CHECK(has_header(transport->request_headers[2], "sec-token: token-2"));
	}

	EditorUserSession::set_session_storage_for_test(original_storage);
}

TEST_CASE("[Editor][UserSystem] Manager signs out when expired profile token cannot be refreshed") {
	Dictionary original_storage = EditorUserSession::get_session_storage_for_test();
	EditorUserSession::set_session_storage_for_test(Dictionary());

	AuthSessionData session;
	session.user_id = "78";
	session.token = "expired-token";
	session.refresh_token = "refresh-1";
	session.device_id = "device-1";
	EditorUserSession::save_session(session);

	Ref<FakeAuthTransport> transport;
	transport.instantiate();
	transport->push_http_error(401, "Authentication server returned HTTP 401.", "{\"code\":401,\"reason\":\"INVALID_TOKEN\",\"message\":\"token is expired\"}");
	transport->push_http_error(401, "Authentication server returned HTTP 401.", "{\"code\":401,\"reason\":\"INVALID_TOKEN\",\"message\":\"refresh token expired\"}");

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	Ref<EditorUserManager> manager;
	manager.instantiate();
	manager->set_auth_client_for_test(client);
	manager->set_session_for_test(session);

	CHECK(manager->request_refresh_profile());
	for (int i = 0; i < 6; i++) {
		manager->wait_for_request_for_test();
		if (MessageQueue::get_main_singleton()) {
			MessageQueue::get_main_singleton()->flush();
		}
	}

	CHECK(manager->get_state() == EditorUserManager::STATE_LOGGED_OUT);
	CHECK(manager->get_display_name().is_empty());
	CHECK(manager->get_credits_text() == "--");
	AuthSessionData cleared_session = EditorUserSession::load_session();
	CHECK(cleared_session.user_id.is_empty());
	CHECK(cleared_session.token.is_empty());
	CHECK(cleared_session.refresh_token.is_empty());
	CHECK(cleared_session.device_id == "device-1");
	check_no_session_credentials_in_storage();
	CHECK(transport->request_count == 2);
	if (transport->request_count == 2) {
		CHECK(transport->request_paths[0] == "/user/info");
		CHECK(transport->request_paths[1] == "/v1/auth/token/refresh");
	}

	EditorUserSession::set_session_storage_for_test(original_storage);
}

TEST_CASE("[Editor][UserSystem] Manager keeps auth when profile load fails") {
	Dictionary original_storage = EditorUserSession::get_session_storage_for_test();
	EditorUserSession::set_session_storage_for_test(Dictionary());

	Ref<FakeAuthTransport> transport;
	transport.instantiate();
	transport->push_json("{\"code\":0,\"data\":{\"userId\":\"user-2\",\"token\":\"token-2\",\"refreshToken\":\"refresh-2\"}}");
	transport->push_error("profile unavailable");

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	Ref<EditorUserManager> manager;
	manager.instantiate();
	manager->set_auth_client_for_test(client);

	AuthResult result = manager->login_with_phone_code("+8613900000000", "1357");

	CHECK(result.success);
	CHECK(manager->get_state() == EditorUserManager::STATE_PROFILE_UNAVAILABLE);
	CHECK(manager->get_display_name().is_empty());
	CHECK(manager->get_credits_text() == "--");
	CHECK(manager->get_last_error() == "profile unavailable");

	AuthSessionData session = EditorUserSession::load_session();
	CHECK(session.user_id == "user-2");
	CHECK(session.token.is_empty());
	CHECK(session.refresh_token.is_empty());
	AuthSessionData live_session = manager->get_session_for_test();
	CHECK(live_session.token == "token-2");
	CHECK(live_session.refresh_token == "refresh-2");
	check_no_session_credentials_in_storage();

	EditorUserSession::set_session_storage_for_test(original_storage);
}

TEST_CASE("[Editor][UserSystem] Logout clears local session even when server logout fails") {
	Dictionary original_storage = EditorUserSession::get_session_storage_for_test();

	AuthSessionData session;
	session.user_id = "user-3";
	session.token = "token-3";
	session.refresh_token = "refresh-3";
	session.device_id = "device-3";
	session.phone = "+8613700000000";
	EditorUserSession::save_session(session);

	Ref<FakeAuthTransport> transport;
	transport.instantiate();
	transport->push_error("logout failed");

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	Ref<EditorUserManager> manager;
	manager.instantiate();
	manager->set_auth_client_for_test(client);
	manager->set_session_for_test(session);

	AuthResult result = manager->logout();

	CHECK(result.success);
	CHECK(has_header(transport->last_headers, "sec-token: token-3"));
	CHECK(has_header(transport->last_headers, "sec-sign: 1"));
	CHECK_FALSE(has_header_prefix(transport->last_headers, "Cookie:"));
	Dictionary body = parse_json_dictionary(transport->last_body);
	CHECK(String(body["scene"]) == AuthClient::get_default_scene_for_test());
	CHECK(String(body["service"]) == AuthClient::get_default_service_for_test());
	CHECK(String(body["deviceId"]) == "device-3");
	CHECK(manager->get_state() == EditorUserManager::STATE_LOGGED_OUT);
	AuthSessionData cleared = EditorUserSession::load_session();
	CHECK(cleared.user_id.is_empty());
	CHECK(cleared.token.is_empty());
	CHECK(cleared.refresh_token.is_empty());
	CHECK(cleared.device_id == "device-3");
	check_no_session_credentials_in_storage();

	EditorUserSession::set_session_storage_for_test(original_storage);
}

TEST_CASE("[Editor][UserSystem] Avatar formats nickname initial and credits") {
	AuthUserInfo user;
	AuthSessionData session;
	session.phone = "+8613600000000";
	session.user_id = "user-4";

	CHECK(EditorUserAvatar::get_display_name_for_test(user, session).is_empty());
	CHECK(EditorUserAvatar::get_avatar_initial_for_test("alex") == "A");
	CHECK(EditorUserAvatar::get_avatar_initial_for_test("") == "?");
	CHECK(EditorUserAvatar::format_credits_for_test("123") == "Credits 123");
	CHECK(EditorUserAvatar::format_credits_for_test("") == "Credits --");
	CHECK(EditorUserAvatar::format_credits_value_for_test("123") == "123");
	CHECK(EditorUserAvatar::format_credits_value_for_test("") == "--");

	user.nickname = "Nora";
	CHECK(EditorUserAvatar::get_display_name_for_test(user, session) == "Nora");
}

TEST_CASE("[Editor][UserSystem] Avatar formats user id and falls back to session id") {
	AuthUserInfo user;
	AuthSessionData session;
	session.user_id = "user-4";

	CHECK(EditorUserAvatar::get_user_id_for_test(user, session) == "user-4");
	CHECK(EditorUserAvatar::format_user_id_for_test("user-4") == "ID user-4");
	CHECK(EditorUserAvatar::format_user_id_for_test("") == "ID --");

	user.user_id = "user-5";
	CHECK(EditorUserAvatar::get_user_id_for_test(user, session) == "user-5");
}

TEST_CASE("[Editor][UserSystem] Login dialog validates phone-code fields") {
	EditorUserLoginDialog dialog;
	dialog.build_for_test();

	dialog.set_phone_code_fields_for_test("", "");
	CHECK_FALSE(dialog.can_submit_phone_code_for_test());

	dialog.set_phone_code_fields_for_test("+8613800000000", "");
	CHECK_FALSE(dialog.can_submit_phone_code_for_test());

	dialog.set_phone_code_fields_for_test("+8613800000000", "2468");
	CHECK(dialog.can_submit_phone_code_for_test());
	CHECK(dialog.get_phone_code_phone_for_test() == "+8613800000000");
}

TEST_CASE("[Editor][UserSystem] Login dialog shows 30s cooldown after sending phone code") {
	Ref<FakeAuthTransport> transport;
	transport.instantiate();
	transport->push_json("{\"code\":200}");

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	Ref<EditorUserManager> manager;
	manager.instantiate();
	manager->set_auth_client_for_test(client);

	EditorUserLoginDialog dialog;
	dialog.set_manager(manager);
	dialog.build_for_test();
	dialog.set_phone_code_fields_for_test("13800000000", "");

	CHECK_FALSE(dialog.is_send_code_button_disabled_for_test());
	CHECK(dialog.get_send_code_button_text_for_test() == "Send Code");

	dialog.send_phone_code_for_test();

	CHECK(transport->last_path == "/v1/auth/send/phone/code");
	Dictionary body = parse_json_dictionary(transport->last_body);
	CHECK(String(body["phone"]) == "+8613800000000");
	CHECK(dialog.get_send_code_cooldown_for_test() == 30);
	CHECK(dialog.is_send_code_button_disabled_for_test());
	CHECK(dialog.get_send_code_button_text_for_test() == "Send Code (30s)");
}

TEST_CASE("[Editor][UserSystem] Login dialog formats local phone numbers with country code") {
	CHECK(EditorUserLoginDialog::format_phone_for_test("+86", "13800000000") == "+8613800000000");
	CHECK(EditorUserLoginDialog::format_phone_for_test("86", "013800000000") == "+8613800000000");
	CHECK(EditorUserLoginDialog::format_phone_for_test("+86", "+14155550100") == "+14155550100");
}

} // namespace TestEditorUserSystem
