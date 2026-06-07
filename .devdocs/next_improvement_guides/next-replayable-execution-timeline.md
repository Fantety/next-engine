# NEXT Replayable Execution Timeline

## Purpose

Give users a clear, replayable view of what happened during a NEXT workflow run. The goal is to remove the feeling that multiple agents are changing the project behind a curtain.

## User Experience

When the user runs a milestone, the NEXT panel shows a timeline such as:

```text
10:21 Milestone started: Core Movement
10:21 Script Agent started: Create player movement script
10:22 Tool call: project.read_file res://player.gd
10:22 Tool call: script.write res://scripts/player_controller.gd
10:23 Task completed: Create player movement script
10:23 Scene Agent started: Bind script to Player node
10:24 Task completed: Scene saved
10:24 Milestone completed, waiting for acceptance
```

The user can open older workflow runs and inspect the same timeline after the fact.

## Existing Basis

- `AINextEventLog` already stores structured events with timestamp, event type, milestone id, task id, agent id, message, and metadata.
- `AIAgentNextSession` already records planning, task, review, feedback, resume, cancel, and lock events.
- `AINextWorkflowSnapshot` persists `event_log` with each workflow.
- `AINextPanel` already has an Activity foldable section that shows recent runtime messages and events.

## Proposed Design

Promote the current activity list into a first-class replay timeline.

Each timeline row should show:

- timestamp
- event kind
- agent name
- milestone and task context when available
- short message
- optional metadata summary, such as tool name, output path, error, or change set id

The first iteration should remain read-only. It should not replay tool calls or mutate project state. "Replay" means reconstructing the human-readable execution story from stored events and agent run messages.

## Data Model

Keep using `AINextEventLog` as the canonical source. Add helper query APIs rather than changing the storage format heavily:

- get events by workflow run id
- get events by milestone id
- get events by task id
- get events by agent id
- get timeline rows as normalized dictionaries

If a runtime message maps to a tool call, store a compact event metadata payload:

```json
{
  "tool_name": "script.write",
  "path": "res://scripts/player_controller.gd",
  "status": "completed"
}
```

## UI Placement

Use the existing `Activity` section in `AINextPanel` as the entry point.

Recommended first UI:

- summary in the foldable title bar: latest event or "12 events"
- list of recent events while folded/open
- filter chips later: All, Milestone, Task, Tool, Error

## Acceptance Criteria

- User can see task start, tool activity, task completion, milestone completion, review, feedback, rollback, and lock events.
- Timeline survives closing and reopening the editor because it is persisted in workflow snapshots.
- Failed agent runs show the error event and the task context.
- Timeline rows are readable without opening raw JSON.

## Risks

- Too many streaming updates can make the timeline noisy. Only record durable milestones, tool calls, task state changes, and user decisions.
- Runtime messages and event log may duplicate information. The UI should prefer event log rows and only supplement with runtime progress when a run is active.

## First Implementation Step

Add a normalized timeline query method to `AINextEventLog`, then update `AINextPanel::_refresh_activity()` to render those rows consistently.
