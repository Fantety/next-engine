# MarkdownViewer Godot Resource Links

## Purpose

Let users navigate from AI reports directly to Godot project resources referenced in markdown.

## User Experience

When a report contains:

```text
res://scripts/player_controller.gd
res://scenes/player.tscn
```

The user can click the resource path to open it in the appropriate editor.

## Existing Basis

- `MarkdownViewer` already has link-related behavior controls.
- Godot editor APIs can open resources and scripts.
- AI reports and task outputs already contain `res://` paths.

## Proposed Design

Add safe handling for Godot resource links:

- detect `res://` links
- validate that the resource path belongs to the current project
- open scripts in script editor
- open scenes/resources through the editor filesystem or inspector
- ignore or warn on missing resources

Do not treat arbitrary external URLs as trusted.

## Link Forms

Support explicit markdown links first:

```markdown
[player_controller.gd](res://scripts/player_controller.gd)
```

Later, optionally auto-link plain `res://` text.

## Acceptance Criteria

- Clicking a valid `res://` link opens the resource in the editor.
- Missing resources produce a non-blocking warning.
- External links remain disabled unless explicitly enabled.
- Resource link handling can be reused by AI reports and other editor markdown views.

## Risks

- Auto-linking plain text can produce false positives.
- Opening resources must happen on the editor/main thread.

## First Implementation Step

Add a resource-link callback path to `MarkdownViewer`, then wire it in NEXT report views for explicit markdown links.
