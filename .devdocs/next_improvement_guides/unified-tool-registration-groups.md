# Unified Tool Registration Groups

## Purpose

Make built-in AI tools easier to register, audit, and expose consistently across Normal mode and NEXT agents.

## User Value

Users should experience predictable permissions:

- planning agents read project context
- script agents can edit scripts
- scene agents can edit scenes
- shader agents can edit shaders
- destructive operations require review or confirmation

Tool exposure should not depend on duplicated registration code drifting apart.

## Existing Basis

- Tool classes already live under `editor/ai_component/tools`.
- `AIToolRegistry` already provides a common runtime registry.
- Normal and NEXT agents both register project, scene, script, and shader tools.
- `AIToolPermissionPolicy` and agent profiles already control allow, ask, and deny behavior.
- `AIToolFactory` already provides shared registration helpers such as `register_shared_project_tools`, `register_scene_write_tools`, `register_script_write_tools`, `register_shader_tools`, and `register_editor_runtime_tools`.
- `AIMainAgent` and NEXT agents already use these helpers.

## Proposed Design

Continue refining the existing `AIToolFactory` registration groups instead of creating a second grouping system. The current groups cover the most important built-in project, scene, script, shader, and runtime tools.

Potential missing or under-specified groups:

```text
next_project_tools
destructive_tools
mcp_tools
asset_tools
review_only_tools
```

Agents and profiles should keep choosing groups rather than listing each tool manually. New tools should be added to exactly one primary group, with comments when a tool intentionally belongs to multiple exposure paths.

Example:

```text
Planning Agent:
- read_only_tools
- next_project_tools

Script Agent:
- read_only_tools
- script_write_tools

Scene Agent:
- read_only_tools
- scene_write_tools

Normal Write Profile:
- read_only_tools
- project_write_tools
- scene_write_tools
- script_write_tools
- shader_write_tools
```

## API Shape

Extend the existing `AIToolFactory` API where needed:

```cpp
void AIToolFactory::register_next_project_tools(AIAgentBase *p_agent, const Ref<AINextProjectState> &p_project_state);
void AIToolFactory::register_mcp_tools(AIAgentBase *p_agent, AIMCPService *p_service, AIToolPermission p_permission);
void AIToolFactory::register_destructive_tools(AIAgentBase *p_agent, AIToolPermission p_permission);
void AIToolFactory::register_asset_tools(AIAgentBase *p_agent, AIToolPermission p_permission);
```

## Acceptance Criteria

- Tool registration lists are defined once.
- Normal mode and NEXT mode use the same group helpers.
- Tests can assert which tools are exposed for each agent/profile.
- Adding a new tool requires choosing its group explicitly.
- Existing `AIToolFactory` helpers are reused rather than duplicated.

## Risks

- Some tools may fit multiple groups. Prefer conservative grouping and explicit comments.
- Permissions are still enforced separately; registration groups should not replace permission policy.

## First Implementation Step

Audit the existing `AIToolFactory` helpers and agent usage. Add tests that assert the expected tool exposure for `AIMainAgent`, Planning Agent, Script Agent, Scene Agent, Shader Agent, and Review Agent. Then add only the missing helper groups, starting with `register_next_project_tools()`.
