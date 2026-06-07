# Context Budget Visualization

## Purpose

Help users and developers understand what the AI actually received as context, and what was omitted or truncated.

## User Experience

After a request, the AI panel can show a compact context usage summary:

```text
Context:
- History: 18 messages, 4 omitted
- Project tree: included
- Rules: included
- Skills: 2 indexed, 1 activated
- Tool results: 1 truncated
- Estimated input: 42k / 96k chars
```

For developers, this makes poor AI behavior easier to debug. If the model missed a file because context was truncated, the UI should make that visible.

## Existing Basis

- `AIContextManager` already produces `estimated_context_usage`.
- `AIAgentMessage.metadata` already stores usage and context estimate metadata.
- Token usage is already tracked in conversation and runtime results.

## Proposed Design

Expose context budget metadata in a small diagnostics panel.

First version:

- show estimated input chars
- show max input chars
- show omitted history message count
- show truncated context document count
- show truncated tool result count

Later:

- show per-provider contribution
- show exact included context document titles
- allow copying context diagnostics

## UI Placement

Normal mode:

- message metadata popover
- or compact usage row near token usage

NEXT mode:

- task inspector or activity details for the selected agent run

## Acceptance Criteria

- User can see whether context was truncated for the latest request.
- Developers can inspect context usage from persisted message metadata.
- UI remains compact and does not overwhelm normal users.

## Risks

- Exact token counts are provider-specific. Label estimates clearly as estimates.
- Too much diagnostic text can clutter the main chat UI.

## First Implementation Step

Add a reusable formatter for `estimated_context_usage`, then show it in a foldable diagnostics area for the latest assistant message or NEXT agent run.
