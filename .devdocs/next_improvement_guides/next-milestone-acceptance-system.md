# NEXT Milestone Acceptance System

## Purpose

Turn milestone completion into an explicit human acceptance step. NEXT should feel like staged delivery: agents complete work, the user inspects the result, gives feedback if needed, and locks the milestone only when satisfied.

## User Experience

After a milestone run finishes, the user sees an acceptance summary:

```text
Milestone: Core Movement

Completed:
- Created player movement script
- Added exported speed and jump parameters
- Bound the script to Player

Produced:
- res://scripts/player_controller.gd
- res://scenes/player.tscn

Acceptance checks:
- Player scene opens
- Movement parameters are exposed
- No task failed

Available actions:
[Review] [Send Playtest Feedback] [Rollback Safe Changes] [Accept & Lock]
```

## Existing Basis

- `AINextProjectState` tracks milestones, tasks, task status, result summaries, and output paths.
- `review_active_milestone()` already exists.
- `AINextFeedbackPanel` already provides playtest feedback and `Accept & Lock`.
- `MarkdownViewer` is already used in NEXT UI for review findings and task details.

## Proposed Design

Add an acceptance summary view for the active milestone. It should be generated locally from structured state first, not by asking the model.

Summary fields:

- milestone title and status
- completed tasks
- failed or blocked tasks
- task result summaries
- output paths
- recent review findings
- safe change set count
- warnings for untracked or non-revertible changes

This summary can be rendered with `MarkdownViewer`.

## Feedback Behavior

Current implementation: Playtest Feedback uses `AINextFeedbackPanel::_generate_pressed()` -> `AIAgentNextSession::generate_feedback_tasks()`. That path asks the Planning Agent to call `ai_next.manage_project` with `append_tasks`, so current feedback becomes additional fix tasks.

Target behavior: Playtest Feedback should first try to continue the problem task's repair conversation, not always create a new task. `AIAgentNextSession::send_task_session_message()` and `can_continue_task_session()` already provide the lower-level task-session continuation path; the missing piece is a feedback attribution UI/session layer that decides when to use it.

Rule:

- If feedback can be attributed to an existing task, append a repair message to that task session with `send_task_session_message()` and rerun the assigned agent.
- If feedback is a new requirement, cannot be attributed, or crosses multiple tasks, keep the current Planning Agent `append_tasks` path.

Examples that repair existing tasks:

- jump is too high
- shader is too bright
- script throws an error
- button is misaligned

Examples that become new tasks:

- add double jump
- add a new enemy system
- create a new level
- add sound effects

## Acceptance States

The first version can reuse existing milestone statuses and add UI-level acceptance interpretation:

- completed tasks but unlocked milestone: waiting for acceptance
- feedback submitted: repair in progress or waiting for repair
- locked milestone: accepted baseline

Later versions can add explicit milestone acceptance fields.

## Acceptance Criteria

- Completed milestone shows a readable acceptance summary.
- User can see outputs and task results without opening each task.
- User can send feedback from the acceptance area.
- User can lock only when `can_lock_active_milestone()` allows it.
- Timeline records acceptance, feedback, repair, and lock decisions.

## Risks

- A model-generated acceptance report may hide missing data. First version should be deterministic.
- If feedback always creates tasks, the plan becomes noisy. Keep the current `append_tasks` behavior as the fallback, but make repair conversation the preferred path when attribution is clear.

## First Implementation Step

Create a local milestone acceptance summary builder in NEXT UI or session layer, then render it in a new foldable section above Playtest Feedback. After that, add feedback attribution: selected task or explicit task picker should call `send_task_session_message()`, while "new issue / broader change" should keep using `generate_feedback_tasks()`.
