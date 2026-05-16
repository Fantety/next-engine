# Godot Editor AI Agent Runtime Architecture Design

## Purpose

This document defines the next architecture step for the NEXT Engine editor AI feature. The current implementation is a stable chat MVP: it supports model settings, streaming chat, session history, and a small read-only context bundle. The next step is to evolve it into a real coding-agent runtime without disturbing Godot's native editor/runtime behavior.

The design follows the architecture pattern used by mature agent coding tools such as OpenCode, Cline, Continue, and Aider:

- Agent profiles with explicit tool permissions.
- A tool registry instead of ad-hoc model output parsing.
- Context providers that expose project state through bounded, auditable interfaces.
- A permission gate before any sensitive operation.
- Session history that records user messages, assistant messages, tool calls, and tool results.

The main reference model is OpenCode's `agent + tools + permission` structure. Cline contributes the human-in-the-loop approval model. Continue contributes provider-style context gathering. Aider contributes the idea of using compressed project structure first, then reading specific files on demand.

## Current State

The current code already has useful boundaries:

- `editor/ai_component/agent`
  - `AIAgentSession`
  - `AIAgentRunner`
  - `AIAgentMessage`
- `editor/ai_component/providers`
  - `AIAgentProvider`
  - `AIOpenAICompatibleProvider`
  - `AIModelSettings`
- `editor/ai_component/context`
  - `AIProjectTreeContextProvider`
  - `AIEditorContextProvider`
  - `AIFileContextProvider`
- `editor/ai_component/storage`
  - `AIConversationStore`
- `editor/ai_component/ui`
  - `AIAgentDock`
  - `AIComposer`
  - `AIMessageList`
  - `AIAgentSettingsDialog`

The current request flow is single-turn from the agent runtime point of view:

1. User sends a message.
2. `AIAgentSession` collects a small read-only context bundle.
3. `AIAgentRunner` builds OpenAI-compatible chat messages.
4. `AIOpenAICompatibleProvider` streams the assistant response.
5. The assistant response is displayed and saved.

There is no actual tool-call loop yet. The LLM cannot inspect arbitrary project files on demand, cannot modify project files, cannot run Godot commands, and cannot call MCP. This is the correct stability baseline, but it is not yet a coding-agent architecture.

## Goals

The first Agent Runtime milestone should provide:

- A reusable agent loop that can call tools, observe results, and continue reasoning.
- A typed read-only tool system.
- A permission policy model, even before write tools exist.
- Agent profiles such as `Plan` and `Build`.
- Tool-call and tool-result messages in session history.
- UI affordances for showing tool calls and results.
- Provider support for OpenAI-compatible tool calls where available.
- A fallback behavior when the selected provider does not support tool calls.

The milestone must remain safe:

- No MCP support.
- No write tools.
- No command execution tools.
- No direct scene mutation tools.
- No editor action execution tools.
- No Godot runtime class exposure.

## Non-Goals

This stage explicitly does not implement:

- MCP client or MCP server support.
- Local HTTP server support.
- File writing, patch application, deletion, rename, or move.
- Running shell commands.
- Running the project.
- Creating or modifying scenes, nodes, resources, or scripts.
- Automatic fixing of errors.
- Autonomous background agents.

These features can be added later once the read-only runtime, permission model, and UI event history are stable.

## Architecture Overview

The new structure should keep all AI business logic inside `editor/ai_component`.

```text
editor/ai_component/
  agent/
    ai_agent_profile.*
    ai_agent_runtime.*
    ai_agent_runner.*
    ai_agent_session.*
    ai_agent_message.*
    ai_agent_types.*
  context/
    ai_context_provider.*
    ai_editor_context_provider.*
    ai_project_tree_context_provider.*
    ai_file_context_provider.*
  providers/
    ai_agent_provider.*
    ai_openai_compatible_provider.*
    ai_provider_config.*
    ai_provider_features.*
    ai_sse_parser.*
  storage/
    ai_conversation_store.*
    ai_conversation_serializer.*
  tools/
    ai_tool.*
    ai_tool_registry.*
    ai_tool_permission.*
    ai_tool_call.*
    project/
      ai_list_project_tool.*
      ai_read_file_tool.*
      ai_search_project_tool.*
    editor/
      ai_get_editor_context_tool.*
  ui/
    ai_agent_dock.*
    ai_agent_settings_dialog.*
    ai_composer.*
    ai_message_bubble.*
    ai_message_list.*
    ai_tool_call_view.*
  prompts/
    agent_system_prompt.*
```

`AIAgentSession` remains the UI-facing conversation owner. `AIAgentRuntime` becomes the internal execution engine for a single user request. `AIAgentRunner` can either be replaced by `AIAgentRuntime` or reduced to a small adapter that starts runtime work.

## Agent Profiles

Agent profiles define model behavior and tool permissions. They are similar to OpenCode agents.

### Plan

Purpose:

- Explain the project.
- Inspect project context.
- Propose changes.
- Produce implementation plans.

Default permissions:

- `project.list_tree`: allow
- `project.read_file`: allow
- `project.search_text`: allow
- `editor.get_context`: allow
- all write tools: deny
- all command tools: deny

### Build

Purpose:

- Same read-only abilities as `Plan`.
- Prepares the future path for write tools and command tools.

First milestone permissions:

- Same as `Plan`.
- Write tools are not registered yet.

Future permissions:

- File edits: ask
- Scene edits: ask
- Commands: ask

`Build` exists in the UI now so the user-facing architecture is ready, but it should not silently gain write ability until a later milestone adds explicit approval flows.

## Tool System

Tools are native C++ editor-side components. The model asks for tool execution, but only the runtime executes tools.

## Tool Calling Strategy

The runtime should use a ReAct-style loop, but the provider protocol should use structured function/tool calling.

These are different layers:

- ReAct is the agent control loop: think, choose an action, observe a result, continue.
- Function/tool calling is the wire protocol: the model returns structured tool names and JSON arguments.

The project should not use a pure text ReAct protocol such as:

```text
Thought: ...
Action: read_file
Action Input: ...
Observation: ...
```

It should also not revive the previous XML-like protocol:

```text
<tool name="read_file">...</tool>
```

Those protocols are easy to parse incorrectly, hard to permission safely, and provider-dependent. They also make it too easy for normal assistant text to be mistaken for an executable action.

The target strategy is:

1. The runtime exposes tool schemas through `AIToolRegistry`.
2. The provider adapter sends those schemas using the provider's native tool/function-calling API when available.
3. The model returns structured tool calls with ids, names, and JSON arguments.
4. The runtime validates tool names and arguments.
5. `AIToolPermissionPolicy` decides `allow`, `ask`, or `deny`.
6. The runtime executes allowed tools and records tool results.
7. Tool results are sent back to the model as structured tool messages.
8. The loop continues until the model returns final assistant text or a runtime limit is reached.

This mirrors the practical architecture of modern coding agents:

- OpenCode-style structure: agent profiles, tool registry, and per-tool permissions.
- Cline-style safety: human approval before sensitive tools.
- Continue-style context: project state exposed by context/tool providers rather than a monolithic prompt.
- Aider-style inspection: start from compact project structure, then inspect concrete files on demand.

Fallback behavior is allowed but must be non-executable:

- If the provider does not support native tool calls, the request falls back to the current read-only chat behavior.
- The assistant can suggest files to inspect, but the runtime must not parse free-form text into tool calls.
- No hidden "best effort" text parser should execute tools.

This decision keeps the agent loop extensible while making tool execution auditable and permissionable.

### Core Types

`AITool`

- Abstract base class.
- Owns name, description, input schema, permission category, and execution method.
- Returns `AIToolResult`.

`AIToolRegistry`

- Registers built-in tools.
- Looks up tools by stable name.
- Exports tool schemas to providers.

`AIToolCall`

- Stores `id`, `tool_name`, raw arguments JSON, parsed arguments, status, timestamps.

`AIToolResult`

- Stores `tool_call_id`, `content`, `error`, `truncated`, metadata.

`AIToolPermissionPolicy`

- Evaluates whether a tool call is `allow`, `ask`, or `deny`.
- First milestone can implement only `allow` and `deny`; `ask` can return a pending state for future UI.

### Tool Naming

Use stable names with namespaces:

- `project.list_tree`
- `project.read_file`
- `project.search_text`
- `editor.get_context`

Avoid provider-specific names. The provider adapter maps these names to OpenAI-compatible tool schemas.

## First Read-Only Tools

### `project.list_tree`

Input:

```json
{
  "path": "res://",
  "max_depth": 4,
  "max_entries": 400
}
```

Behavior:

- Lists directories and files under `res://`.
- Rejects paths outside `res://`.
- Rejects `..`.
- Applies entry and character limits.
- Returns a truncated marker when limits are reached.

### `project.read_file`

Input:

```json
{
  "path": "res://scripts/player.gd",
  "max_bytes": 65536
}
```

Behavior:

- Reads only allowlisted text extensions.
- Rejects files outside `res://`.
- Rejects `..`.
- Applies max byte limits.
- Returns a truncated marker when content is longer than the limit.

Allowed first extensions:

- `.gd`
- `.cs`
- `.tscn`
- `.tres`
- `.md`
- `.txt`
- `.json`
- `.cfg`
- `.shader`
- `.gdshader`

### `project.search_text`

Input:

```json
{
  "query": "class_name Player",
  "path": "res://",
  "max_results": 50
}
```

Behavior:

- Searches allowlisted text files under `res://`.
- First implementation can use literal substring search.
- Regex can be added later.
- Returns path, line number, and line preview.

### `editor.get_context`

Input:

```json
{}
```

Behavior:

- Returns safe editor metadata.
- Current milestone should include only information that is already safe:
  - project path
  - edited scene path when available
  - selected model id
  - agent profile

It should not expose secrets, API keys, full editor settings, or OS paths outside the project unless required and reviewed.

## Provider Tool Calling

The preferred protocol is OpenAI-compatible tool calling:

- Request includes `tools`.
- Assistant stream may include `tool_calls`.
- Runtime accumulates tool call ids, names, and argument deltas.
- Runtime executes completed tool calls.
- Runtime sends tool results back as `tool` role messages.
- Runtime continues until the assistant returns normal final content or max iterations is reached.

Provider support must be capability-based:

```cpp
struct AIProviderFeatures {
	bool supports_streaming = true;
	bool supports_tools = false;
};
```

`AIOpenAICompatibleProvider` should declare `supports_tools` based on config or provider preset. Not every OpenAI-compatible endpoint supports tools reliably.

Fallback behavior:

- If tools are not supported, the runtime sends the current small context bundle and behaves like the current chat MVP.
- The system prompt must tell the model that tools are unavailable in this request.
- The UI should not show tool-call affordances for that request.

Do not reintroduce the old XML-like `<tool>` parsing protocol. It is fragile, provider-specific, and hard to permission safely.

## Agent Runtime Loop

For one user request:

1. Add user message.
2. Create an assistant working message.
3. Build initial provider request:
   - system prompt
   - agent profile prompt
   - compact context
   - previous user/assistant/tool messages
   - tool schemas when supported
4. Stream provider response.
5. If final assistant text arrives, update assistant message and finish.
6. If tool calls arrive:
   - validate each tool name
   - parse arguments
   - check permission policy
   - execute allowed tools
   - record tool call and tool result messages
   - send tool results back to provider
7. Repeat until final answer, cancellation, error, or max iterations.

Default max iterations:

- 6 provider turns per user request.
- 20 tool calls per user request.

These limits prevent infinite tool loops.

## Context Strategy

Do not send the full project tree and arbitrary file contents on every request.

Initial context should be compact:

- editor context
- project root summary
- maybe shallow tree

The model should call tools to inspect more:

- list tree to locate files
- read file to inspect specific files
- search text to find symbols or strings

This keeps prompts small and makes the model's inspection path auditable in the UI.

## Session Storage

Session JSON should evolve from plain chat messages to event history.

Message roles:

- `user`
- `assistant`
- `tool_call`
- `tool_result`
- `error`

Each message should support metadata:

```json
{
  "role": "tool_call",
  "content": "project.read_file",
  "metadata": {
    "tool_call_id": "call_123",
    "tool_name": "project.read_file",
    "arguments": {
      "path": "res://scripts/player.gd"
    },
    "status": "completed"
  },
  "created_at": 1778947200
}
```

This makes history reproducible and debuggable.

## UI Design

The existing dock can evolve without a large visual rewrite.

Required first milestone UI additions:

- Agent profile selector: `Plan`, `Build`.
- Tool-call cards in the message list.
- Tool result collapsed preview.
- Clear label when tools are unavailable for the selected provider.
- Session list continues to work.

Tool cards should show:

- tool name
- arguments summary
- status: running, completed, denied, failed
- truncated marker when applicable

Do not add write confirmation UI until write tools are implemented.

## Permission Model

Permission decisions should be centralized.

Policy inputs:

- agent profile
- tool name
- tool category
- tool arguments
- provider request id or session id

Policy outputs:

- `allow`
- `ask`
- `deny`

First milestone behavior:

- Read-only project/editor tools: `allow`.
- Unknown tools: `deny`.
- Write and command tools: not registered. If encountered, `deny`.

Later behavior:

- `ask` creates a pending tool-call message and waits for user approval.
- Approval result is stored in session history.

## Error Handling

The runtime must convert all failures into structured error messages:

- provider does not support tools
- provider tool-call JSON is invalid
- tool name is unknown
- arguments fail validation
- permission denied
- file path rejected
- file too large
- search limit reached
- max tool iterations reached
- user cancelled

The assistant should receive tool errors as tool results only when doing so is useful. User-facing errors should be clear and not expose secrets.

## Files To Add

### Agent

- `editor/ai_component/agent/ai_agent_profile.h`
- `editor/ai_component/agent/ai_agent_profile.cpp`
- `editor/ai_component/agent/ai_agent_runtime.h`
- `editor/ai_component/agent/ai_agent_runtime.cpp`

### Provider

- `editor/ai_component/providers/ai_provider_features.h`

### Tools

- `editor/ai_component/tools/SCsub`
- `editor/ai_component/tools/ai_tool.h`
- `editor/ai_component/tools/ai_tool.cpp`
- `editor/ai_component/tools/ai_tool_call.h`
- `editor/ai_component/tools/ai_tool_call.cpp`
- `editor/ai_component/tools/ai_tool_registry.h`
- `editor/ai_component/tools/ai_tool_registry.cpp`
- `editor/ai_component/tools/ai_tool_permission.h`
- `editor/ai_component/tools/ai_tool_permission.cpp`
- `editor/ai_component/tools/project/ai_list_project_tool.h`
- `editor/ai_component/tools/project/ai_list_project_tool.cpp`
- `editor/ai_component/tools/project/ai_read_file_tool.h`
- `editor/ai_component/tools/project/ai_read_file_tool.cpp`
- `editor/ai_component/tools/project/ai_search_project_tool.h`
- `editor/ai_component/tools/project/ai_search_project_tool.cpp`
- `editor/ai_component/tools/editor/ai_get_editor_context_tool.h`
- `editor/ai_component/tools/editor/ai_get_editor_context_tool.cpp`

### UI

- `editor/ai_component/ui/ai_tool_call_view.h`
- `editor/ai_component/ui/ai_tool_call_view.cpp`

### Tests

- `tests/editor/test_ai_agent_tools.cpp`
- `tests/editor/test_ai_agent_runtime.cpp`

## Files To Modify

- `editor/ai_component/SCsub`
  - include `tools/SCsub`.
- `editor/ai_component/agent/SCsub`
  - include profile/runtime sources.
- `editor/ai_component/agent/ai_agent_message.*`
  - add tool-call and tool-result roles or metadata helpers.
- `editor/ai_component/agent/ai_agent_session.*`
  - own selected agent profile.
  - call `AIAgentRuntime`.
  - save tool events.
- `editor/ai_component/agent/ai_agent_runner.*`
  - replace with runtime or reduce to adapter.
- `editor/ai_component/providers/ai_agent_provider.*`
  - expose provider features.
  - accept optional tool schemas.
  - emit structured assistant/tool-call events.
- `editor/ai_component/providers/ai_openai_compatible_provider.*`
  - serialize OpenAI-compatible tools.
  - parse streamed `tool_calls`.
  - send `tool` role result messages.
- `editor/ai_component/storage/ai_conversation_serializer.*`
  - persist tool-call and tool-result metadata.
- `editor/ai_component/ui/ai_agent_dock.*`
  - add profile selector.
  - render tool messages.
- `editor/ai_component/ui/ai_composer.*`
  - expose selected profile or let dock own profile selection.
- `editor/ai_component/ui/ai_message_list.*`
  - support tool-call view rows.
- `editor/ai_component/ui/ai_agent_settings_dialog.*`
  - keep MCP/Skills/Rules placeholders.
  - optionally add read-only tool availability info.
- `tests/editor/test_ai_model_settings.cpp`
  - keep current tests; add no unrelated assertions.

## Implementation Order

1. Add tool data structures and tests.
2. Add read-only tools and tests.
3. Add agent profile and permission policy tests.
4. Add provider feature plumbing without changing runtime behavior.
5. Add OpenAI-compatible tool schema serialization and parser tests.
6. Add `AIAgentRuntime` loop with a fake provider test.
7. Integrate runtime into `AIAgentSession`.
8. Persist tool-call and tool-result messages.
9. Render tool messages in UI.
10. Build and run focused AI tests.
11. Build normal Windows editor.

## Verification

Minimum verification before merging implementation:

```powershell
scons platform=windows tests=yes target=editor
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[AI]*"
scons platform=windows target=editor
```

Manual checks:

- Open project manager.
- Open a Godot project.
- Open AI dock.
- Select `Plan`.
- Ask: "List the project files."
- Confirm the model calls `project.list_tree` when provider supports tools.
- Ask about a specific script.
- Confirm the model calls `project.read_file` and does not invent file contents.
- Confirm no file is modified.
- Confirm history reload shows tool calls and tool results.

## Later Milestones

After the read-only runtime is stable:

1. Add `ask` permission UI.
2. Add patch preview and file write tools.
3. Add scene inspection tools.
4. Add scene mutation tools with approval.
5. Add run/test tools with approval.
6. Add rule configuration.
7. Add skill configuration.
8. Reconsider MCP as a separate optional integration layer only after the native tool model is stable.

## Decision

Proceed with the read-only Agent Runtime milestone first. This gives the LLM a real, auditable way to inspect project contents while keeping all mutating behavior out of scope. It also creates the correct foundation for future write tools, rules, skills, and optional MCP without polluting Godot core or runtime APIs.
