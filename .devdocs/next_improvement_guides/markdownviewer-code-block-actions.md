# MarkdownViewer Code Block Actions

## Purpose

Make code blocks in AI reports actionable while keeping file modifications explicit and reviewable.

## User Experience

For a GDScript code block, the viewer can show small actions:

```text
[Copy] [Create File] [Compare With File]
```

For a code block tied to an existing file, the user can compare it with the current file before applying changes.

## Existing Basis

- `MarkdownViewer` already parses and renders markdown content.
- `MarkdownViewerCodeHighlighter` exists for syntax highlighting.
- `MarkdownViewer` already supports a code block Copy action through `code_copy_enabled`, clipboard integration, and the `code_block_copied` signal.
- AI change review and diff viewer already exist.
- Script and shader tools already support reviewable file writes.

## Proposed Design

Build on the existing code block Copy action by adding optional code block action metadata and UI for file-aware actions.

Already available:

- Copy code block

Next version:

- Show language and line count more explicitly when useful
- Compare code block with selected file
- Create a file through review/change-set path
- Apply to file only through explicit confirmation

## Safety Rules

- Never auto-apply code from markdown.
- File writes should go through existing review/change-set mechanisms where possible.
- For generated reports, actions should be disabled unless the report explicitly marks the block as actionable.

## Acceptance Criteria

- User can continue copying code blocks from MarkdownViewer.
- GDScript and GDShader blocks keep syntax highlighting.
- Apply/compare actions do not bypass AI review safeguards.
- Code block UI does not clutter normal prose-heavy reports.

## Risks

- Inline action buttons inside every code block may clutter the UI.
- Applying code without context can overwrite user work. Keep application flows explicit.

## First Implementation Step

Add file-aware code block metadata and a Compare With File action. Keep Create File and Apply actions behind explicit review/change-set flows.
