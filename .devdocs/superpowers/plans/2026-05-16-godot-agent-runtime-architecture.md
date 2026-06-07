# Godot Agent Runtime Architecture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a production-grade, read-only Agent Runtime foundation with structured tool calling, high-cohesion modules, and safe project inspection tools.

**Architecture:** Keep all AI business logic inside `editor/ai_component`. Introduce a focused `tools` layer first, then agent profiles, provider tool schemas, runtime loop, session persistence, and UI rendering in separate steps. The runtime uses a ReAct-style loop internally, but provider integration uses native function/tool calling rather than parsing free-form text.

**Tech Stack:** Godot Engine C++, Editor-only code under `editor/ai_component`, JSON/Dictionary/Array, DirAccess/FileAccess, SCons, doctest-based engine tests.

---

## File Structure

### New Tool Layer

- Create `editor/ai_component/tools/SCsub`
  - Compiles tool layer and subdirectories.
- Create `editor/ai_component/tools/ai_tool.h/.cpp`
  - Defines `AITool`, `AIToolResult`, schema helpers, and execution contract.
- Create `editor/ai_component/tools/ai_tool_call.h/.cpp`
  - Defines `AIToolCall`, `AIToolCallStatus`, serialization helpers.
- Create `editor/ai_component/tools/ai_tool_registry.h/.cpp`
  - Owns tool registration and schema export.
- Create `editor/ai_component/tools/ai_tool_permission.h/.cpp`
  - Centralizes `allow/ask/deny` policy decisions.
- Create `editor/ai_component/tools/project/SCsub`
  - Compiles project tools.
- Create `editor/ai_component/tools/project/ai_list_project_tool.h/.cpp`
  - Implements `project.list_tree`.
- Create `editor/ai_component/tools/project/ai_read_file_tool.h/.cpp`
  - Implements `project.read_file`.
- Create `editor/ai_component/tools/project/ai_search_project_tool.h/.cpp`
  - Implements `project.search_text`.
- Create `editor/ai_component/tools/editor/SCsub`
  - Compiles editor tools.
- Create `editor/ai_component/tools/editor/ai_get_editor_context_tool.h/.cpp`
  - Implements `editor.get_context`.

### Agent Layer

- Create `editor/ai_component/agent/ai_agent_profile.h/.cpp`
  - Defines `Plan` and `Build` profile ids and permission defaults.
- Later create `editor/ai_component/agent/ai_agent_runtime.h/.cpp`
  - Runs the tool-capable request loop.
- Modify `editor/ai_component/agent/ai_agent_types.h`
  - Add tool-call/tool-result message roles.
- Modify `editor/ai_component/agent/ai_agent_message.h/.cpp`
  - Preserve metadata for tool messages.
- Modify `editor/ai_component/agent/SCsub`
  - Include new agent files.

### Provider Layer

- Create `editor/ai_component/providers/ai_provider_features.h`
  - Defines provider capability flags.
- Modify `editor/ai_component/providers/ai_agent_provider.h/.cpp`
  - Add feature reporting and structured request shape.
- Modify `editor/ai_component/providers/ai_openai_compatible_provider.h/.cpp`
  - Add tool schema serialization and streamed tool-call parsing.

### Storage/UI

- Modify `editor/ai_component/storage/ai_conversation_serializer.h/.cpp`
  - Persist tool metadata.
- Modify `editor/ai_component/ui/ai_agent_dock.h/.cpp`
  - Add profile selector and route profile to session.
- Modify `editor/ai_component/ui/ai_message_list.h/.cpp`
  - Render tool-call/tool-result messages.
- Create `editor/ai_component/ui/ai_tool_call_view.h/.cpp`
  - Displays tool name, arguments summary, status, and result preview.

### Tests

- Create `tests/editor/test_ai_agent_tools.cpp`
  - Tool registry, permission policy, read-only tools.
- Create `tests/editor/test_ai_agent_runtime.cpp`
  - Runtime loop with fake provider after provider contract exists.
- Modify test force-link registration if a new test file needs explicit linking.

## Task 1: Add Tool Core Types

**Files:**

- Create: `editor/ai_component/tools/SCsub`
- Create: `editor/ai_component/tools/ai_tool.h`
- Create: `editor/ai_component/tools/ai_tool.cpp`
- Create: `editor/ai_component/tools/ai_tool_call.h`
- Create: `editor/ai_component/tools/ai_tool_call.cpp`
- Create: `editor/ai_component/tools/ai_tool_registry.h`
- Create: `editor/ai_component/tools/ai_tool_registry.cpp`
- Modify: `editor/ai_component/SCsub`
- Test: `tests/editor/test_ai_agent_tools.cpp`

- [ ] **Step 1: Write failing tests for registry and tool-call serialization**

Add tests that expect:

- a registry can register one tool and find it by name.
- duplicate registration fails or is ignored predictably.
- `get_tool_schemas()` returns one schema dictionary with `type = "function"`.
- `AIToolCall::to_dict()` and `from_dict()` preserve id, name, status, and arguments.

- [ ] **Step 2: Run red build**

Run:

```powershell
scons platform=windows tests=yes target=editor
```

Expected:

- FAIL because the new tool classes do not exist yet.

- [ ] **Step 3: Implement minimal tool core**

Implement:

- `AIToolResult`
- `AITool`
- `AIToolCall`
- `AIToolRegistry`

Keep the API small:

```cpp
class AITool : public RefCounted {
	GDCLASS(AITool, RefCounted);

public:
	virtual String get_name() const = 0;
	virtual String get_description() const = 0;
	virtual Dictionary get_parameters_schema() const = 0;
	virtual AIToolResult execute(const Dictionary &p_arguments) = 0;
	Dictionary get_openai_schema() const;
};
```

- [ ] **Step 4: Include tools in SCons**

Add `SConscript("tools/SCsub")` to `editor/ai_component/SCsub`.

- [ ] **Step 5: Run green tests**

Run:

```powershell
scons platform=windows tests=yes target=editor
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[AI]*"
```

Expected:

- AI tests pass.

- [ ] **Step 6: Commit**

```powershell
git add editor/ai_component/SCsub editor/ai_component/tools tests/editor/test_ai_agent_tools.cpp
git commit -m "feat: add ai tool core"
```

## Task 2: Add Permission Policy And Agent Profiles

**Files:**

- Create: `editor/ai_component/tools/ai_tool_permission.h`
- Create: `editor/ai_component/tools/ai_tool_permission.cpp`
- Create: `editor/ai_component/agent/ai_agent_profile.h`
- Create: `editor/ai_component/agent/ai_agent_profile.cpp`
- Modify: `editor/ai_component/tools/SCsub`
- Modify: `editor/ai_component/agent/SCsub`
- Test: `tests/editor/test_ai_agent_tools.cpp`

- [ ] **Step 1: Write failing permission tests**

Add tests:

- `Plan` allows `project.list_tree`, `project.read_file`, `project.search_text`, and `editor.get_context`.
- `Plan` denies unknown tools.
- `Build` currently has the same read-only permissions.
- policy result serializes to `allow`, `ask`, or `deny`.

- [ ] **Step 2: Run red build**

Run:

```powershell
scons platform=windows tests=yes target=editor
```

Expected:

- FAIL because permission/profile classes do not exist.

- [ ] **Step 3: Implement permission/profile classes**

Keep this data-driven:

- `AIAgentProfile::get_plan_profile()`
- `AIAgentProfile::get_build_profile()`
- `AIToolPermissionPolicy::evaluate(profile, tool_name, arguments)`

Do not hard-code policy checks inside tools.

- [ ] **Step 4: Run green tests and commit**

Run:

```powershell
scons platform=windows tests=yes target=editor
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[AI]*"
```

Commit:

```powershell
git add editor/ai_component/tools editor/ai_component/agent tests/editor/test_ai_agent_tools.cpp
git commit -m "feat: add ai agent profiles and tool permissions"
```

## Task 3: Add Read-Only Project Tools

**Files:**

- Create: `editor/ai_component/tools/project/SCsub`
- Create: `editor/ai_component/tools/project/ai_list_project_tool.h/.cpp`
- Create: `editor/ai_component/tools/project/ai_read_file_tool.h/.cpp`
- Create: `editor/ai_component/tools/project/ai_search_project_tool.h/.cpp`
- Modify: `editor/ai_component/tools/SCsub`
- Test: `tests/editor/test_ai_agent_tools.cpp`

- [ ] **Step 1: Write failing tests for safe path and extension behavior**

Test:

- `project.read_file` rejects non-`res://` paths.
- `project.read_file` rejects paths containing `..`.
- `project.read_file` rejects unsupported extensions.
- `project.list_tree` returns a structured result for `res://`.
- `project.search_text` returns bounded line previews.

- [ ] **Step 2: Run red build**

Run:

```powershell
scons platform=windows tests=yes target=editor
```

Expected:

- FAIL because read-only tools do not exist.

- [ ] **Step 3: Implement read-only tools**

Implementation rules:

- Use `DirAccess` and `FileAccess`.
- Never use OS absolute paths.
- Never read outside `res://`.
- Keep max output limits inside each tool.
- Return errors as `AIToolResult`, not `ERR_FAIL_*` unless constructor misuse happens.

- [ ] **Step 4: Run green tests and commit**

Run:

```powershell
scons platform=windows tests=yes target=editor
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[AI]*"
```

Commit:

```powershell
git add editor/ai_component/tools tests/editor/test_ai_agent_tools.cpp
git commit -m "feat: add read-only ai project tools"
```

## Task 4: Add Editor Context Tool

**Files:**

- Create: `editor/ai_component/tools/editor/SCsub`
- Create: `editor/ai_component/tools/editor/ai_get_editor_context_tool.h/.cpp`
- Modify: `editor/ai_component/tools/SCsub`
- Test: `tests/editor/test_ai_agent_tools.cpp`

- [ ] **Step 1: Write failing test**

Test:

- `editor.get_context` returns a dictionary-like text result.
- result does not include API keys.
- result includes only safe metadata.

- [ ] **Step 2: Implement tool**

Use existing safe editor APIs only. If an editor value is not readily available without broad dependencies, leave it out.

- [ ] **Step 3: Run tests and commit**

Run:

```powershell
scons platform=windows tests=yes target=editor
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[AI]*"
```

Commit:

```powershell
git add editor/ai_component/tools tests/editor/test_ai_agent_tools.cpp
git commit -m "feat: add ai editor context tool"
```

## Task 5: Add Provider Feature And Tool Schema Plumbing

**Files:**

- Create: `editor/ai_component/providers/ai_provider_features.h`
- Modify: `editor/ai_component/providers/ai_agent_provider.h/.cpp`
- Modify: `editor/ai_component/providers/ai_openai_compatible_provider.h/.cpp`
- Test: `tests/editor/test_ai_agent_tools.cpp`

- [ ] **Step 1: Write failing schema serialization tests**

Test:

- tool registry schemas are accepted by provider request-building helpers.
- tool names preserve namespace-like names.
- fallback feature says tools unsupported unless enabled.

- [ ] **Step 2: Implement provider features**

Add a small feature struct. Avoid changing network behavior yet.

- [ ] **Step 3: Add request-body tool serialization helper**

Make this testable without network:

- input: messages and tool schemas.
- output: JSON body containing `tools`.

- [ ] **Step 4: Run tests and commit**

Run:

```powershell
scons platform=windows tests=yes target=editor
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[AI]*"
```

Commit:

```powershell
git add editor/ai_component/providers tests/editor/test_ai_agent_tools.cpp
git commit -m "feat: add ai provider tool schema plumbing"
```

## Task 6: Add Runtime Loop With Fake Provider

**Files:**

- Create: `editor/ai_component/agent/ai_agent_runtime.h/.cpp`
- Modify: `editor/ai_component/agent/SCsub`
- Test: `tests/editor/test_ai_agent_runtime.cpp`

- [ ] **Step 1: Write fake provider tests**

Use a fake provider or test-only helper to simulate:

- assistant asks for `project.list_tree`.
- runtime executes tool.
- runtime appends tool result.
- provider returns final assistant message.

- [ ] **Step 2: Implement minimal runtime loop**

Do not wire UI yet. Keep the runtime independent:

- input: messages, profile, provider, registry.
- output: emitted message events.
- limits: max 6 provider turns, max 20 tool calls.

- [ ] **Step 3: Run tests and commit**

Run:

```powershell
scons platform=windows tests=yes target=editor
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[AI]*"
```

Commit:

```powershell
git add editor/ai_component/agent tests/editor/test_ai_agent_runtime.cpp
git commit -m "feat: add ai agent runtime loop"
```

## Task 7: Integrate Runtime Into Session And Storage

**Files:**

- Modify: `editor/ai_component/agent/ai_agent_session.h/.cpp`
- Modify: `editor/ai_component/agent/ai_agent_message.h/.cpp`
- Modify: `editor/ai_component/storage/ai_conversation_serializer.h/.cpp`
- Test: `tests/editor/test_ai_agent_runtime.cpp`

- [ ] **Step 1: Write failing persistence tests**

Test:

- tool-call messages survive serializer round trip.
- tool-result messages survive serializer round trip.
- normal existing sessions still load.

- [ ] **Step 2: Extend message roles and metadata**

Add:

- `AI_AGENT_ROLE_TOOL_CALL`
- `AI_AGENT_ROLE_TOOL_RESULT`

Keep backward compatibility with old message dictionaries.

- [ ] **Step 3: Wire session to runtime behind feature flag/availability**

If provider tools are unavailable, keep current MVP path.

- [ ] **Step 4: Run tests and commit**

Run:

```powershell
scons platform=windows tests=yes target=editor
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[AI]*"
```

Commit:

```powershell
git add editor/ai_component/agent editor/ai_component/storage tests/editor/test_ai_agent_runtime.cpp
git commit -m "feat: persist ai tool events"
```

## Task 8: Render Tool Calls In UI

**Files:**

- Create: `editor/ai_component/ui/ai_tool_call_view.h/.cpp`
- Modify: `editor/ai_component/ui/SCsub`
- Modify: `editor/ai_component/ui/ai_message_list.h/.cpp`
- Modify: `editor/ai_component/ui/ai_message_bubble.cpp`
- Modify: `editor/ai_component/ui/ai_agent_dock.h/.cpp`
- Test: existing AI tests plus manual editor check.

- [ ] **Step 1: Add UI test coverage where practical**

At minimum, test message dictionary roles map to the expected UI branch.

- [ ] **Step 2: Implement tool-call view**

Keep it compact:

- name
- status
- arguments summary
- collapsed content preview

- [ ] **Step 3: Add profile selector**

Add `Plan` and `Build` selection without changing write permissions.

- [ ] **Step 4: Run tests and build normal editor**

Run:

```powershell
scons platform=windows tests=yes target=editor
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[AI]*"
scons platform=windows target=editor
```

- [ ] **Step 5: Commit**

```powershell
git add editor/ai_component/ui tests/editor
git commit -m "feat: render ai tool calls"
```

## Task 9: Manual Verification

- [ ] Open project manager.
- [ ] Open a Godot project.
- [ ] Open AI dock.
- [ ] Select `Plan`.
- [ ] Send a normal chat prompt with a provider that does not support tools; verify fallback chat still works.
- [ ] With a provider that supports tools, ask "List the project files."
- [ ] Verify a `project.list_tree` tool card appears.
- [ ] Ask about a script file.
- [ ] Verify `project.read_file` is called and file content is not invented.
- [ ] Confirm no project file changed.
- [ ] Reload editor and confirm history shows tool events.

## Commit Strategy

Commit after each task. Keep commits focused:

- tool core
- permissions/profiles
- read-only project tools
- editor context tool
- provider schema plumbing
- runtime loop
- persistence
- UI rendering

Do not bundle write tools, command tools, scene mutation, MCP, or unrelated UI redesign into this milestone.
