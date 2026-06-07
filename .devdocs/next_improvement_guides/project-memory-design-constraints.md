# Project Memory and Design Constraints

## Purpose

Maintain an AI-readable memory for each Godot project so Normal mode and NEXT mode can follow project-specific rules without relying only on recent chat history.

## User Experience

The user opens an "AI Project Memory" settings page and sees editable sections:

- Game vision
- Gameplay rules
- Scene structure
- Resource naming rules
- Code style
- Protected files and folders
- Notes for future agents

During later AI requests, the user does not need to repeat these constraints. Agents automatically receive the relevant project memory.

## Existing Basis

- Context providers already exist under `editor/ai_component/context`.
- Rules and Skills already have settings pages and context providers.
- NEXT workflow context builder already combines project state and event log into agent context.
- `AIContextManager` already budgets and truncates context documents.

## Proposed Design

Add a project-scoped memory document stored under the same project scope key used by conversations and NEXT workflows.

The memory should be structured, not one giant text field:

```json
{
  "game_vision": "...",
  "style_rules": "...",
  "code_rules": "...",
  "scene_rules": "...",
  "protected_paths": ["res://addons/", "res://third_party/"],
  "agent_notes": "..."
}
```

Normal mode and NEXT mode should consume it through a shared context provider, such as `AIProjectMemoryContextProvider`.

## Behavior Rules

- Project memory is user-authored or user-approved.
- Agents can suggest memory updates, but should not silently rewrite memory.
- Protected paths should also be exposed to tool permission logic later.
- Memory should be concise. Large design docs should be linked or summarized, not pasted entirely.

## UI Placement

Add a page under the AI settings dialog, near Rules and Skills.

Recommended controls:

- multiline text edits for memory sections
- protected path list with add/remove
- "Suggest from current project" later, not required in first version

## Acceptance Criteria

- User can create, edit, save, and reload project memory.
- Normal mode includes project memory in context.
- NEXT mode includes project memory in planning and task execution context.
- Context metadata reports whether memory was included or truncated.

## Risks

- If memory becomes too long, it can crowd out task-specific context.
- If agents can modify memory automatically, bad assumptions may become permanent.

## First Implementation Step

Create storage and context provider for project memory. Add UI after the provider works in tests.
