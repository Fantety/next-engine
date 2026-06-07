# NEXT Asset Registry and Reference Panel

## Purpose

Make generated and user-provided assets visible as project objects instead of hidden side effects of AI tasks.

## User Experience

The user opens an "Assets" section in NEXT and sees:

```text
Generated Assets
- asset_001 res://scripts/player_controller.gd
  Source: Script Agent
  Milestone: Core Movement
  Used by: task_001, task_003

- asset_002 res://shaders/hit_flash.gdshader
  Source: Shader Agent
  Milestone: Core Movement
  Used by: task_004

User Assets
- asset_010 res://art/player.png
  Protected from agent edits
```

## Existing Basis

- `AINextProjectState` already contains `AINextAssetRecord` and `register_asset()`.
- NEXT task completion already has `output_paths`.
- Planning documents already describe asset registration and protected user assets.

## Proposed Design

Productize the existing asset records as a visible registry.

Each asset record should answer:

- what is this asset?
- who created or imported it?
- which milestone established it?
- which task produced it?
- which tasks reference it?
- can agents edit it?
- is it a baseline asset?

## Data Model

The existing asset record may need these fields if not already present:

- id
- path
- source
- protected_from_agent_edits
- parent_asset_id
- baseline_milestone_id
- created_by_agent_id
- created_by_task_id
- referenced_by_task_ids

If the first version cannot infer references reliably, it can display produced assets first and add references later.

## UI Placement

Add a foldable section to `AINextPanel` after Tasks or after Acceptance Summary.

Recommended controls:

- compact asset list
- filter: All, Generated, User, Protected
- click an asset to inspect details
- later: open resource, copy path, mark protected

## Acceptance Criteria

- Output paths from completed tasks are visible in the asset registry.
- Protected user assets are clearly labeled.
- Locked milestones can mark assets as baseline outputs.
- Asset records persist with the NEXT workflow.

## Risks

- Asset references are hard to infer from arbitrary scene and script content. Avoid pretending references are complete in first version.
- Binary imported files should not be edited by agents unless explicitly allowed.

## First Implementation Step

Register task `output_paths` as generated assets when a NEXT task completes, then add a read-only asset panel.
