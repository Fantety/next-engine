# Findings & Decisions

## Requirements
- Understand the current project with focus on `editor/ai_component`.
- Use `docs` as architecture and behavior reference.
- Build a richer Markdown viewer for AI messages, replacing the current RichTextLabel-based implementation with a real component-style renderer.

## Research Findings
- Repository root is a Godot-derived C++ engine/editor tree with `core`, `scene`, `editor`, `modules`, `servers`, tests, SCons build files, and generated Windows editor binaries.
- Relevant docs found:
  - `docs/ai_agent_architecture.md`
  - `docs/ai_agent_metadata.md`
  - `docs/ai_mcp_flow.md`
  - `docs/ai_script_tooling_design.md`
  - `docs/NexusServer.openapi.json`
  - `docs/sign.md`
- `editor/ai_component` is split into source submodules: `agent`, `context`, `planning`, `prompts`, `providers`, `review`, `rules`, `skills`, `storage`, `tools`, and `ui`.
- `tools` has generic tool interfaces plus `project` file/search tools and `editor` scene/script/shader tools.
- `docs/ai_agent_architecture.md` says the feature is an in-editor AI Coding Agent, not an external script runner. Main boundaries are UI, session, runtime, provider, context, tools, review/diff, storage, MCP, skills, rules, and planning.
- Request flow in docs: `AIComposer` emits send, `AIAgentDock` configures provider/profile, `AIAgentSession` persists user/assistant placeholder messages and collects context, `AIAgentRuntimeRunner` runs `AIAgentRuntime` on a thread, runtime calls an OpenAI-compatible provider client, streams assistant text, handles tool calls through permission policy and `AIToolRegistry`, then session applies results and persists the conversation.
- MCP docs define MCP as a dynamic tool source managed by `AIMCPService`, with discovery off the UI thread via `AIMCPToolDiscoveryRunner`. Session consumes service tool snapshots; UI observes service status.
- MCP supports `stdio`, `streamable_http`, and legacy `sse`. Discovery failures do not expose failed server tools to the model.
- MCP tool schemas enter the model request through OpenAI-compatible `tools`, not by being pasted into the system prompt. MCP tools default to ask-gated permissions.
- `docs/ai_script_tooling_design.md` centralizes script mutations in `AIScriptEditingService`; individual tool classes validate JSON arguments and delegate. `script.delete` is always approval-gated.
- `docs/ai_agent_metadata.md` treats message/runtime/context/tool metadata as extensible fields to avoid churn in persisted conversation schema.
- `docs/sign.md` and `docs/NexusServer.openapi.json` look adjacent to server/API integration rather than core `editor/ai_component` architecture.
- `editor/ai_component/SCsub` builds submodules in this order: `agent`, `providers`, `context`, `planning`, `review`, `rules`, `storage`, `prompts`, `skills`, `tools`, `ui`.
- `editor/register_editor_types.cpp` registers AI Agent, MCP, context, storage, tool execution, editor services, and UI classes. `editor/editor_node.cpp` creates `AIAgentSettingsDialog` and `AIAgentDock`.
- `AIAgentSession` constructs and owns the runtime, runner, OpenAI-compatible runtime client, tool registry, conversation store, project/editor/best-practices/rules/skill context providers, and active profile. It connects MCP `tools_changed` and runtime runner progress signals.
- `AIAgentSession::send_user_message()` strips input, persists a user message, creates an assistant placeholder, and calls `_start_runtime_turn()`.
- `_start_runtime_turn()` collects context, removes an empty assistant placeholder from provider request messages, sets state to streaming, maps runtime progress messages back to local messages, and starts `AIAgentRuntimeRunner`.
- `AIAgentRuntimeRunner` runs `AIAgentRuntime::run()` on a `Thread`, forwards progress via deferred `runtime_message_added/updated`, stores the last result, and emits `runtime_finished`.
- `AIAgentRuntime` builds tool schemas from `profile.allowed_tools` plus `profile.ask_tools`, calls `AIContextManager::build_messages()`, then uses `AIAgentRuntimeClient::complete_streaming()` for each provider turn.
- Tool calls are handled in runtime with `AIToolPermissionPolicy`: ask returns a successful result with `pending_approval`, deny appends a denied tool message, allow executes through `AIToolExecutionContext` and appends tool result messages.
- Session approval path executes the exact pending tool call, appends the tool result, clears approval state, and starts another runtime turn. Reject path appends a denied tool message and also resumes runtime.
- `AIContextManager` creates system prompt + read-only context system message + trimmed history. It strips local `usage` and `estimated_context_usage` before provider messages, truncates tool result content separately, and preserves assistant tool-call/tool-result grouping when trimming.
- Profiles are static in `AIAgentProfile`: `plan` and `build` are read-oriented; `write` adds scene/script/shader mutation plus project folder creation; `review` currently uses write permissions but services check `AIToolExecutionContext::is_review_mode()` to record review changes. `script.delete` is ask-gated.
- `AIProviderConfig` and `AIModelSettings` carry model/provider URL/key plus context budgets, provider turn limit, tool call limit, output token limit, and timeout.
- `AIOpenAICompatibleRuntimeClient` maps internal tool names like `project.read_file` to provider-safe names, builds chat messages, calls transport, parses streaming/non-streaming responses, and maps provider tool names back to internal names.
- `AIMCPService` is the single MCP discovery entry point. It coalesces refresh requests, stores discovered tool snapshots, emits `status_changed`/`tools_changed`, and registers cached MCP tools into an `AIToolRegistry`.
- `AIMCPToolDiscoveryRunner` performs blocking MCP discovery on a thread and returns results with a request id. `AIMCPToolDiscovery` validates servers, lists tools, and deduplicates discovered agent tool names.
- `AIChangeSetStore` maintains pending/kept/reverted review change sets and can merge repeated pending changes for the same project/session/path. `AIDiffService` builds text change dictionaries and unified diff text with added/removed counts.
- `AIConversationStore` persists conversations under `user://ai_agent/projects/<scope>/conversations`; scope defaults to an MD5 of the project resource path, so chat history is project-isolated.
- `AIAgentDock` owns the visible dock flow: creates `AIComposer`, message list, change review panel, MCP status UI, tool approval dialog, and a single `AIAgentSession`. It handles send/cancel, state changes, token updates, MCP status refresh, and tool approvals.
- Context/rules/skills/planning are independent submodules: session collects their context providers; skills expose indexed prompt/context entries and a read-only activation tool; planning has `agent.manage_plan` and `AIPlanPanel`.
- Scene editing is centralized in `AISceneEditingService`, which performs main-thread editor operations for create/add/delete/rename/move/set/list/save/open scene actions.
- Script editing is centralized in `AIScriptEditingService`, which normalizes project paths, parses GDScript with `GDScriptParser`, supports function-level patching, and records review-mode text changes. Shader edits use `AIShaderEditingService`, create/update `.gdshader`, bind `ShaderMaterial`, save the scene, and record review changes when in review mode.
- Test references:
  - `tests/editor/test_ai_agent_runtime.cpp` covers context manager budgeting, runtime tool loops, streaming progress, approval pause, token usage aggregation, session application/persistence, OpenAI-compatible codec/client, and runtime runner threading.
  - `tests/editor/test_ai_agent_tools.cpp` covers tool schemas/permissions, MCP protocol/discovery/status/client utilities, planning, rules, review diff/change sets, project tool boundaries, scene tool argument validation, and codec tool schema serialization.
  - `tests/editor/test_ai_model_settings.cpp` covers model profile settings, MCP settings, skill/rule settings UI data, and settings pages.
- Build wiring: `editor/SCsub` includes `ai_component/SCsub`; each AI submodule `SCsub` contributes `*.cpp` to `env.editor_sources`, with `context/SCsub` also generating `best_practices.gen.h` from `agent/best_practices.md`.
- Current Markdown UI:
  - `AIMarkdownLabel` is a `VBoxContainer` that owns one `RichTextLabel`.
  - `AIMarkdownRenderer` traverses `core/markdown/MarkdownNode` and pushes formatting into `RichTextLabel`.
  - Tables are manually recognized in `ai_markdown_label.cpp`, but still rendered through `RichTextLabel::push_table`.
  - Links use `RichTextLabel::push_meta`, but there is no visible standalone link component or explicit navigation handling in `AIMarkdownLabel`.
  - Images fall back to alt text or URL text; there is no image loading/display.
  - Code blocks are plain mono text in RichTextLabel; there is no code panel, syntax highlighter, copy button, language badge, or horizontal scroll container.
  - `AIMessageBubble` creates `AIMarkdownLabel` and uses it for assistant/user content, while tool bubbles use `add_text()` summaries/details.
  - Existing tests in `tests/editor/test_ai_model_settings.cpp` assert parsed text for markdown label, tables, parser metadata, and message bubble markdown.
- User clarified the new Markdown Viewer should not be scoped only to AI chat messages. It should be registered as a Godot UI node that can render Markdown text.
- User confirmed it should be a runtime-usable Godot UI node, not editor-only.
- Runtime registration path is `scene/register_scene_types.cpp`, where `Label`, `TextureRect`, `ScrollContainer`, `TextEdit`, `CodeEdit`, `CodeHighlighter`, and `RichTextLabel` are registered.
- Existing Markdown parser lives in `core/markdown` and exposes `MarkdownParser`/`MarkdownNode`, so a runtime `scene/gui/markdown_viewer.*` can consume it without depending on editor code.
- Candidate implementation shape: a `MarkdownViewer` runtime `Control`/`ScrollContainer` that rebuilds a tree of child Controls from the parsed Markdown AST, using `Label`/custom text blocks for paragraphs, `GridContainer`/containers for tables, read-only `CodeEdit` or custom code block controls for fenced code, `LinkButton` for links, and `TextureRect` for local/project images.
- User selected visual direction B: default should feel like a complete Markdown document surface with more opinionated spacing, image presentation/captions, polished tables/code blocks, and visible links, rather than a neutral bare component tree.
- Remote images are in scope for the first version. This implies asynchronous HTTP fetch, failure/loading state, size constraints, and some cache policy.
- User accepted recommended default: remote image auto-download is disabled by default and controlled by a property such as `remote_images_enabled`.
- User rejected the component-tree implementation path because existing controls would likely create unacceptable performance and memory overhead. The design should be a bottom-level self-drawn runtime `Control`, not a wrapper around RichTextLabel or a tree of Label/CodeEdit/TextureRect nodes.
- `core/markdown/MarkdownNode` currently exposes document, paragraph, heading, list/list item, text, inline code, code block, link, image, emphasis, strong, and block quote nodes. It does not expose table nodes, so pipe table support needs a viewer-side preprocessing/layout layer unless the parser is extended later.
- Runtime APIs available for implementation:
  - `scene/main/http_request.*` provides `HTTPRequest`, `request()`, `request_completed`, redirects, timeout/body size settings.
  - `core/io/image.h` supports `load_png_from_buffer`, `load_jpg_from_buffer`, and `load_webp_from_buffer`.
  - `ImageTexture::create_from_image()` converts decoded images to textures for `TextureRect`.
  - `AITextDiffViewer` already demonstrates read-only `CodeEdit` with syntax highlighter lookup for GDScript/GDShader.

## Technical Decisions
| Decision | Rationale |
|----------|-----------|
| Read docs before source internals | Docs should reveal intended architecture and vocabulary, then source can validate implementation. |
| Prioritize module boundaries and request flow | This creates useful context for later feature or bugfix work. |
| Implement new MarkdownViewer as a self-drawn runtime Control | Matches user preference for lower overhead and general Godot UI node availability. |
| Keep remote image auto-download disabled by default | Avoids surprise network traffic and gives projects explicit control over remote content. |

## Issues Encountered
| Issue | Resolution |
|-------|------------|
| Initial default `Get-Content` output for Chinese docs appeared garbled | Re-read with `-Encoding UTF8`; content is readable and docs are UTF-8. |
| One parallel command group reported a Windows sandbox spawn setup refresh error while other reads completed | Logged the anomaly and continued with focused follow-up reads. |
| PowerShell wildcard paths like `editor\ai_component\context\*.h` failed through `rg` | Use directory-level `rg` searches or explicit files on Windows in this environment. |
| One parallel read of `progress.md` hit Windows sandbox spawn setup refresh | Non-critical; current source context was read from other files. |

## Resources
- `editor/ai_component/`
- `docs/`
- `tests/editor/test_ai_agent_runtime.cpp`
- `tests/editor/test_ai_agent_tools.cpp`
- `tests/editor/test_ai_model_settings.cpp`

## Visual/Browser Findings
- No browser or visual content used.
