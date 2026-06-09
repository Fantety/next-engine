# MarkdownViewer as AI Report View

## Purpose

Use `MarkdownViewer` as the standard renderer for AI-generated and AI-derived reports inside the editor.

## User Experience

Instead of scattered labels, the user sees readable reports for:

- review findings
- rollback summaries
- tool execution details
- error analysis
- context budget diagnostics
- generated implementation notes

These reports support headings, lists, code blocks, and links in a consistent visual style.

## Existing Basis

- `scene/gui/MarkdownViewer` exists as a reusable GUI node.
- AI UI has `AIMarkdownRenderer` and markdown labels for chat messages.
- Change review and tool result messages already produce structured data that can be formatted.

## Proposed Design

Define MarkdownViewer as the preferred report surface for structured AI reports.

Report builders should generate markdown from structured data:

```text
## Change Review Summary

### Files Changed
- res://scripts/player_controller.gd

### Tool Results
- script.write completed

### Warnings
- File changes require user approval before they are kept.
```

The model does not need to generate all reports. Local code should generate reports when structured data is available.

## Candidate Reports

- change set summary
- tool execution detail
- context budget diagnostics
- MCP server diagnostics
- implementation notes generated from persisted message metadata

## Acceptance Criteria

- New report views use MarkdownViewer rather than custom ad hoc label trees when rich text is needed.
- Reports are generated from structured data where possible.
- Remote images are disabled by default for AI reports.
- Links are either disabled or handled safely.

## Risks

- Markdown can become an excuse to avoid proper UI controls. Actions should remain buttons, not markdown links.
- Generated markdown must avoid untrusted remote content by default.

## First Implementation Step

Create a local formatter for AI change review markdown and render it in a compact report section.
