# NEXT Change Sandbox and Rollback

## Purpose

Let users treat all safe AI file changes from a NEXT milestone as one reviewable change package. This gives users confidence to let agents modify scripts, shaders, and text resources.

## User Experience

After a milestone finishes, the acceptance panel shows:

```text
Change package:
- Safe rollback available: 2 changes
- Manual inspection needed: 1 change

Safe rollback:
- res://scripts/player_controller.gd
- res://shaders/player_flash.gdshader

Manual inspection:
- res://scenes/player.tscn
```

If the user clicks rollback:

```text
The following files will be restored:
- res://scripts/player_controller.gd
- res://shaders/player_flash.gdshader

The following files will not be restored automatically:
- res://scenes/player.tscn

[Confirm Rollback] [Cancel]
```

## Existing Basis

- `AIChangeSetStore` supports pending change sets, keep, and revert.
- Script and shader editing services already use change sets in review mode.
- `AIToolExecutionContext` has session and tool call identifiers.
- NEXT session has workflow id, milestone id, task id, and agent run id that can be recorded in metadata.

## Proposed Design

Use `AIChangeSetStore` as the safe rollback engine. NEXT should not invent a second rollback mechanism for text files.

Add NEXT metadata to change sets:

```json
{
  "workflow_id": "...",
  "milestone_id": "...",
  "task_id": "...",
  "agent_id": "script_agent",
  "agent_run_id": "..."
}
```

The milestone change package is a filtered view of change sets that share workflow and milestone metadata.

## Rollback Boundary

First version should automatically roll back only changes with safe old/new text records in `AIChangeSetStore`.

Included:

- GDScript files
- GDShader files
- text resources that participate in change sets

Not automatically included in first version:

- scene edits without old/new change set records
- binary resources
- imported assets
- editor state changes

These untracked changes should appear as warnings or timeline records, not as rollback targets.

## Package Actions

Recommended first actions:

- View package
- Revert all safe pending change sets in package
- Keep all safe pending change sets in package

Partial keep/revert can come later if the package view links to individual change sets.

## Acceptance Criteria

- NEXT-generated safe change sets can be grouped by milestone.
- User can see which files can and cannot be rolled back.
- Confirmed rollback calls the existing `AIChangeSetStore::revert_change_set()` path.
- Rollback result is recorded in `AINextEventLog`.
- A failed rollback reports the exact file and reason.

## Risks

- If scene tools mutate `.tscn` without change sets, claiming full rollback would be unsafe.
- If current file content no longer matches the expected new text, rollback should fail rather than overwrite user work.

## First Implementation Step

Thread NEXT workflow and task metadata into tool execution context or change set metadata, then add session methods to list and revert active milestone change sets.
