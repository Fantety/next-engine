# Agent Skill Design

## Goal

Add first-class AgentSkill support to NEXT Engine's editor-side AI Agent while preserving a strict security boundary. The first version supports only prompt/context skills with progressive disclosure: the agent sees an enabled skill index first, then calls a read-only activation tool to load the full `SKILL.md` content only when needed.

## Existing Architecture

NEXT Engine is a Godot-derived C++ editor with AI functionality concentrated under `editor/ai_component`. The current AI Agent implementation already has useful separation:

- UI lives under `editor/ai_component/ui`.
- Session orchestration lives in `editor/ai_component/agent/ai_agent_session.*`.
- Runtime function-calling lives in `editor/ai_component/agent/ai_agent_runtime.*`.
- Read-only and editing context providers live under `editor/ai_component/context`.
- Tools are registered through `AIToolRegistry` and gated by `AIAgentProfile` plus `AIToolPermissionPolicy`.
- MCP dynamic tools are loaded through a service and consumed by the session as a snapshot.

The Skills page is already present in `AIAgentSettingsDialog`, but it currently uses a placeholder page. AgentSkill should follow the existing layering: settings and UI manage configuration, context providers expose safe context, tools expose explicit runtime actions, and session only wires the pieces together.

## Product Scope

AgentSkill v1 supports `prompt_context` skills only.

A skill contains:

- `id`
- `display_name`
- `description`
- `content`
- `enabled`
- `kind`, initially constrained to `prompt_context`

The content is the equivalent of a local `SKILL.md` body: procedural instructions and reference guidance for the model. The description is the trigger surface the model uses to decide whether to activate the skill.

## Progressive Disclosure Flow

The AgentSkill flow should match common agent skill systems:

1. At request time, the session collects normal editor/project/best-practices context.
2. `AISkillIndexContextProvider` appends a compact "Available Agent Skills" context document.
3. That context document includes enabled skill IDs, names, descriptions, kind, and explicit safety constraints.
4. The model may call `agent.activate_skill` with a `skill_id`.
5. `AIActivateSkillTool` validates that the skill exists, is enabled, and has `kind == "prompt_context"`.
6. The tool returns the full skill content as a tool result.
7. The runtime gives that tool result back to the model in the normal function-calling loop.
8. The model follows the full skill content for the remaining task.

Full skill content is not injected by default. This keeps context usage predictable and lets the model opt into skills based on the name and description.

## Security Boundary

The UI and the runtime-visible skill index must state the v1 boundary clearly:

- AgentSkill currently supports prompt/context instructions only.
- AgentSkill does not execute scripts.
- AgentSkill does not launch processes.
- AgentSkill does not read arbitrary bundled resources.
- AgentSkill does not automatically grant tool permissions.
- AgentSkill does not bypass `AIAgentProfile`, `AIToolPermissionPolicy`, MCP approvals, or existing review/change-set flows.

`agent.activate_skill` is a read-only tool. It returns configured text from editor settings and metadata explaining the activated skill. It must not interpret content as code, resolve file references, or perform any side effects.

In v1, skill content comes from the stored skill configuration itself. The activation tool must not accept a file path and must not read arbitrary `SKILL.md` files from disk. Importing local skill folders can be added later as a separate settings workflow with explicit user action and path validation.

## Future Extension Points

The v1 schema should intentionally reserve fields without implementing stronger behavior:

- `kind`: v1 accepts only `prompt_context`; future values may include `tool_bundle`, `resource_reference`, or `executable`.
- `requested_tools`: future metadata for tool-bundle skills; ignored by v1.
- `capabilities`: future metadata for UI presentation and policy checks; ignored by v1.

Future stronger skill kinds must remain opt-in and policy-gated. In particular, executable skills should require a separate permission model and must not reuse the prompt/context activation path as an execution primitive.

## Module Design

Create a focused `editor/ai_component/skills` module.

`ai_skill_settings.*`

- Owns skill persistence in `EditorSettings` under `ai_agent/skills`.
- Converts between `AISkillConfig` and `Dictionary`.
- Normalizes and validates `kind`.
- Provides CRUD helpers and test storage hooks.
- Provides `get_enabled_skills()`.

`ai_skill_context_provider.*`

- Inherits `AIContextProvider`.
- Reads enabled skills from `AISkillSettings`.
- Produces one compact `AIContextDocument` containing the skill index and safety boundary.
- Does not include full skill content.

`ai_activate_skill_tool.*`

- Inherits `AITool`.
- Name: `agent.activate_skill`.
- Schema: `{ "skill_id": "string" }`.
- Validates enabled `prompt_context` skill.
- Returns full skill content with metadata such as `skill_id`, `skill_name`, `skill_kind`, and `tool_origin = "agent_skill"`.
- Returns an error if the skill is missing, disabled, or not a supported v1 kind.

## UI Design

Replace the Skills placeholder in `AIAgentSettingsDialog` with `AISettingsSkillsPage`.

The page should provide:

- A skill table/list with name, kind, enabled state, and description.
- Add, edit, delete, enable, and disable actions.
- A clear v1 safety note: "Current skills only provide prompt/context instructions. They do not execute code or grant tools."
- An editor for display name, description, and content.

The UI should not expose future stronger skill behavior as usable controls. If future fields are present internally, they remain hidden or read-only until implemented.

## Session Integration

`AIAgentSession` should:

- Own `Ref<AISkillIndexContextProvider>`.
- Append skill index context in `_collect_context()`.
- Register `AIActivateSkillTool` in `_configure_tool_runtime()`.
- Add `agent.activate_skill` to read-only profile permissions so it is visible in plan, build, review, and write modes.

No Runtime or Provider changes are required. The existing function-calling loop already supports a model calling a tool, receiving the tool result, and continuing the next provider turn.

## Testing Strategy

Add focused tests before implementation:

- `AISkillSettings` CRUD stores and restores skill configs.
- Disabled skills are omitted from the skill index.
- The skill index includes only metadata and does not include full content.
- `agent.activate_skill` returns full content for an enabled `prompt_context` skill.
- `agent.activate_skill` rejects missing, disabled, or non-`prompt_context` skills.
- Agent profiles expose `agent.activate_skill` as a read-only tool.

Existing AI runtime tests should continue to cover the function-calling loop. The new tool can be tested directly and through the registry/profile schema path without requiring a real provider.

## Non-Goals

- No script execution.
- No external process execution.
- No automatic reading of bundled `references/`, `scripts/`, or `assets/`.
- No automatic tool permission grants.
- No MCP changes.
- No Runtime protocol changes.

## Acceptance Criteria

- Users can create, edit, enable, disable, and delete prompt/context skills in AI Settings.
- Enabled skills appear in Agent context as an index with name and description only.
- The model can call `agent.activate_skill` to load one skill's full content.
- The activation tool is read-only and does not perform side effects.
- The UI and context clearly explain the current security boundary.
- The implementation keeps Skills isolated under a dedicated module and only wires into Session, Settings UI, and read-only profile permissions.
