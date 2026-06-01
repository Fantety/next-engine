# Findings: AI Agent Milestone Streaming Failure

## Reported Symptoms
- NEXT agent operations block the main thread, including but not limited to milestone generation.
- Runtime logs show provider streaming response completes successfully, but finalization fails:
  - `Provider streaming response finalize failed: Failed to parse provider tool arguments.`
  - `Provider turn failed. turn=2 error=Failed to parse provider tool arguments.`
- Nearby logs show the final streaming event has very small chunk/buffer sizes and `done=yes`.

## Investigation Notes
- Error string `Failed to parse provider tool arguments.` is emitted in `editor/ai_component/providers/ai_openai_compatible_codec.cpp`.
- Streaming HTTP events and finalization logging are in `editor/ai_component/providers/ai_openai_runtime_client.cpp`.
- NEXT milestone planning is driven by `editor/ai_component/next/ai_agent_next_session.cpp`; UI button text is "Generate Milestones".
- Existing tests for streaming/runtime live in `tests/editor/test_ai_agent_runtime.cpp`; NEXT session/tool tests live in `tests/editor/test_ai_agent_next_session.cpp` and `tests/editor/test_ai_next_tools.cpp`.
- Root-cause evidence for main-thread blocking: `AIAgentNextSession::generate_plan()` calls `planning_agent->run(messages)` directly from the UI action path. The normal chat session uses `AIAgentBase::start()` / `AIAgentRuntimeRunner` to execute runtime work on a thread.
- Same synchronous pattern exists for `run_active_milestone()` specialist agents (`script_agent`, `scene_agent`, `shader_agent`), `review_active_milestone()` (`review_agent`), and `generate_feedback_tasks()` (`planning_agent`).
- Streaming accumulator parses provider tool arguments on the final/finish event by calling `JSON::parse(tool_call.arguments_json)` and errors if the result is not a dictionary.
- Implemented direction: NEXT session starts agent work with `AIAgentRuntimeRunner` and advances the workflow from `runtime_finished` callbacks.
- Implemented direction: invalid provider tool arguments are converted into failed tool calls with raw argument preview metadata, so runtime reports the tool failure back to the model instead of aborting the provider turn.

## 2026-05-31 NEXT UI / Milestone Flow Findings
- `AINextPanel` only refreshes on `project_state_changed`; it has no running-state query, operation label, activity list, or button disable logic.
- `AINextMilestoneList` renders milestone rows as labels only. It never calls `AINextProjectState::set_active_milestone_id`, so users cannot switch milestones from the UI.
- `AINextTaskTree` renders task rows as labels only. There is no per-task execution path in `AIAgentNextSession`; only `run_active_milestone()` exists.
- `AINextTaskInspector` always reads `tasks[0]` from the active milestone, so it keeps showing the first task rather than the selected/running task.
- `accept_and_lock_active_milestone()` locks the current milestone but does not move `active_milestone_id` to the next milestone, which leaves the UI focused on the locked first milestone.
- Agent runtime progress already exists at the runner level (`runtime_message_added` / `runtime_message_updated`) but NEXT session does not expose or forward those messages to the panel.

## 2026-05-31 NEXT Sub-Agent Extraction Findings
- `AIAgentNextSession` currently owns both workflow orchestration and all sub-agent construction. Its constructor directly instantiates five bare `AIAgentBase` objects and then configures prompts, profiles, tools, and runner callbacks.
- Agent-specific static setup is concentrated in `_configure_agent`, `_register_next_tools`, `_register_shared_read_tools`, and `_register_specialist_write_tools`.
- Runtime/business flow should remain in `AIAgentNextSession`: it builds user messages for planning, task execution, review, and feedback planning; starts agents asynchronously via `AIAgentBase::start`; consumes `AIAgentRuntimeRunner` callbacks; and updates `AINextProjectState` / `AINextEventLog`.
- External supporting facilities that depend on stable agent IDs include `AINextManageProjectTool`, `AINextAgentSettings`, the NEXT settings UI, and tests that inject runtime clients through `get_agent_for_test`.
- The narrow extraction boundary is to move sub-agent identity/prompt/profile/tool registration into `editor/ai_component/next/agents` subclasses of `AIAgentBase`, while preserving session-owned workflow state and callback wiring.

## 2026-05-31 NEXT Persistence / Resume Findings
- `AIAgentSession` is the local reference pattern. It scopes storage to the current project, loads the most recent conversation in its constructor, saves user messages before starting a runtime turn, saves terminal/runtime approval states, and saves cancellation/failure outcomes.
- `AIConversationStore` writes JSON under `user://ai_agent/projects/<project-scope>/conversations` and can list/load/delete by session id. It includes metadata like `id`, `title`, `updated_at`, and serialized messages.
- NEXT currently has `AINextProjectStore`, but `AIAgentNextSession` only instantiates it and exposes `get_project_store()`. No current session method calls `project_store->load()` or `project_store->save()`.
- `AINextProjectStore` stores only `AINextProjectState::to_dict()` under `user://ai_agent/next/<project-key>.json`. It does not scope by project resource path today unless callers pass such a key.
- `AINextProjectState::to_dict()` includes brief, session_state, active_milestone_id, last_error, milestones, tasks, asset records, and task `run_id`; it does not include workflow orchestration fields such as pending operation, pending agent/task/milestone, workflow active flag, single-task run flag, selected task id, active task batch, runtime progress messages, or event log.
- `AINextEventLog` can serialize to/from an array, but current `AINextProjectStore` does not persist it.
- NEXT has a public `cancel_current_operation()`, but it only clears in-memory workflow flags and sets session state to idle. It does not stop the runner thread, mark in-progress tasks back to resumable status, or save cancellation.
- If the app exits while a NEXT runtime is active, the next process cannot continue the actual thread. Resume must be modelled as a durable workflow checkpoint, not as restoring a thread.
- `AIAgentRuntimeRunner` has `start`, `wait_to_finish`, and `is_running`; it has no generic cancel/abort API for the provider request. `AIAgentSession::cancel_request()` sets a cancelled state and ignores a later runner result, which is the pattern NEXT should mirror.
- `AINextProjectState` already serializes task `run_id`, but `AIAgentNextSession` does not currently assign run ids or use them to ignore stale runtime completions after cancellation/resume.
- Existing NEXT task state can represent resumable work if interrupted tasks are normalized from `in_progress` back to `pending`/`ready`, while completed/skipped/failed tasks remain durable checkpoints.
- User clarified that "interrupt" means the user actively terminates the current NEXT execution. It should not be treated as app crash recovery or provider-thread suspension.
- User-terminated NEXT execution should persist a resumable idle/cancelled workflow state, reset only the active in-progress work to a runnable checkpoint, ignore any late runtime result from the old run, and allow the user to resume from that checkpoint.
- Sub-agent conversation state must be durable separately from task status. NEXT currently starts each sub-agent run from a freshly constructed message vector, so it cannot resume an agent's prior tool-call conversation unless those messages are persisted.
- Resume should distinguish incomplete streaming text from stable conversation checkpoints. Stable assistant/tool messages after completed tool calls can be reused; partial assistant streaming content from an interrupted provider call should be discarded before retrying.
- User clarified that NEXT must support multiple workflow sessions per project, similar to chat conversation sessions. Users need to create, manage, and jump between NEXT workflows.
- This means NEXT persistence should not be a single `project_key -> state` file. It needs session metadata (`id`, `title`, `updated_at`, state summary) and operations equivalent to list/load/create/delete/switch, project-scoped like `AIConversationStore`.
- Switching away from an active NEXT workflow should be treated as user termination of that workflow run before loading another workflow, so late runtime results from the old workflow cannot mutate the newly selected workflow.
- Implemented workflow persistence as focused infrastructure:
  - `AINextWorkflowSnapshot` owns schema serialization for workflow metadata, project state, event log, checkpoint, and sub-agent run messages.
  - `AINextWorkflowStore` owns project-scoped workflow files under `user://ai_agent/projects/<scope>/next_workflows`.
  - `AIAgentNextSession` now orchestrates create/list/load/delete/continue and saves snapshots around state transitions.
- User termination now persists a `user_terminated` checkpoint, marks the active agent run `user_terminated`, resets only the active `in_progress` task through `AINextProjectState::reset_interrupted_task`, and ignores late runtime progress/results after termination.
- Sub-agent resume now reuses persisted `AINextAgentRunState.messages` when `continue_workflow()` restarts a run, preserving stable assistant/tool context rather than rebuilding a single fresh user prompt.
- Loading/switching workflows rebinds `AINextPlanningAgent` to the loaded `AINextProjectState`; without this, `ai_next.manage_project` would mutate an old state instance after workflow switches.
- `AINextPanel` now exposes a minimal workflow management bar: workflow selector, New, Delete, Continue, and Terminate actions backed by session APIs.

## 2026-05-31 NEXT Planning Freeze / Partial Restore Findings
- NEXT planning does not parse final free-text assistant content into milestones. Milestones are written when the planning agent calls `ai_next.manage_project` with `action = replace_plan`; `AINextManageProjectTool::execute()` validates the `milestones` array and mutates `AINextProjectState` via `replace_from_milestones_array`.
- `AIAgentNextSession::_finish_generate_plan()` only validates that the project state now has at least one milestone and moves the session to human approval; it is not the source of milestone parsing.
- The likely data-loss boundary is after `AINextManageProjectTool` mutates `project_state` but before `AIAgentNextSession` saves the workflow snapshot. Current saves happen before runtime start and when deferred runtime finish/state changes are processed, not immediately after a successful `ai_next.manage_project` tool result during runtime.
- Root cause found inside the tool validation path: `AINextProjectState::has_dependency_cycle()` used a path-insensitive expanding stack with no visited state. This can falsely reject valid shared dependencies and can expand the same dependency subgraphs repeatedly for dense generated plans, making `ai_next.manage_project` appear to hang during execution.
- Implemented fix keeps workflow persistence ownership in `AIAgentNextSession`: runtime progress callbacks now persist stable agent-run messages, and when a completed `ai_next.manage_project` tool result is observed on the main thread, the session immediately saves and emits project-state changes. The tool still only mutates `AINextProjectState`.

## 2026-06-01 NEXT Stutter Audit Findings
- NEXT runtime work is threaded, but streamed progress returns to the editor through deferred main-thread signals. A fast provider can enqueue many `runtime_message_updated` callbacks before the panel has time to render a frame.
- Each progress update previously propagated into `AIAgentNextSession`, updated in-memory activity rows, stored active agent-run messages, emitted `agent_progress_changed`, and could trigger full activity rebuilds. Plain assistant text chunks are not stable resume checkpoints and do not need durable agent-run persistence.
- Completed `ai_next.manage_project` writes must still save immediately for durability, but ordinary project-state notifications were also synchronously saving workflow snapshots. That serialized full project/event/agent state and wrote workflow JSON on the main thread.
- `AINextPanel::_refresh()` refreshed workflow lists on every project-state refresh. The workflow list path loaded and parsed every workflow file, and metadata loading previously constructed full workflow snapshots and project state objects.
- The activity feed only renders a few rows, but it used public deep-copy accessors for all runtime messages and all event-log entries on every progress refresh.
- Script and shader write helpers dispatch editor mutations to the main thread correctly, but `EditorFileSystem::scan_changes()` was called synchronously after each write and could add noticeable stalls during NEXT tool-heavy runs.
- Implemented mitigations: coalesce runner update callbacks, coalesce NEXT progress signals, skip durable checkpoints for plain streamed assistant chunks, keep immediate saves for completed NEXT project writes only, avoid duplicate saves after explicit workflow saves, refresh workflow lists only on workflow-session changes, parse metadata directly for list views, add bounded recent-message/event accessors for UI refresh, remove per-chunk streaming logs, and defer file-system scans.
