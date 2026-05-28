# Editor User System Design

## Goal

Add a first-party editor user system to this Godot-derived engine. The system adds a top-right account avatar, persistent score display, login dialog, token persistence, automatic token refresh, and user profile loading against the NexusServer authentication APIs.

The implementation should treat this engine as a fork where editor layout changes are allowed, while keeping upstream merge conflicts small. `EditorNode` should only host the account widget; user-system behavior should live in an isolated module.

## Non-Goals

- Do not expose the NexusServer base URL as an editor setting.
- Do not call `/v1/auth/register` in the first version.
- Do not force profile completion after backend auto-registration.
- Do not implement email login, third-party auth, two-factor auth UI, payments, gift cards, score history, or order flows.

## API Scope

The first version uses these endpoints from `docs/NexusServer.openapi.json`:

- `POST /v1/auth/send/phone/code`
  - Request: `phone`.
  - Used by the phone-code login/register tab.
- `POST /v1/auth/validate/phone/code`
  - Request: `phone`, `code`, `scene`, `deviceId`.
  - This is the quick login/register path. The backend auto-registers the phone account if it does not exist.
- `POST /v1/auth/validate/password`
  - Request: `phone`, `password`, `twoFactorAuth`, `scene`, `deviceId`.
  - Used by existing password-auth accounts.
- `POST /v1/auth/token/refresh`
  - Request: `refreshToken`, `token`, `scene`, `service`, `deviceId`.
  - Used on editor startup when a saved session exists.
- `POST /v1/auth/logout`
  - Request: `scene`, `service`, `all`, `deviceId`.
  - Used when the user logs out. Local logout should still complete if the network call fails.
- `GET /user/{id}`
  - Used after login or token refresh to load `id`, `nickname`, `phone`, `email`, and `score`.

`AuthUser` responses are expected to provide at least `userId`, `token`, and `refreshToken`.

## Module Layout

Add a new module under `editor/user_system/`:

- `auth_client.h/.cpp`
  - Class: `AuthClient`.
  - Owns the hardcoded NexusServer base URL as a single internal code constant.
  - Builds HTTP requests and parses auth/user responses.
  - Does not own UI or persistent state.
- `editor_user_session.h/.cpp`
  - Class: `EditorUserSession`.
  - Persists and clears `userId`, `token`, `refreshToken`, `deviceId`, and last phone.
  - Generates a stable `deviceId` if none exists.
  - Does not call network APIs.
- `editor_user_manager.h/.cpp`
  - Class: `EditorUserManager`.
  - Owns the current auth state and user profile.
  - Coordinates startup refresh, login, logout, and profile refresh.
  - Emits state/profile change signals for UI.
- `editor_user_avatar.h/.cpp`
  - Class: `EditorUserAvatar`.
  - Top-right widget that shows avatar initial and persistent score.
  - Opens login dialog when logged out and account menu when logged in.
- `editor_user_login_dialog.h/.cpp`
  - Class: `EditorUserLoginDialog`.
  - Contains phone-code login/register and password login tabs.
- `SCsub`
  - Adds the new files to the editor build.

`editor/editor_node.cpp` should only include the avatar header, create the widget, connect it to the manager if needed, and add it to `right_menu_hb`. This keeps the upstream conflict surface small.

## Editor Integration

The account widget should be inserted into `EditorNode` near the existing title-bar right controls:

- Current insertion point: `right_menu_hb` in `editor/editor_node.cpp`.
- Preferred order: after the renderer selector and before the update spinner.
- `EditorNode` should not contain login logic, HTTP logic, token parsing, or profile formatting.

On startup, `EditorUserManager` initializes before or during UI construction so the avatar can show an initial loading/logged-out state and update when refresh completes.

## State Model

The manager tracks these states:

- `LoggedOut`
  - No usable saved auth state or refresh failed.
- `Refreshing`
  - A saved refresh token exists and startup refresh is in progress.
- `LoggingIn`
  - A user-triggered login is in progress.
- `LoggedIn`
  - Valid session is stored and profile loading has succeeded or is in progress.
- `ProfileUnavailable`
  - Auth is valid, but `/user/{id}` failed. The user stays logged in; score displays as unavailable.

Profile display priority:

1. `nickname`
2. saved phone
3. `userId`
4. generic account glyph

Avatar initial uses the first character from the selected display name. If no display name exists, use a generic account icon.

## Session Persistence

Persist:

- `userId`
- `token`
- `refreshToken`
- `deviceId`
- last used `phone`

The values should not be user-editable in the UI. The design does not require a project setting or editor preference for server URL. If the existing codebase has secure credential storage available, prefer it for tokens; otherwise use the editor's existing persistent configuration mechanism in a centralized `EditorUserSession` implementation.

Startup flow:

1. Load saved session.
2. If no refresh token exists, enter `LoggedOut`.
3. Call `AuthClient::refresh_token()`.
4. If refresh succeeds, persist new token values.
5. Call `AuthClient::get_user()`.
6. Update profile and score if user lookup succeeds.
7. If refresh fails, clear session and enter `LoggedOut`.

## Login Dialog

The dialog has two tabs.

### Phone-Code Login/Register

Fields:

- Phone
- Verification code

Actions:

- `Send Code`: calls `/v1/auth/send/phone/code`.
- `Login`: calls `/v1/auth/validate/phone/code`.

Behavior:

- This tab is the quick login/register path.
- If the phone account does not exist, the backend auto-registers it.
- The editor does not call `/v1/auth/register`.
- After success, save session, load profile, update avatar and score, close the dialog.
- If no nickname exists after auto-registration, fall back to phone or user ID.

### Password Login

Fields:

- Phone
- Password

Action:

- `Login`: calls `/v1/auth/validate/password`.

Behavior:

- Intended for existing password-auth accounts.
- After success, follows the same session-save and profile-load path as phone-code login.

## Top-Right Account Widget

Logged-out state:

- Show a compact account button/icon.
- Click opens `EditorUserLoginDialog`.

Logged-in state:

- Show circular avatar initial.
- Show persistent score text, for example `Score 1234`.
- Show account name when space allows.
- If score is unavailable, display `Score --`.

Click menu:

- Display nickname, phone or user ID.
- Display score.
- Provide `Refresh Account Info`.
- Provide `Log Out`.

When space is constrained, avatar and score have priority over account name.

## Error Handling

- HTTP non-2xx: surface a concise error in the dialog or menu action.
- JSON response with `code != 0`: show backend `message` when present, otherwise use a generic failure message.
- Send-code failure: keep the phone/code tab open and re-enable controls.
- Login failure: keep the dialog open, clear only sensitive transient state if appropriate, and show the error.
- Startup refresh failure: clear saved session and silently show logged-out state.
- Profile lookup failure after valid auth: keep session, display `Score --`, and allow manual profile refresh.
- Logout network failure: still clear local session and update UI to logged out.

## Testing Strategy

Do not run build commands unless explicitly allowed later.

Target tests:

- `AuthClient`
  - Builds expected request paths and JSON bodies.
  - Parses success responses containing `AuthUser`.
  - Parses backend error responses and HTTP error cases.
- `EditorUserSession`
  - Saves, loads, and clears session.
  - Generates and reuses `deviceId`.
- `EditorUserManager`
  - Startup refresh success persists new tokens and loads profile.
  - Startup refresh failure clears session.
  - Phone-code login success handles backend auto-registration as ordinary login.
  - Profile lookup failure keeps auth and marks score unavailable.
  - Logout clears local state even when server logout fails.
- `EditorUserAvatar`
  - Display-name fallback order: nickname, phone, user ID, generic.
  - Score formatting, including unavailable score.
  - Logged-out click opens login dialog.
- `EditorUserLoginDialog`
  - Validates required fields.
  - Disables controls while requests are in flight.
  - Shows send-code/login errors.

## Open Questions For Implementation

- Exact hardcoded NexusServer base URL value must be filled in before implementation or left as a single TODO constant.
- Token storage can be upgraded if the engine has a secure credential store; otherwise use centralized editor persistence.
- UI copy may be localized later; first implementation can follow existing editor translation patterns with `TTR/TTRC`.

