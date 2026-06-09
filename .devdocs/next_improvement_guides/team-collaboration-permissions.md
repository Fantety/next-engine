# Team Collaboration Permissions

## Purpose

Support team workflows where different users have different authority over AI actions.

## User Experience

In a team project:

- designers may ask questions and run planning
- reviewers may inspect and approve changes
- trusted developers may allow write mode
- owners may configure models, MCP, Rules, and Skills

The UI disables actions the current user cannot perform and explains why.

## Existing Basis

- User session and authentication code exists under `editor/user_system`.
- AI tool permissions already support allow, ask, and deny.
- Agent profiles already define behavior boundaries such as plan, review, and write.
- Change review already provides a place for human approval.

## Proposed Design

Add a user permission layer above AI actions.

Permission examples:

```text
ai.ask
ai.plan
ai.write
ai.review
ai.settings.models
ai.settings.mcp
ai.settings.rules
ai.settings.skills
```

Tool permission checks should consider both agent profile and current user permissions.

## UI Behavior

- Disable unavailable actions.
- Show tooltip reason, such as "Requires ai.write permission."
- Record denied attempts in audit logs.

## Acceptance Criteria

- Current user role can restrict AI write actions.
- Model, MCP, Rules, and Skills settings can be permission-gated.
- Permission denial is visible and non-destructive.
- Existing single-user behavior remains unchanged when team permissions are disabled.

## Risks

- Permission logic can become fragmented if each UI checks it separately.
- Tool-level permissions must still be enforced even if UI buttons are hidden.

## First Implementation Step

Define a small AI permission service that answers whether the current user can perform named AI actions. Integrate it with one high-value action, such as approving write-mode tool execution.
