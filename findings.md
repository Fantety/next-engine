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
| Verify Skill/MCP with code tracing plus targeted doctest runs | The user asked whether the current Skill and MCP links work, so static inspection alone is insufficient. |
| Generate `best_practices.gen.h` at build time | User preferred using SCons to generate a prompt header and hardcode the content into the program instead of runtime file reads. |
| Append bundled best practices after configured system prompts | This preserves user/project agent prompt configuration while guaranteeing the fixed Godot guidance is present. |

## Skill/MCP Chain Findings
- MCP configuration chain is wired: `AIAgentSettingsDialog` emits MCP settings changes, MCP settings pages call `AIAgentV1UIBridge::patch_settings()`, `AIAgentV1UIConfigAdapter::patch_settings()` persists config and calls `AIV1MCPService::import_config()`, and `AIAgentDock::_mcp_settings_changed()` calls `refresh_mcp_status()`. `AIAgentV1UIBridge::refresh_mcp_status()` reimports config, calls `AIV1MCPService::refresh()`, and returns statuses/summary.
- MCP runtime chain is wired: `AIV1MCPService` discovers configured/fake server tools, resources, and prompts; MCP tools are registered through `AIV1ToolRegistry`; resource reads, prompt rendering, startup permission, tool permission, disconnect handling, and failed-discovery rollback are covered by tests.
- Skill configuration chain is partially UI-facing and fully runtime-facing: Skills settings call `AIAgentV1UIBridge::patch_settings()`, then `AIAgentV1UIConfigAdapter::patch_settings()` persists config and calls `AIV1SkillService::import_config()`.
- Skill runtime chain is wired: `AISessionRunner::_configure_skill_service_from_config()` imports Skill config, calls `AIV1SkillService::refresh_struct()`, selects skills, converts selected skills into context sources, and injects them into the Context Epoch before model request creation.
- Skill tool chain is wired when enabled: Skill script tools are registered through `AIV1ToolRegistry`, carry `tool_origin = skill` metadata, and go through the configured permission action `skill.script.run`.
- UI status chain is wired: the shared `AIAgentV1UIBridge` owns the MCP and Skill services used by config/session adapters, and `AIStatusPanel` can combine MCP and Skill status tabs.

## Skill/MCP Caveats
- I found an explicit MCP settings refresh path from settings change to `refresh_mcp_status()`. For Skills, the settings patch path imports config, while manifest discovery/refresh is explicit in the Runner path. Runtime Skill use is verified; immediate settings-page manifest refresh may depend on an existing service snapshot or a later Runner refresh unless another UI signal path is added.
- The Dock status-panel test passes, but after success the process prints cleanup/theming warnings about leaked RIDs/ObjectDB instances and early `AIComposer` theme access. These warnings do not fail the test, but they are worth cleaning separately.

## Skill/MCP Verification Results
| Check | Command | Result |
|-------|---------|--------|
| MCP service tests | `bin\next.windows.editor.dev.x86_64.console.exe --test --test-case="*[Editor][AgentV1] MCP*"` | 11 test cases, 116 assertions passed |
| Skill service tests | `bin\next.windows.editor.dev.x86_64.console.exe --test --test-case="*[Editor][AgentV1] Skill*"` | 4 test cases, 76 assertions passed |
| Skill runner Context Epoch injection | `bin\next.windows.editor.dev.x86_64.console.exe --test --test-case="*[Editor][AgentV1] Session runner injects selected Skill into Context Epoch"` | 1 test case, 18 assertions passed |
| MCP settings bridge write path | `bin\next.windows.editor.dev.x86_64.console.exe --test --test-case="*[Editor][AgentV1] Settings MCP page writes MCP servers through bridge"` | 1 test case, 5 assertions passed |
| Skill/rules settings bridge write path | `bin\next.windows.editor.dev.x86_64.console.exe --test --test-case="*[Editor][AgentV1] Settings skills and rules pages patch agent_v1 config"` | 1 test case, 6 assertions passed |
| Shared UI backend services | `bin\next.windows.editor.dev.x86_64.console.exe --test --test-case="*[Editor][AgentV1][UIAdapter] Bridge owns shared backend services for UI adapters"` | 1 test case, 15 assertions passed |
| Dock MCP/Skill status tabs | `bin\next.windows.editor.dev.x86_64.console.exe --test --test-case="*[Editor][AgentUI] Dock combines MCP and Skill status in server popup tabs"` | 1 test case, 15 assertions passed; cleanup/theming warnings printed after success |

## Issues Encountered
| Issue | Resolution |
|-------|------------|
| `Get-Content` repeatedly failed on `ai_config_service.h` with a sandbox helper error | Read the file with `rg -n "^"` instead. |
| One parallel read in the extension/runtime batch failed with a sandbox helper error | Retry the missing file with `rg` rather than repeating the whole batch. |
| First tool-class search used Windows-invalid wildcard path arguments | Reran with header-only `rg` and `-g "*.h"`. |
| Recursive `rg` path using `editor\\agent_v1\\**\\SCsub` is invalid on Windows | Used direct `editor/SCsub` findings; enough for build integration. |
| `Select-String` keyword check hit a sandbox helper error | Re-ran the keyword verification with `rg`. |
| Non-dev editor test binary is not built with tests enabled | Used `bin\next.windows.editor.dev.x86_64.console.exe`. |
| First build after the prompt refactor failed when `core/os/os.h` was removed | Restored the include because the file still uses `OS` later. |
| Parallel targeted prompt test run produced one `Failed to delete files` startup cleanup message | Reran both prompt tests serially; both produced clean doctest success output. |
| Generated best-practices header stayed stale after the markdown changed | Added explicit SCons dependencies from config objects to the generated header and verified SCons regenerated it before compiling. |

## Agent Prompt Findings
- The fixed fallback prompt lived in two places before this change: `AIConfigService::get_system_prompt()` and `AIAgentConfig::from_dictionary()`.
- `AIConfigService::get_system_prompt()` is the right assembly point for fixed best-practices injection because it is already the config-facing source used by request building.
- `AIAgentConfig::from_dictionary()` still needs the same optimized fixed fallback so resolved agent configs and config-service fallback stay aligned.
- `AIAgentConfig::from_dictionary()` should not append the bundled document because `AIAgentService::list()` feeds UI config snapshots; final runtime prompt assembly already goes through `AIContextSourceRegistry`, which calls `AIConfigService::get_system_prompt()`.
- `editor/editor_builders.py` already exposes `make_ai_best_practices_header()`, so `editor/SCsub` can generate `editor/agent_v1/best_practices.gen.h` directly from `editor/agent_v1/best_practices.md`.
- A stable generated-block marker is safer than checking for a best-practices section title, because user prompts can mention `Core Architectural Philosophy` without already containing the bundled document.
- `editor/agent_v1/config/SCsub` needs an explicit dependency on the generated header; otherwise the command can exist without reliably updating the header before `ai_config_service.cpp` compiles.
- The bundled `best_practices.md` has been shortened into a compact, ASCII-only prompt payload so it is safer to embed in a generated C++ raw string.

## Agent Prompt Verification Results
| Check | Command | Result |
|-------|---------|--------|
| Editor test build | `scons platform=windows target=editor dev_build=yes tests=yes -j4` | Exit 0; generated `best_practices.gen.h`, compiled touched files; existing SCsub SyntaxWarning and PDB LNK4099 warnings printed |
| Fixed prompt + bundled best practices | `bin\next.windows.editor.dev.x86_64.console.exe --test --test-case="*[Editor][AgentV1] Config service includes fixed agent guidance and bundled best practices*"` | 1 test case, 5 assertions passed |
| Custom prompt + bundled best practices | `bin\next.windows.editor.dev.x86_64.console.exe --test --test-case="*[Editor][AgentV1] Configured agent prompt still receives bundled best practices*"` | 1 test case, 5 assertions passed |
| Config service group | `bin\next.windows.editor.dev.x86_64.console.exe --test --test-case="*[Editor][AgentV1] Config service*"` | 2 test cases, 19 assertions passed |
| Agent resolve behavior | `bin\next.windows.editor.dev.x86_64.console.exe --test --test-case="*[Editor][AgentV1] Agent service resolves configured and session-bound agents*"` | 1 test case, 9 assertions passed |
| Whitespace check | `git diff --check` | Exit 0, no output |

## Resources
- `editor/agent_v1`
- `editor/agent_ui`
- `README.md`

## Visual/Browser Findings
- None.
