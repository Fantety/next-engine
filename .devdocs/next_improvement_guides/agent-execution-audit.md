# Agent Execution Audit

## Purpose

Combine user identity and AI event logs into an audit trail that answers who triggered an AI action, what the agents did, what changed, and who approved it.

## User Experience

Project owners can open an audit view:

```text
2026-06-07 10:21 Alice started NEXT milestone Core Movement
2026-06-07 10:22 Script Agent modified res://scripts/player_controller.gd
2026-06-07 10:24 Alice accepted and locked milestone Core Movement
```

This is useful for teams, debugging, and accountability.

## Existing Basis

- `AINextEventLog` records agent and user events.
- `AIChangeSetStore` records file change sets.
- `editor/user_system` tracks current user session.
- NEXT workflow snapshots persist event logs.

## Proposed Design

Extend AI events with user identity metadata when a human triggers an action.

Example metadata:

```json
{
  "user_id": "...",
  "user_name": "Alice",
  "workflow_id": "...",
  "milestone_id": "...",
  "action": "accept_and_lock"
}
```

Agent-generated events should remain agent-attributed, but include the initiating user or workflow run when available.

## Audit Scope

First version should audit:

- brief submission
- plan approval
- milestone run start
- task run start
- feedback submission
- rollback
- keep/revert change package
- milestone lock
- settings changes later

## Acceptance Criteria

- Human-triggered NEXT events include current user metadata when logged in.
- Audit data persists with the workflow.
- Audit view can be reconstructed from event log and change set metadata.
- Offline or anonymous users still produce local audit entries with a fallback identity.

## Risks

- Audit logs can contain sensitive project details. Export and cloud sync need privacy review.
- User identity should not be trusted for security unless backed by server-side authorization.

## First Implementation Step

Add a helper that enriches `AINextEventLog::record_event()` metadata with current user session data when available.
