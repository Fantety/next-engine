# Findings & Decisions

## Requirements
- Understand `editor/agent_v1`.
- Understand `editor/agent_ui`.
- Rewrite the relevant README based on the current implementation.
- Avoid duplicating existing infrastructure or inventing undocumented behavior.

## Research Findings
- `editor/agent_v1` contains the backend/domain implementation: config, runtime, MCP, permissions, skills, tools, session, domain model, context, attachments, and UI adapter layers.
- `editor/agent_ui` contains the editor-facing UI: AI dock, composer, settings dialog/pages, change review panel, and reusable UI components such as message list, markdown renderer, todo list, attachment bar, status panel, and MCP/model/skill dialogs.
- No README exists directly under `editor/agent_v1` or `editor/agent_ui`; the likely README target is the repository root `README.md`, unless discovery shows a more specific doc target.
- The workspace already has unrelated modified/untracked files in `editor/agent_ui`, `editor/register_editor_types.cpp`, `editor/themes/editor_fonts.cpp`, `scene/theme`, `tests/editor/test_agent_v1.cpp`, icons, and fonts. Avoid overwriting those changes.
- Root `README.md` is the current NEXT Engine README and already covers AI Agent concepts, but its "Project Structure" section still describes the older `editor/ai_component` layout instead of the current `editor/agent_v1` and `editor/agent_ui` split.
- `editor/agent_v1/core/API.md` defines the provider-neutral core: cancellation, error/result types, operation context, HTTP/SSE transport, task runner/main-thread dispatcher, model request/stream event/sink, and scoped registration.
- `editor/agent_v1/domain/API.md` defines durable/recoverable session domain contracts: session/location/model/prompt/attachment/message/tool/context epoch value objects, append-only events, projections, history, token estimation, and attachment blob handling.
- `AIAgentService` resolves configured agents, session-bound agents, agent permission rules, and child task/subagent spawn constraints; `AIV1TaskTool` is implemented as a tool over session and agent services.
- `AIAgentDock` is an `EditorDock` wired to `AIAgentV1UIBridge`, with session selection, status panel, message list, todo panel, composer, token usage, tool approval dialog, requirement form dialog, and change review panel.
- `AIAgentV1UIBridge` is the singleton facade for editor UI: it owns/wires session, config, runtime registry, tool registry, permission, MCP, skill, agent, change-set, conversation adapter, and config adapter services.
- `AIAgentV1UIAdapter` projects sessions/messages/todos/run state/permission requests into UI dictionaries and applies project scope defaults for conversation operations.
- `AIAgentV1UIConfigAdapter` projects model profiles, agents, MCP servers, skills, rules, and marquee settings from config into UI-facing snapshots.
- `AISessionService` composes session store, input store, event store, projector, execution, prompt promoter, runners, compaction, context epoch services, config, runtime, permissions, tools, todos, attachments, skills, and agent service. It exposes create/prompt/reply_permission/interrupt/promote/update_todos operations.
- `AISessionRunner` drains session inputs into model turns: promotes prompts, builds context epochs and model requests, materializes tools, settles tool calls, runs the provider runtime, handles permission waits, compaction, skills, and continuation.
- `AILLMRuntime` is the provider-neutral runtime interface; implementations configure from dictionaries and stream `AIModelRequest` events to `AIStreamSink`.
- `AIV1ToolRegistry` registers built-in/scoped tools, materializes tool definitions per root directory and permission rules, validates/coerces arguments, and settles materialized calls into event-store-backed tool settlements.
- `AIConfigService` merges multiple config sources: global, project, opencode, account, managed, remote, and runtime override. It supports JSON read/write, comment stripping, migration, variable resolution, patching by scope, default provider/model lookup, provider config, and agent system prompts.
- `AIPermissionService` evaluates action/resource rules, creates pending requests for `ask`, stores saved approvals and decision audits, emits permission events, and exposes pending requests/replies.
- `AIV1MCPService` manages configured/fake MCP servers, discovers tools/resources/prompts, registers MCP tools through `AIV1ToolRegistry`, exposes statuses/summaries, reads resources, renders prompts, and calls tools. Tool/startup permissions have configurable defaults.
- `AIV1SkillService` scans skill source roots, parses manifests/frontmatter, selects skills by prompt/explicit names, creates context sources, reads declared resources, and optionally registers script tools with permission defaults.
- `AIOpenAICompatibleRuntime` converts provider-neutral model requests into OpenAI chat-completions requests, including text/media parts and tools, and streams responses over the shared HTTP/SSE infrastructure.
- `AISessionExecution` owns per-session run slots, wakes background drain tasks via `AITaskRunner`, merges wake sequence numbers, settles completed runs, and interrupts active runs with cancel tokens.
- `AITodoStore`/`AITodoService` keep session-scoped todo snapshots and emit/update todo events; `AIV1TodoWriteTool` exposes this to the agent.
- Built-in low-level tools include read file, write file, shell, and todo write; editor/project-specific tools live in subdirectories and are registered separately.
- `AIV1EditorTool` wraps editor-facing tools with a shared permission action/effect/reason boundary and a thread-local execution state that carries session/agent/tool-call/cancel/review flags.
- Project tools include list/search/read project, create folder, create markdown, attach multimodal file, and requirement form submission.
- Editor tools include current editor context, documentation search, scene describe/inspect/list properties/apply patch/delete node, run/stop scene, terminal error collection, script create/write/inspect/patch/bind/unbind/delete, shader create/edit/delete/apply/set parameters, and diff/change-set services.
- `AIChangeSetStore` stores pending file changes by project/session/tool call, merges repeated changes for a file, supports keep/revert, and refreshes the editor filesystem.
- `AIV1ProjectToolUtils` enforces `res://`-style allowed paths/extensions and refreshes the editor filesystem after project file changes.
- `AIComposer` contains model and mode selectors, send/cancel action, reference menu, file dialog, and clipboard/canvas/path reference attachment flows.
- `AIAgentSettingsDialog` is a navigation-based dialog with pages for Models, Next Marquee, MCP, Skills, and Rules.
- `AIChangeReviewPanel` lists pending change sets through `AIAgentV1UIBridge`, opens diff previews, keeps changes, and confirms reverts.
- `AIMessageList` groups UI messages into bubbles and manages bottom scrolling; `AIMessageBubble` renders markdown content, metadata/details, and attachments.
- `AIStatusPanel` is a popup button for MCP and Skill statuses.
- `AITodoListPanel` renders the session todo summary/current task/items and can collapse.
- `AIComposerInput` wraps rich prompt text with reference chips and returns send-ready attachments.
- Models settings manage project-scoped model profiles, including provider presets and custom OpenAI-compatible models, API keys, max output tokens, and request timeout.
- MCP settings manage servers with add/edit/remove, enable toggles, JSON import, status refresh, and transport-specific fields such as command/arguments/environment or URL/headers.
- Skills settings import skill folders, enable/disable/remove skills, and surface skill scan status.
- Rules settings edit action/resource/effect/reason rules, matching the permission service rule model.
- Next Marquee settings manage built-in and custom shader presets used by the AI UI.
- `AIMarkdownLabel` embeds the shared `MarkdownViewer`; older `AIMarkdownRenderer` also exists for RichTextLabel rendering paths.
- `AITextDiffViewer` shows multi-file change sets with side-by-side `CodeEdit`, synchronized scrolling, and GDScript/GDShader syntax highlighting.
- `AIReferenceResolver`, `AIReferenceTextEdit`, and `AIAttachmentBar` support inline `@`-style file/clipboard/canvas references and convert them to attachment dictionaries.
- Dialog components exist for model profiles, MCP servers, skills, and agent-requested requirement forms.
- `editor/SCsub` builds `agent_v1/core`, `config`, `agents`, `domain`, `permission`, `tools`, `mcp`, `skills`, `runtime`, `session`, `ui_adapter`, and then `agent_ui`.
- `editor/register_editor_types.cpp` registers the backend services, runtime/tools/session/domain classes, bridge/adapters, and UI widgets with ClassDB.
- `AIV1ToolRegistry::register_builtin_tools()` registers low-level tools and then calls `AIV1EditorTools::register_editor_tools()`.
- `AIV1EditorTools::register_editor_tools()` assigns categories and permission defaults: project/editor/scene/script/shader reads and many writes are `allow`; requirement form, script delete, and shader delete default to `ask`.
- README should avoid claiming all editing actions require manual approval; current code uses permission rules/default effects and a change review store for file changes.

## Technical Decisions
| Decision | Rationale |
|----------|-----------|
| Rewrite root README instead of adding a new module README | User asked for "README" and the only agent-related README currently present is the root README, which already describes NEXT Engine but is stale. |

## Issues Encountered
| Issue | Resolution |
|-------|------------|
| `Get-Content` repeatedly failed on `ai_config_service.h` with a sandbox helper error | Read the file with `rg -n "^"` instead. |
| One parallel read in the extension/runtime batch failed with a sandbox helper error | Retry the missing file with `rg` rather than repeating the whole batch. |
| First tool-class search used Windows-invalid wildcard path arguments | Reran with header-only `rg` and `-g "*.h"`. |
| Recursive `rg` path using `editor\\agent_v1\\**\\SCsub` is invalid on Windows | Used direct `editor/SCsub` findings; enough for build integration. |
| `Select-String` keyword check hit a sandbox helper error | Re-ran the keyword verification with `rg`. |

## Resources
- `editor/agent_v1`
- `editor/agent_ui`
- `README.md`

## Visual/Browser Findings
- None.
