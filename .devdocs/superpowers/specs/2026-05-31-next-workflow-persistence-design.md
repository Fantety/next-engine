# NEXT Workflow Persistence Design

## Purpose

This document defines the persistence and resume architecture for NEXT workflows in the editor AI system.

NEXT should no longer behave as one global in-memory project plan. It should behave like `AIAgentSession`: a project-scoped session system where users can create multiple NEXT workflows, switch between them, terminate active execution, and later continue from a durable checkpoint.

The design also makes sub-agent conversations durable. A workflow checkpoint without each sub-agent's message history is not enough: the system would only be able to restart tasks from scratch, not resume the agent context that led to the current state.

## Current State

Relevant existing code:

- `editor/ai_component/agent/AIAgentSession`
  - Project-scoped conversation storage.
  - Loads the most recent session on construction.
  - Saves before and after runtime turns.
  - Cancels active work by recording cancelled state and ignoring late runtime results.
- `editor/ai_component/storage/AIConversationStore`
  - Stores JSON under `user://ai_agent/projects/<project_scope>/conversations`.
  - Supports save, load, delete, list, and most-recent lookup.
- `editor/ai_component/next/AIAgentNextSession`
  - Owns NEXT workflow orchestration.
  - Starts sub-agent runtime work asynchronously.
  - Updates `AINextProjectState` and `AINextEventLog`.
  - Has no load/save integration today.
- `editor/ai_component/next/AINextProjectStore`
  - Stores only `AINextProjectState`.
  - Uses `user://ai_agent/next/<project_key>.json`.
  - Does not project-scope storage like `AIConversationStore`.
  - Does not persist event logs, workflow checkpoint state, workflow metadata, or sub-agent run conversations.
- `editor/ai_component/next/agents`
  - Provides dedicated `AIAgentBase` subclasses for planning, script, scene, shader, and review agents.

Important runtime constraint:

`AIAgentRuntimeRunner` has `start`, `wait_to_finish`, and `is_running`, but it has no generic provider abort API. A user interrupt cannot safely pause and resume the same provider request. It must be represented as a durable workflow termination, then later restarted from the last stable checkpoint.

## Goals

- Support multiple NEXT workflow sessions per project.
- Provide create, load, list, delete, and switch behavior comparable to chat sessions.
- Persist workflow project state, event log, execution checkpoint, and sub-agent run conversations.
- Let users terminate an active NEXT execution and later continue from a stable checkpoint.
- Ignore late runtime results from a terminated or switched-away workflow.
- Preserve completed task checkpoints and avoid repeating completed tasks.
- Preserve stable sub-agent conversation messages so continuation keeps context.
- Keep responsibilities readable and focused for later maintenance.
- Keep storage JSON forward-compatible with versioned root fields.

## Non-Goals

- Do not restore the same provider HTTP request or runtime thread after interrupt.
- Do not introduce a global event-sourcing replay engine.
- Do not auto-run workflows immediately after editor startup.
- Do not solve collaborative multi-editor conflict resolution.
- Do not refactor unrelated chat session storage.

## Architecture

The new infrastructure has three persistent layers:

1. **Workflow Session**
   - A user-visible NEXT workflow, similar to an `AIAgentSession` conversation.
   - Project-scoped.
   - Has metadata for list and jump UI.

2. **Workflow Checkpoint**
   - Durable orchestration state for one workflow.
   - Records which operation was active, which task/milestone was active, and which run id owns the active provider result.
   - Defines how to resume after user termination or app restart.

3. **Sub-Agent Run State**
   - Durable message history for a single sub-agent operation.
   - Stores stable `AIAgentMessage` entries and run metadata.
   - Allows the next provider turn to continue from prior assistant/tool context.

The session orchestrator remains `AIAgentNextSession`. It should not become a storage parser or a UI list model. Storage and serialization should move into focused helper classes.

## File Responsibilities

### Existing Files To Extend

- `editor/ai_component/next/ai_agent_next_session.h/.cpp`
  - Owns active workflow id and in-memory workflow state.
  - Calls store load/save operations.
  - Creates workflow checkpoints before starting runtime work.
  - Normalizes interrupted state.
  - Starts sub-agent runs from persisted messages where available.
  - Ignores stale runtime results using workflow id and run id.

- `editor/ai_component/next/ai_next_project_state.h/.cpp`
  - Keeps domain state for milestones, tasks, and assets.
  - May gain helpers to normalize interrupted in-progress tasks back to resumable state.
  - Should not know about workflow session lists or sub-agent message history.

- `editor/ai_component/next/ai_next_project_store.h/.cpp`
  - Should be replaced or evolved into workflow-session storage.
  - Must support project scoping and metadata listing.

- `editor/ai_component/next/ai_next_event_log.h/.cpp`
  - Already supports array serialization.
  - No major responsibility change.

- `editor/ai_component/ui/ai_agent_next_dock.cpp`
  - Owns a single `AIAgentNextSession` node.
  - Applies agent model settings after workflow load or creation.

- `editor/ai_component/ui/ai_next_panel.*`
  - Should expose workflow management controls or host a dedicated child component.
  - Should refresh when workflow session changes.

### New Files

- `editor/ai_component/next/ai_next_workflow_snapshot.h/.cpp`
  - Defines serializable data structures:
    - `AINextWorkflowCheckpoint`
    - `AINextAgentRunState`
    - `AINextWorkflowSnapshot`
    - `AINextWorkflowMetadata`
  - Converts these structures to/from `Dictionary`.
  - Keeps schema version handling local.

- `editor/ai_component/next/ai_next_workflow_store.h/.cpp`
  - Project-scoped store equivalent to `AIConversationStore`.
  - Provides:
    - `set_project_scope(scope_key)`
    - `save_workflow(snapshot)`
    - `load_workflow(workflow_id, snapshot)`
    - `delete_workflow(workflow_id)`
    - `list_workflows()`
    - `get_most_recent_workflow_id(out_id)`
  - Stores JSON under:
    - `user://ai_agent/projects/<project_scope>/next_workflows/<workflow_id>.json`

- `editor/ai_component/ui/ai_next_workflow_list.h/.cpp`
  - Optional first UI split if the panel would become too dense.
  - Displays workflow metadata and exposes new/load/delete actions.

## Persistent JSON Shape

Each workflow file should use a versioned root object:

```json
{
  "schema_version": 1,
  "id": "workflow_...",
  "title": "Build inventory prototype",
  "created_at": 1780000000,
  "updated_at": 1780000100,
  "project_state": {},
  "event_log": [],
  "checkpoint": {},
  "agent_runs": []
}
```

### Metadata

Metadata is derived from the root object and `project_state`:

- `id`
- `title`
- `updated_at`
- `session_state`
- `active_milestone_id`
- `milestone_count`
- `task_count`
- `has_resumable_checkpoint`

The store should load metadata without exposing implementation details to UI code.

### Workflow Checkpoint

Fields:

- `status`
  - `idle`
  - `running`
  - `user_terminated`
  - `failed`
- `operation`
  - `none`
  - `generate_plan`
  - `run_task`
  - `run_milestone`
  - `review`
  - `feedback_tasks`
- `workflow_run_id`
  - Unique id for a started orchestration run.
- `agent_run_id`
  - Active sub-agent run id.
- `agent_id`
- `milestone_id`
- `task_id`
- `single_task_run`
- `feedback_text`
- `feedback_previous_task_count`
- `selected_task_id`
- `active_task_batch`
- `active_task_batch_index`

The checkpoint is the authority for whether a workflow can be resumed. In-memory fields in `AIAgentNextSession` are reconstructed from it, not the other way around.

### Sub-Agent Run State

Fields:

- `run_id`
- `workflow_id`
- `agent_id`
- `operation`
- `milestone_id`
- `task_id`
- `status`
  - `running`
  - `waiting_provider`
  - `user_terminated`
  - `completed`
  - `failed`
- `messages`
  - Serialized `AIAgentMessage` array.
- `runtime_base_message_count`
  - Optional, used only while applying an in-memory runtime result.
- `created_at`
- `updated_at`

Stable messages are durable. Partial streaming assistant fragments are not stable until they are committed by the runtime. On user termination, partial assistant text should be discarded before saving.

## Lifecycle Flows

### Editor Startup

1. `AIAgentNextSession` constructs the store.
2. Store is scoped with the current project key, using the same resource-path MD5 pattern as `AIAgentSession`.
3. Load the most recent NEXT workflow if one exists.
4. If no workflow exists, create a new empty workflow.
5. Normalize any stored `running` checkpoint to a resumable `user_terminated` checkpoint because no runtime thread survived process shutdown.
6. Emit `workflow_session_changed` and `project_state_changed`.

Startup should not automatically resume execution. The user must explicitly continue.

### New Workflow

1. If current workflow is active, terminate it first.
2. Generate a new workflow id.
3. Reset `AINextProjectState`, event log, checkpoint, runtime messages, and selected task.
4. Derive title from the brief when the user submits it, or use `New NEXT Workflow`.
5. Save immediately.

### Load / Switch Workflow

1. If current workflow is active, perform user termination first.
2. Save current workflow.
3. Load target workflow snapshot.
4. Replace project state, event log, checkpoint, agent runs, runtime messages, and selection.
5. Normalize interrupted/running task state.
6. Emit workflow and project change signals.

Late runtime results from the old workflow must be ignored. Runtime finish handling must compare both active workflow id and active run id.

### Delete Workflow

1. Refuse deleting while active unless the delete path first terminates the active workflow.
2. Delete the workflow file.
3. If deleting current workflow, load the most recent remaining workflow.
4. If none remain, create a new empty workflow.

### Start Operation

All operation entry points follow the same pattern:

1. Build or load the sub-agent run state.
2. Create a new run id.
3. Save checkpoint with `status=running`.
4. Save the request messages before starting `AIAgentBase::start`.
5. Start runtime.
6. If runtime fails to start, mark checkpoint failed and save.

This mirrors `AIAgentSession::send_user_message`, where user input is saved before the runtime begins.

### Runtime Progress

Runtime progress is useful for UI but should not be treated as committed durable conversation by default.

Recommended rule:

- Save stable messages when a provider turn completes or a tool result is appended.
- Keep streaming UI progress in memory.
- If a user terminates during streaming, discard partial assistant progress from durable run messages.

This avoids resuming from incomplete assistant text.

### Runtime Finish

Runtime finish handling must validate:

- The workflow id still matches.
- The workflow run id still matches.
- The sub-agent run id still matches.
- The checkpoint status is still `running`.

If any check fails, ignore the result. This is the same safety model as `AIAgentSession` cancellation, but with explicit run ids so switching workflows is safe.

### User Termination

User termination is the explicit meaning of "interrupt".

1. Mark checkpoint `user_terminated`.
2. Mark active agent run `user_terminated`.
3. Normalize active task:
   - If the task is still `in_progress`, move it back to `pending`.
   - Completed, skipped, and failed tasks remain unchanged.
4. Clear in-memory pending operation fields.
5. Keep completed task results.
6. Save workflow snapshot.
7. Emit UI refresh signals.
8. Ignore late runtime result by run id mismatch or checkpoint status.

The provider thread may still finish later. Its output must not mutate the workflow after termination.

### Continue Workflow

Continue should be explicit. It should not start automatically on load.

Rules:

- If checkpoint operation is `run_task`, restart that task using its agent run messages.
- If checkpoint operation is `run_milestone`, resume the current task if present; otherwise schedule the next ready task.
- If checkpoint operation is `generate_plan`, resume planning from planning run messages.
- If checkpoint operation is `review`, resume the review run messages.
- If checkpoint operation is `feedback_tasks`, resume the planning run with stored feedback text and previous task count.
- If no resumable checkpoint exists, continue should be disabled.

## Sub-Agent Conversation Resume

Sub-agent runs should be created through a small session-owned API:

- `_get_or_create_agent_run(operation, agent_id, milestone_id, task_id)`
- `_append_agent_run_message(run_id, message)`
- `_replace_agent_run_messages(run_id, messages)`
- `_mark_agent_run_status(run_id, status)`
- `_get_agent_run_messages(run_id)`

When a task is started for the first time, its user message is saved in the run state:

```text
Run NEXT task `Create player script`.

Description:
...
```

When resumed, the runtime receives the persisted messages instead of a freshly created single user message. This preserves prior assistant/tool context.

For side-effect safety:

- A completed tool result must be kept, so resume does not re-execute that completed tool call.
- A running tool call without a tool result should be marked failed or omitted before resume, depending on what was durably recorded.
- Partial assistant streaming content should not be saved as stable conversation.

## UI Model

The UI should expose workflow management without mixing it into task rendering code.

Minimum UI:

- Workflow selector/list with title, state, updated time.
- New workflow button.
- Delete workflow button.
- Continue button when a resumable checkpoint exists.
- Terminate button while workflow is running.

Existing panels continue to show the active workflow's milestones, tasks, inspector, feedback, and runtime activity.

Signals:

- `workflow_session_changed`
- `project_state_changed`
- `agent_progress_changed`
- existing `state_changed`

## Error Handling

- Store parse failure should not crash the editor. It should create a fresh workflow and record/log an error.
- Save failures should be recorded in the event log and visible through `last_error`.
- Unknown agent ids in persisted runs should fail the run gracefully.
- Missing task/milestone references in a checkpoint should invalidate the checkpoint and leave the workflow idle.
- Schema version mismatch should use best-effort load for known fields.

## Maintainability Rules

- `AIAgentNextSession` owns orchestration, not file parsing.
- `AINextWorkflowStore` owns files and project scoping.
- `AINextWorkflowSnapshot` owns serialization shape.
- `AINextProjectState` owns milestone/task domain state only.
- UI components consume public session methods and metadata arrays; they should not parse snapshot dictionaries.
- Run ids are required for any async result application.
- All state transitions that affect resume must go through one save path.

## Test Strategy

Focused tests should be added before implementation:

- Store round-trips workflow snapshot with project state, event log, checkpoint, and agent runs.
- Store isolates workflows by project scope.
- Session loads the most recent workflow on project scope change.
- Session creates multiple workflows and switches between them.
- Deleting current workflow loads the next most recent workflow or creates a clean one.
- User termination during a running task resets only in-progress work and saves the checkpoint.
- Late runtime result after termination is ignored.
- Late runtime result after switching workflow is ignored.
- Continue resumes a task using persisted sub-agent messages.
- Continue after milestone interruption preserves completed tasks and schedules the next ready task.
- Feedback planning interruption preserves feedback text and previous task count.

Verification commands:

```powershell
scons platform=windows target=editor tests=yes -j4
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[Editor][AI][NEXT]*"
git diff --check
```

## Migration

The existing `AINextProjectStore` format only stores `AINextProjectState`. The new store can support a one-time best-effort import:

1. If no workflow files exist.
2. If the old `user://ai_agent/next/<project_key>.json` file exists.
3. Load it as `project_state`.
4. Create a workflow snapshot with empty event log, idle checkpoint, and no agent runs.

This keeps existing local NEXT plans from disappearing after the storage upgrade.

## Open Implementation Questions

- Whether workflow title rename should be implemented now or deferred.
- Whether the UI workflow list should be embedded in `AINextPanel` first or split immediately into `AINextWorkflowList`.
- Whether stable runtime messages should be persisted only at runtime finish initially, then improved to tool-result checkpoints later.

The recommended first implementation should support full workflow session management and user termination semantics, while persisting sub-agent run messages at operation boundaries. Tool-result-level checkpointing can be tightened in a follow-up if the runtime does not expose enough stable progress events today.
