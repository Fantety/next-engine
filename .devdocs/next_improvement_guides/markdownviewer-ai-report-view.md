# MarkdownViewer as AI Report View

## Purpose

Use `MarkdownViewer` as the standard renderer for AI-generated and AI-derived reports inside the editor.

## User Experience

Instead of scattered labels, the user sees readable reports for:

- NEXT milestone acceptance summaries
- review findings
- rollback summaries
- task result details
- error analysis
- generated implementation notes

These reports support headings, lists, code blocks, and links in a consistent visual style.

## Existing Basis

- `scene/gui/MarkdownViewer` exists as a reusable GUI node.
- NEXT already uses `MarkdownViewer` for review findings and task inspector content.
- AI UI has `AIMarkdownRenderer` and markdown labels for chat messages.

## Proposed Design

Define MarkdownViewer as the preferred report surface for structured AI reports.

Report builders should generate markdown from structured data:

```text
## Milestone: Core Movement

### Completed Tasks
- Create player movement script
- Bind script to Player node

### Outputs
- res://scripts/player_controller.gd

### Warnings
- Scene save is recorded but not automatically revertible.
```

The model does not need to generate all reports. Local code should generate reports when structured data is available.

## Candidate Reports

- NEXT acceptance summary
- NEXT change package summary
- timeline event detail
- context budget diagnostics
- MCP server diagnostics

## Acceptance Criteria

- New report views use MarkdownViewer rather than custom ad hoc label trees when rich text is needed.
- Reports are generated from structured data where possible.
- Remote images are disabled by default for AI reports.
- Links are either disabled or handled safely.

## Risks

- Markdown can become an excuse to avoid proper UI controls. Actions should remain buttons, not markdown links.
- Generated markdown must avoid untrusted remote content by default.

## First Implementation Step

Create a local formatter for NEXT milestone acceptance markdown and render it in a new acceptance section.
