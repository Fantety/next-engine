# NEXT Workflow Persistence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add project-scoped, multi-workflow NEXT persistence with resumable interruption checkpoints and durable sub-agent run history.

**Architecture:** Keep `AIAgentNextSession` as the workflow orchestrator. Put JSON schema and message serialization in `AINextWorkflowSnapshot`, put filesystem and project scoping in `AINextWorkflowStore`, and expose session-level create/list/load/delete/continue APIs for UI and tests.

**Tech Stack:** Godot editor C++ modules, existing `AIAgentMessage` serialization, `AINextProjectState`, `AINextEventLog`, `AIAgentRuntimeRunner`, doctest-style editor tests.

---

## File Map

- Create `editor/ai_component/next/ai_next_workflow_snapshot.h/.cpp`
  - Versioned serializable structs for workflow metadata, checkpoint, and sub-agent run state.
  - Converts persisted agent messages through `AIConversationSerializer`.
- Create `editor/ai_component/next/ai_next_workflow_store.h/.cpp`
  - Project-scoped workflow file store under `user://ai_agent/projects/<scope>/next_workflows`.
  - Supports save/load/delete/list/latest and metadata loading.
- Modify `editor/ai_component/next/SCsub`
  - Compile new NEXT infrastructure files automatically through existing wildcard behavior, no special source list needed.
- Modify `editor/ai_component/next/ai_next_project_state.h/.cpp`
  - Add a focused helper to normalize interrupted `in_progress` tasks back to pending.
- Modify `editor/ai_component/next/ai_agent_next_session.h/.cpp`
  - Replace single-project store usage with workflow store.
  - Add workflow ids/titles, checkpoint state, agent run state map, save/load helpers, create/list/load/delete/continue APIs, run-id guarded runtime finishing.
- Modify `tests/editor/test_ai_next_project_state.cpp`
  - Add store/snapshot round-trip and project-scope isolation tests.
- Modify `tests/editor/test_ai_agent_next_session.cpp`
  - Add session workflow management, interruption, stale-result, and resume-message tests.
- Modify UI files only after core behavior is stable:
  - `editor/ai_component/ui/ai_next_panel.*`
  - optional `editor/ai_component/ui/ai_next_workflow_list.*`

## Tasks

### Task 1: Snapshot And Store

**Files:**
- Create: `editor/ai_component/next/ai_next_workflow_snapshot.h`
- Create: `editor/ai_component/next/ai_next_workflow_snapshot.cpp`
- Create: `editor/ai_component/next/ai_next_workflow_store.h`
- Create: `editor/ai_component/next/ai_next_workflow_store.cpp`
- Test: `tests/editor/test_ai_next_project_state.cpp`

- [x] Step 1: Write failing tests for snapshot round-trip including project state, event log, checkpoint, and agent run messages.
- [x] Step 2: Write failing tests for workflow store project-scope isolation and most-recent ordering.
- [x] Step 3: Implement snapshot structs and serialization.
- [x] Step 4: Implement workflow store save/load/delete/list/latest with atomic temp-file writes.
- [x] Step 5: Run `scons platform=windows target=editor tests=yes -j4` and focused NEXT tests.

### Task 2: Session Workflow Management

**Files:**
- Modify: `editor/ai_component/next/ai_agent_next_session.h`
- Modify: `editor/ai_component/next/ai_agent_next_session.cpp`
- Test: `tests/editor/test_ai_agent_next_session.cpp`

- [x] Step 1: Write failing tests for create/list/load/delete/switch multiple NEXT workflows.
- [x] Step 2: Add workflow id/title metadata and `_save_current_workflow` / `_load_workflow_snapshot`.
- [x] Step 3: Load most recent project workflow on construction or create a clean one.
- [x] Step 4: Save after brief submission, selection changes, state changes, and operation completion/failure.
- [x] Step 5: Run focused NEXT tests.

### Task 3: Interruption And Stale Result Guard

**Files:**
- Modify: `editor/ai_component/next/ai_next_project_state.h`
- Modify: `editor/ai_component/next/ai_next_project_state.cpp`
- Modify: `editor/ai_component/next/ai_agent_next_session.h`
- Modify: `editor/ai_component/next/ai_agent_next_session.cpp`
- Test: `tests/editor/test_ai_agent_next_session.cpp`

- [x] Step 1: Write failing test that cancelling a running task persists `user_terminated`, resets only the active `in_progress` task, and leaves completed work intact.
- [x] Step 2: Write failing test that a late runtime result after cancellation is ignored.
- [x] Step 3: Add checkpoint status/operation/run-id updates before every runtime start.
- [x] Step 4: Make `cancel_current_operation()` user termination, not an in-memory clear only.
- [x] Step 5: Validate workflow id, workflow run id, agent run id, and checkpoint status before applying runtime results.

### Task 4: Sub-Agent Conversation Resume

**Files:**
- Modify: `editor/ai_component/next/ai_agent_next_session.h`
- Modify: `editor/ai_component/next/ai_agent_next_session.cpp`
- Test: `tests/editor/test_ai_agent_next_session.cpp`

- [x] Step 1: Write failing test that resume starts an agent from persisted messages, not a fresh single-user message.
- [x] Step 2: Store sub-agent run messages at runtime start and replace them with stable result messages at runtime finish.
- [x] Step 3: Add `continue_workflow()` for resumable checkpoint operations.
- [x] Step 4: Ensure partial streaming progress remains in memory only.

### Task 5: UI Management Entry Points

**Files:**
- Modify: `editor/ai_component/ui/ai_next_panel.*`
- Optional create: `editor/ai_component/ui/ai_next_workflow_list.h/.cpp`
- Test: existing UI tests or focused compile verification

- [x] Step 1: Expose workflow selector/list metadata, New, Delete, Continue, and Terminate actions.
- [x] Step 2: Keep workflow-list UI separate from task rendering logic if the panel becomes dense.
- [x] Step 3: Refresh UI on `workflow_session_changed`, `project_state_changed`, and `agent_progress_changed`.

## Verification

- [x] `scons platform=windows target=editor tests=yes -j4`
- [x] `bin\next.windows.editor.x86_64.console.exe --test --test-case="*[Editor][AI][NEXT]*"`
- [x] `git diff --check`
