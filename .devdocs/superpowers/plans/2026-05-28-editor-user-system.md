# Editor User System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an isolated editor user system with phone-code login/auto-registration, password login, persisted auth state, startup token refresh, profile/score loading, and a top-right avatar/score widget.

**Architecture:** Add a focused `editor/user_system` module. Keep HTTP, session persistence, state coordination, and UI in separate files; keep `EditorNode` changes to a small mount point in `right_menu_hb`.

**Tech Stack:** Godot editor C++, `HTTPClient`, `JSON`, `EditorSettings` or existing editor persistence for local session data, editor `Control` UI, doctest-based C++ tests under `tests/editor`.

---

## Constraints

- Do not run SCons or any build command unless the user explicitly permits it.
- Prefer existing test binaries when available. If no usable test binary exists, record tests as not run because building is disallowed.
- Do not expose NexusServer base URL in UI or editor settings. Keep it as one code-level constant in `AuthClient`.
- Do not call `/v1/auth/register`; phone-code validation auto-registers on the backend.
- Keep `EditorNode` changes minimal to reduce upstream merge conflicts.

## File Structure

- Create `editor/user_system/SCsub`
  - Adds all `editor/user_system/*.cpp` files to `env.editor_sources`.
- Modify `editor/SCsub`
  - Add `SConscript("user_system/SCsub")` near `ai_component/SCsub`.
- Create `editor/user_system/auth_client.h`
  - Declares `AuthClient`, `AuthSessionData`, `AuthUserInfo`, `AuthResult`, and a transport interface for tests.
- Create `editor/user_system/auth_client.cpp`
  - Implements request body construction, response parsing, endpoint methods, and HTTP transport.
- Create `editor/user_system/editor_user_session.h`
  - Declares `EditorUserSession` and `EditorUserSessionData`.
- Create `editor/user_system/editor_user_session.cpp`
  - Implements centralized save/load/clear and device ID generation.
- Create `editor/user_system/editor_user_manager.h`
  - Declares `EditorUserManager`, state enum, signals, and auth/profile operations.
- Create `editor/user_system/editor_user_manager.cpp`
  - Coordinates startup refresh, login, profile refresh, logout, and state transitions.
- Create `editor/user_system/editor_user_login_dialog.h`
  - Declares `EditorUserLoginDialog`.
- Create `editor/user_system/editor_user_login_dialog.cpp`
  - Builds phone-code login/register and password login tabs.
- Create `editor/user_system/editor_user_avatar.h`
  - Declares `EditorUserAvatar`.
- Create `editor/user_system/editor_user_avatar.cpp`
  - Builds top-right account button/menu and score display.
- Modify `editor/register_editor_types.cpp`
  - Include and register `AuthClient`, `EditorUserManager`, `EditorUserLoginDialog`, and `EditorUserAvatar` if they use `GDCLASS`.
- Modify `editor/editor_node.h`
  - Add forward declaration and member pointer for `EditorUserAvatar`.
- Modify `editor/editor_node.cpp`
  - Include `editor/user_system/editor_user_avatar.h`.
  - Create `EditorUserAvatar` and add it to `right_menu_hb` after the renderer selector and before update spinner.
- Create `tests/editor/test_user_system.cpp`
  - Covers `AuthClient`, `EditorUserSession`, `EditorUserManager`, and display-format helpers from `EditorUserAvatar`.

## Task 1: Add User-System Build Skeleton

**Files:**
- Create: `editor/user_system/SCsub`
- Create: `editor/user_system/auth_client.h`
- Create: `editor/user_system/auth_client.cpp`
- Modify: `editor/SCsub`
- Test: none yet

- [ ] **Step 1: Create the module `SCsub`**

```python
#!/usr/bin/env python
from misc.utility.scons_hints import *

Import("env")

env.add_source_files(env.editor_sources, "*.cpp")
```

- [ ] **Step 2: Add the module to editor build scripts**

In `editor/SCsub`, add:

```python
SConscript("user_system/SCsub")
```

Place it near `SConscript("ai_component/SCsub")`.

- [ ] **Step 3: Create placeholder `AuthClient` files**

`editor/user_system/auth_client.h`:

```cpp
#pragma once

#include "core/object/ref_counted.h"

class AuthClient : public RefCounted {
	GDCLASS(AuthClient, RefCounted);

protected:
	static void _bind_methods();
};
```

`editor/user_system/auth_client.cpp`:

```cpp
#include "auth_client.h"

void AuthClient::_bind_methods() {
}
```

- [ ] **Step 4: Register `AuthClient`**

In `editor/register_editor_types.cpp`, include:

```cpp
#include "editor/user_system/auth_client.h"
```

Near existing editor/AI registrations, add:

```cpp
GDREGISTER_CLASS(AuthClient);
```

- [ ] **Step 5: Verification**

Do not build. Run:

```powershell
git diff -- editor/SCsub editor/register_editor_types.cpp editor/user_system
```

Expected: only build skeleton and `AuthClient` registration changes.

- [ ] **Step 6: Commit**

```bash
git add editor/SCsub editor/register_editor_types.cpp editor/user_system/SCsub editor/user_system/auth_client.h editor/user_system/auth_client.cpp
git commit -m "feat: add editor user system module skeleton"
```

## Task 2: Implement AuthClient Request/Response Logic With Tests

**Files:**
- Modify: `editor/user_system/auth_client.h`
- Modify: `editor/user_system/auth_client.cpp`
- Create: `tests/editor/test_user_system.cpp`

- [ ] **Step 1: Write failing tests for request body construction and response parsing**

Create `tests/editor/test_user_system.cpp`:

```cpp
#include "tests/test_macros.h"

#include "editor/user_system/auth_client.h"

TEST_FORCE_LINK(test_user_system);

namespace TestUserSystem {

TEST_CASE("[Editor][UserSystem] AuthClient builds phone code login body") {
	Dictionary body = AuthClient::build_phone_code_login_body_for_test("+8613800000000", "123456", "device-1");

	CHECK(String(body["phone"]) == "+8613800000000");
	CHECK(String(body["code"]) == "123456");
	CHECK(String(body["deviceId"]) == "device-1");
	CHECK(String(body["scene"]) == AuthClient::get_default_scene_for_test());
}

TEST_CASE("[Editor][UserSystem] AuthClient parses auth user response") {
	const String json = R"({"code":0,"message":"ok","data":{"userId":"1001","token":"token-a","refreshToken":"refresh-a"}})";

	AuthResult result;
	CHECK(AuthClient::parse_auth_response_for_test(json, result));
	CHECK(result.error.is_empty());
	CHECK(result.session.user_id == "1001");
	CHECK(result.session.token == "token-a");
	CHECK(result.session.refresh_token == "refresh-a");
}

TEST_CASE("[Editor][UserSystem] AuthClient parses backend error message") {
	const String json = R"({"code":400,"message":"invalid code"})";

	AuthResult result;
	CHECK_FALSE(AuthClient::parse_auth_response_for_test(json, result));
	CHECK(result.error == "invalid code");
}

}
```

- [ ] **Step 2: Run test to verify it fails**

Run only if an existing test binary is available, for example:

```powershell
bin\next.windows.editor.x86_64.console.exe --test --test-case="[Editor][UserSystem]*"
```

Expected: fail to compile/run because helper APIs do not exist, or fail at runtime if compilation already includes the test. If no suitable binary exists, record: "Not run: build command disallowed and no current test binary available."

- [ ] **Step 3: Implement minimal data structs and helper APIs**

In `auth_client.h`, add:

```cpp
#include "core/string/ustring.h"
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
```

Expose test helpers on `AuthClient`:

```cpp
static String get_default_scene_for_test();
static Dictionary build_phone_code_login_body_for_test(const String &p_phone, const String &p_code, const String &p_device_id);
static bool parse_auth_response_for_test(const String &p_json, AuthResult &r_result);
```

- [ ] **Step 4: Implement JSON parsing**

Use `JSON::parse_string()` or project-local JSON parse style. Treat `code == 0` as success. On error, set `r_result.error` from `message`, or `"Authentication request failed."`.

- [ ] **Step 5: Run focused test**

Same command as Step 2, if possible.

Expected: the new `AuthClient` helper tests pass.

- [ ] **Step 6: Commit**

```bash
git add editor/user_system/auth_client.h editor/user_system/auth_client.cpp tests/editor/test_user_system.cpp
git commit -m "feat: add auth client parsing helpers"
```

## Task 3: Add AuthClient Transport and Endpoint Methods

**Files:**
- Modify: `editor/user_system/auth_client.h`
- Modify: `editor/user_system/auth_client.cpp`
- Modify: `tests/editor/test_user_system.cpp`

- [ ] **Step 1: Write failing tests using a fake transport**

Add a fake transport in `tests/editor/test_user_system.cpp`:

```cpp
class FakeAuthTransport : public AuthTransport {
	GDCLASS(FakeAuthTransport, AuthTransport);

public:
	HTTPClient::Method last_method = HTTPClient::METHOD_GET;
	String last_path;
	String last_body;
	String response = R"({"code":0,"data":{"userId":"1","token":"t","refreshToken":"r"}})";
	int http_code = 200;
	String error;

	virtual bool request_json(HTTPClient::Method p_method, const String &p_path, const String &p_body, String &r_response, int &r_http_code, String &r_error) override {
		last_method = p_method;
		last_path = p_path;
		last_body = p_body;
		r_response = response;
		r_http_code = http_code;
		r_error = error;
		return error.is_empty();
	}
};

TEST_CASE("[Editor][UserSystem] AuthClient sends phone code login endpoint") {
	Ref<FakeAuthTransport> transport;
	transport.instantiate();

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	AuthResult result = client->login_with_phone_code("+8613800000000", "123456", "device-1");
	CHECK(result.success);
	CHECK(transport->last_method == HTTPClient::METHOD_POST);
	CHECK(transport->last_path == "/v1/auth/validate/phone/code");
	CHECK(transport->last_body.contains("\"phone\":\"+8613800000000\""));
}
```

- [ ] **Step 2: Run focused test to verify it fails**

Use the existing test binary if available. Expected: fail because `AuthTransport`, `set_transport`, or endpoint methods do not exist.

- [ ] **Step 3: Add transport interface and endpoint methods**

In `auth_client.h`:

```cpp
#include "core/io/http_client.h"

class AuthTransport : public RefCounted {
	GDCLASS(AuthTransport, RefCounted);

protected:
	static void _bind_methods();

public:
	virtual bool request_json(HTTPClient::Method p_method, const String &p_path, const String &p_body, String &r_response, int &r_http_code, String &r_error);
};
```

Add `AuthHTTPTransport : public AuthTransport` for real network calls. Add methods on `AuthClient`:

```cpp
void set_transport(const Ref<AuthTransport> &p_transport);
AuthResult send_phone_code(const String &p_phone);
AuthResult login_with_phone_code(const String &p_phone, const String &p_code, const String &p_device_id);
AuthResult login_with_password(const String &p_phone, const String &p_password, const String &p_device_id);
AuthResult refresh_token(const AuthSessionData &p_session);
AuthResult logout(const AuthSessionData &p_session, bool p_all = false);
AuthResult get_user(const String &p_user_id);
```

- [ ] **Step 4: Implement real HTTP transport**

Use `HTTPClient` blocking mode following `editor/ai_component/providers/ai_openai_runtime_client.cpp` style:

- Parse hardcoded base URL.
- Connect to host with TLS when scheme is `https://`.
- Send `Content-Type: application/json` and `Accept: application/json`.
- Return HTTP code, response body, and transport error.

- [ ] **Step 5: Run focused tests if possible**

Expected: fake transport endpoint tests pass. No real network calls.

- [ ] **Step 6: Commit**

```bash
git add editor/user_system/auth_client.h editor/user_system/auth_client.cpp tests/editor/test_user_system.cpp
git commit -m "feat: add auth client endpoint transport"
```

## Task 4: Implement EditorUserSession

**Files:**
- Create: `editor/user_system/editor_user_session.h`
- Create: `editor/user_system/editor_user_session.cpp`
- Modify: `editor/register_editor_types.cpp`
- Modify: `tests/editor/test_user_system.cpp`

- [ ] **Step 1: Write failing session persistence tests**

Add tests:

```cpp
TEST_CASE("[Editor][UserSystem] EditorUserSession saves loads and clears session") {
	AuthSessionData original = EditorUserSession::get_session_for_test();
	EditorUserSession::clear_session();

	AuthSessionData session;
	session.user_id = "1001";
	session.token = "token";
	session.refresh_token = "refresh";
	session.device_id = "device-1";
	session.phone = "+8613800000000";

	EditorUserSession::save_session(session);
	AuthSessionData loaded = EditorUserSession::load_session();
	CHECK(loaded.user_id == "1001");
	CHECK(loaded.refresh_token == "refresh");
	CHECK(loaded.phone == "+8613800000000");

	EditorUserSession::clear_session();
	CHECK(EditorUserSession::load_session().refresh_token.is_empty());

	EditorUserSession::set_session_for_test(original);
}

TEST_CASE("[Editor][UserSystem] EditorUserSession reuses generated device id") {
	EditorUserSession::clear_session();
	String first = EditorUserSession::get_or_create_device_id();
	String second = EditorUserSession::get_or_create_device_id();
	CHECK(!first.is_empty());
	CHECK(first == second);
	EditorUserSession::clear_session();
}
```

- [ ] **Step 2: Run focused test to verify it fails**

Expected: fail because `EditorUserSession` does not exist.

- [ ] **Step 3: Implement session storage**

Use centralized storage keys under a private editor path such as:

```cpp
static const char *SESSION_PATH = "user_system/session";
```

Store as a `Dictionary` containing the fields. Use `EditorSettings::get_singleton()->set(...)` and `get(...)` following `AIModelSettings` style. Provide test-only helpers to snapshot/restore the session dictionary.

- [ ] **Step 4: Register class if using `GDCLASS`**

Include and `GDREGISTER_CLASS(EditorUserSession)` in `editor/register_editor_types.cpp`.

- [ ] **Step 5: Run focused tests if possible**

Expected: session tests pass.

- [ ] **Step 6: Commit**

```bash
git add editor/user_system/editor_user_session.h editor/user_system/editor_user_session.cpp editor/register_editor_types.cpp tests/editor/test_user_system.cpp
git commit -m "feat: add editor user session storage"
```

## Task 5: Implement EditorUserManager State Flow

**Files:**
- Create: `editor/user_system/editor_user_manager.h`
- Create: `editor/user_system/editor_user_manager.cpp`
- Modify: `editor/register_editor_types.cpp`
- Modify: `tests/editor/test_user_system.cpp`

- [ ] **Step 1: Write failing manager tests with fake auth client**

Create an injectable fake client or fake transport. Test:

```cpp
TEST_CASE("[Editor][UserSystem] User manager login saves session and loads profile") {
	Ref<FakeAuthTransport> transport;
	transport.instantiate();
	transport->response = R"({"code":0,"data":{"userId":"1001","token":"token-a","refreshToken":"refresh-a"}})";

	Ref<AuthClient> client;
	client.instantiate();
	client->set_transport(transport);

	EditorUserManager manager;
	manager.set_auth_client_for_test(client);
	manager.login_with_phone_code("+8613800000000", "123456");

	CHECK(manager.get_state() == EditorUserManager::STATE_LOGGED_IN);
	CHECK(manager.get_session_for_test().user_id == "1001");
}
```

Also add tests for refresh failure clearing session and profile failure keeping auth with unavailable score.

- [ ] **Step 2: Run focused tests to verify they fail**

Expected: fail because manager does not exist.

- [ ] **Step 3: Implement manager**

Expose:

```cpp
enum State {
	STATE_LOGGED_OUT,
	STATE_REFRESHING,
	STATE_LOGGING_IN,
	STATE_LOGGED_IN,
	STATE_PROFILE_UNAVAILABLE,
};

void initialize();
void login_with_phone_code(const String &p_phone, const String &p_code);
void login_with_password(const String &p_phone, const String &p_password);
void refresh_profile();
void logout();
State get_state() const;
AuthUserInfo get_user_info() const;
String get_score_text() const;
String get_display_name() const;
```

Use signals:

```cpp
ADD_SIGNAL(MethodInfo("state_changed"));
ADD_SIGNAL(MethodInfo("profile_changed"));
ADD_SIGNAL(MethodInfo("error", PropertyInfo(Variant::STRING, "message")));
```

- [ ] **Step 4: Add display fallback helpers**

Implement display-name fallback:

1. `nickname`
2. session phone
3. `user_id`
4. empty/generic

Expose helper functions for tests if needed.

- [ ] **Step 5: Run focused tests if possible**

Expected: manager state tests pass.

- [ ] **Step 6: Commit**

```bash
git add editor/user_system/editor_user_manager.h editor/user_system/editor_user_manager.cpp editor/register_editor_types.cpp tests/editor/test_user_system.cpp
git commit -m "feat: add editor user manager"
```

## Task 6: Implement Login Dialog UI

**Files:**
- Create: `editor/user_system/editor_user_login_dialog.h`
- Create: `editor/user_system/editor_user_login_dialog.cpp`
- Modify: `editor/register_editor_types.cpp`
- Modify: `tests/editor/test_user_system.cpp`

- [ ] **Step 1: Write failing UI tests for basic dialog behavior**

Test the public test helpers:

```cpp
TEST_CASE("[Editor][UserSystem] Login dialog validates phone code fields") {
	EditorUserLoginDialog dialog;
	dialog.build_for_test();
	dialog.set_phone_code_fields_for_test("", "");
	CHECK_FALSE(dialog.can_submit_phone_code_for_test());

	dialog.set_phone_code_fields_for_test("+8613800000000", "123456");
	CHECK(dialog.can_submit_phone_code_for_test());
}
```

- [ ] **Step 2: Run focused tests to verify failure**

Expected: fail because dialog does not exist.

- [ ] **Step 3: Build the dialog**

Use existing editor UI patterns:

- Inherit `AcceptDialog` or `ConfirmationDialog`.
- Use `TabContainer`.
- Phone-code tab:
  - `LineEdit *phone_code_phone`
  - `LineEdit *phone_code_code`
  - `Button *send_code_button`
  - `Button *phone_code_login_button`
- Password tab:
  - `LineEdit *password_phone`
  - `LineEdit *password_password` with `set_secret(true)`
  - `Button *password_login_button`
- Shared status/error `Label`.

- [ ] **Step 4: Connect to `EditorUserManager`**

Dialog calls:

```cpp
manager->send_phone_code(phone);
manager->login_with_phone_code(phone, code);
manager->login_with_password(phone, password);
```

Disable active controls while a request is in progress. Show error label on manager `error`.

- [ ] **Step 5: Implement send-code cooldown**

Use a `Timer` child and an integer countdown. Keep this local to the dialog.

- [ ] **Step 6: Run focused tests if possible**

Expected: dialog validation tests pass.

- [ ] **Step 7: Commit**

```bash
git add editor/user_system/editor_user_login_dialog.h editor/user_system/editor_user_login_dialog.cpp editor/register_editor_types.cpp tests/editor/test_user_system.cpp
git commit -m "feat: add editor user login dialog"
```

## Task 7: Implement Top-Right Avatar And Score Widget

**Files:**
- Create: `editor/user_system/editor_user_avatar.h`
- Create: `editor/user_system/editor_user_avatar.cpp`
- Modify: `editor/register_editor_types.cpp`
- Modify: `tests/editor/test_user_system.cpp`

- [ ] **Step 1: Write failing tests for display formatting**

```cpp
TEST_CASE("[Editor][UserSystem] Avatar display falls back from nickname to phone to user id") {
	AuthUserInfo info;
	AuthSessionData session;
	session.user_id = "1001";
	session.phone = "+8613800000000";

	CHECK(EditorUserAvatar::get_display_name_for_test(info, session) == "+8613800000000");
	info.nickname = "Fante";
	CHECK(EditorUserAvatar::get_display_name_for_test(info, session) == "Fante");
}

TEST_CASE("[Editor][UserSystem] Avatar formats score") {
	CHECK(EditorUserAvatar::format_score_for_test("1234") == "Score 1234");
	CHECK(EditorUserAvatar::format_score_for_test("") == "Score --");
}
```

- [ ] **Step 2: Run focused tests to verify failure**

Expected: fail because avatar does not exist.

- [ ] **Step 3: Build widget**

Inherit `HBoxContainer`. Create:

- Avatar `Button` or `MenuButton`.
- Optional account name `Label`.
- Score `Label`.
- `PopupMenu` for logged-in account actions.
- Owned `EditorUserLoginDialog`.

Use a compact minimum size and `FlatMenuButton`-style variations where appropriate.

- [ ] **Step 4: Bind manager state to UI**

Connect to manager signals and refresh:

- logged out: generic account button, hide score or show `Score --`.
- logged in: avatar initial, account name if space allows, score.
- profile unavailable: keep avatar/account fallback, show `Score --`.

- [ ] **Step 5: Implement interactions**

- Logged-out click: popup login dialog.
- Logged-in click: popup menu.
- Menu `Refresh Account Info`: call `manager->refresh_profile()`.
- Menu `Log Out`: call `manager->logout()`.

- [ ] **Step 6: Run focused tests if possible**

Expected: avatar helper tests pass.

- [ ] **Step 7: Commit**

```bash
git add editor/user_system/editor_user_avatar.h editor/user_system/editor_user_avatar.cpp editor/register_editor_types.cpp tests/editor/test_user_system.cpp
git commit -m "feat: add editor user avatar widget"
```

## Task 8: Integrate Avatar Into EditorNode

**Files:**
- Modify: `editor/editor_node.h`
- Modify: `editor/editor_node.cpp`

- [ ] **Step 1: Inspect current insertion point**

Read the `right_menu_hb` construction in `editor/editor_node.cpp` around renderer and update spinner.

- [ ] **Step 2: Add forward declaration/member**

In `editor/editor_node.h`:

```cpp
class EditorUserAvatar;
```

Add member:

```cpp
EditorUserAvatar *user_avatar = nullptr;
```

- [ ] **Step 3: Include the widget**

In `editor/editor_node.cpp`:

```cpp
#include "editor/user_system/editor_user_avatar.h"
```

- [ ] **Step 4: Add the widget to `right_menu_hb`**

After:

```cpp
right_menu_hb->add_child(renderer);
```

Add:

```cpp
user_avatar = memnew(EditorUserAvatar);
right_menu_hb->add_child(user_avatar);
```

Keep this block minimal. Do not add auth logic to `EditorNode`.

- [ ] **Step 5: Verify diff scope**

Run:

```powershell
git diff -- editor/editor_node.h editor/editor_node.cpp
```

Expected: only include, forward declaration/member, and widget mount.

- [ ] **Step 6: Commit**

```bash
git add editor/editor_node.h editor/editor_node.cpp
git commit -m "feat: mount editor user avatar in title bar"
```

## Task 9: Final Wiring And Manual Validation Notes

**Files:**
- Modify as needed: `editor/user_system/*.cpp`
- Modify as needed: `tests/editor/test_user_system.cpp`

- [ ] **Step 1: Ensure manager initializes**

Decide whether `EditorUserAvatar` owns/initializes a singleton `EditorUserManager` or retrieves an existing singleton. Keep ownership localized so `EditorNode` does not manage auth lifecycle.

- [ ] **Step 2: Ensure no real network calls occur in tests**

All tests must use fake transport or pure helper methods.

- [ ] **Step 3: Run focused tests only if possible without building**

Use existing binary if available:

```powershell
bin\next.windows.editor.x86_64.console.exe --test --test-case="[Editor][UserSystem]*"
```

Expected: user-system tests pass.

If no current binary exists, record:

```text
Not run: build commands are disallowed and no suitable current test binary was available.
```

- [ ] **Step 4: Run formatting/checks allowed by user**

Do not run build. If the project has a non-build formatting command available and approved, run it. Otherwise inspect diffs manually.

- [ ] **Step 5: Commit fixes**

```bash
git add editor/user_system tests/editor/test_user_system.cpp editor/editor_node.h editor/editor_node.cpp editor/SCsub editor/register_editor_types.cpp
git commit -m "test: cover editor user system"
```

Skip this commit if there are no remaining changes after previous task commits.

## Implementation Notes

- `AuthClient` base URL placeholder should be a single constant near the top of `auth_client.cpp`, e.g.:

```cpp
static const char *NEXUS_SERVER_BASE_URL = "https://TODO.example";
```

- Replace the placeholder before real release.
- Prefer compact JSON bodies via `JSON::stringify(Dictionary())`.
- Do not log tokens.
- Clear password/code fields after successful login.
- Use `TTR/TTRC` for visible UI strings where existing editor code does.
- Avoid broad refactors in `EditorNode`.

