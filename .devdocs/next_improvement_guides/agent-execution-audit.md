# Agent Execution Audit

## Purpose

Combine user identity and AI activity logs into an audit trail that answers who triggered an AI action, what MainAgent did, what changed, and who approved it.

## User Experience

Project owners can open an audit view:

```text
2026-06-07 10:21 Alice asked MainAgent to update player movement.
2026-06-07 10:22 MainAgent proposed a write to res://scripts/player_controller.gd.
2026-06-07 10:24 Alice approved and kept the change set.
```

This is useful for teams, debugging, and accountability.

## Existing Basis

- `AIChangeSetStore` records file change sets.
- `AIAgentSession` persists conversation messages and metadata.
- Tool execution results are written back into the message chain.
- `editor/user_system` tracks current user session.

## Proposed Design

Extend AI events with user identity metadata when a human triggers an action.

Example metadata:

```json
{
  "user_id": "...",
  "user_name": "Alice",
  "session_id": "...",
  "change_set_id": "...",
  "action": "approve_change"
}
```

Agent-generated events should remain agent-attributed, but include the initiating user and session when available.

## Audit Scope

First version should audit:

- message submission
- plan creation and completion
- tool approval or denial
- write-mode tool execution
- keep/revert change set
- settings changes later

## Acceptance Criteria

- Human-triggered AI events include current user metadata when logged in.
- Audit data can be reconstructed from session metadata, tool results, and change set metadata.
- Offline or anonymous users still produce local audit entries with a fallback identity.

## Risks

- Audit logs can contain sensitive project details. Export and cloud sync need privacy review.
- User identity should not be trusted for security unless backed by server-side authorization.

## First Implementation Step

Add a helper that enriches AI session and change set metadata with current user session data when available.
