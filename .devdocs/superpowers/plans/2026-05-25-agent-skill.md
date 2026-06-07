# Agent Skill Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add safe Prompt/Context AgentSkill support with progressive disclosure through a skill index and read-only `agent.activate_skill` tool.

**Architecture:** Create an isolated `editor/ai_component/skills` module for settings, skill index context, and activation. Wire it into `AIAgentSession`, `AIAgentProfile`, and the existing AI Settings dialog without changing Runtime or Provider protocols.

**Tech Stack:** Godot/NEXT Engine C++, EditorSettings, AIContextProvider, AITool, SCons, editor test runner.

---

## Reference Spec

- `docs/superpowers/specs/2026-05-25-agent-skill-design.md`

## File Structure

- Create `editor/ai_component/skills/SCsub`: builds the new module.
- Create `editor/ai_component/skills/ai_skill_settings.h/.cpp`: owns `AISkillConfig`, `EditorSettings` persistence, validation, and test hooks.
- Create `editor/ai_component/skills/ai_skill_context_provider.h/.cpp`: emits the enabled skill index context document.
- Create `editor/ai_component/skills/ai_activate_skill_tool.h/.cpp`: read-only activation tool that returns full configured skill content.
- Create `editor/ai_component/ui/ai_skill_dialog.h/.cpp`: simple add/edit dialog for prompt/context skills.
- Create `editor/ai_component/ui/ai_settings_skills_page.h/.cpp`: settings page replacing the placeholder.
- Modify `editor/ai_component/SCsub`: include `skills/SCsub`.
- Modify `editor/ai_component/agent/ai_agent_profile.cpp`: add `agent.activate_skill` to read-only tools.
- Modify `editor/ai_component/agent/ai_agent_session.h/.cpp`: instantiate skill index provider, collect skill index context, and register activation tool.
- Modify `editor/ai_component/ui/ai_agent_settings_dialog.h/.cpp`: replace Skills placeholder with `AISettingsSkillsPage`, save skill settings, and expose test helpers.
- Modify `editor/register_editor_types.cpp`: include and register new skill classes.
- Modify `tests/editor/test_ai_agent_tools.cpp`: add skill settings/context/tool/profile tests.
- Modify `tests/editor/test_ai_model_settings.cpp`: add AI Settings Skills page smoke tests.

---

### Task 1: Add Skill Settings Model

**Files:**
- Create: `editor/ai_component/skills/SCsub`
- Create: `editor/ai_component/skills/ai_skill_settings.h`
- Create: `editor/ai_component/skills/ai_skill_settings.cpp`
- Modify: `editor/ai_component/SCsub`
- Test: `tests/editor/test_ai_agent_tools.cpp`

- [ ] **Step 1: Write failing settings CRUD test**

Add includes:

```cpp
#include "editor/ai_component/skills/ai_skill_settings.h"
```

Add test:

```cpp
TEST_CASE("[Editor][AI] Skill settings manage prompt context skills") {
	Array original_skills = AISkillSettings::get_skill_storage_for_test();
	AISkillSettings::clear_skills_for_test();

	const String skill_id = AISkillSettings::add_skill("TDD", "Use when implementing changes.", "Write tests first.", true);
	CHECK(!skill_id.is_empty());

	Vector<AISkillConfig> skills = AISkillSettings::get_skills(false);
	REQUIRE(skills.size() == 1);
	CHECK(skills[0].id == skill_id);
	CHECK(skills[0].display_name == "TDD");
	CHECK(skills[0].description == "Use when implementing changes.");
	CHECK(skills[0].content == "Write tests first.");
	CHECK(skills[0].kind == "prompt_context");
	CHECK(skills[0].enabled);

	CHECK(AISkillSettings::update_skill(skill_id, "TDD Updated", "Use when changing behavior.", "Updated body.", false));
	AISkillConfig updated = AISkillSettings::get_skill(skill_id);
	CHECK(updated.display_name == "TDD Updated");
	CHECK(updated.description == "Use when changing behavior.");
	CHECK(updated.content == "Updated body.");
	CHECK_FALSE(updated.enabled);
	CHECK(AISkillSettings::get_skills(true).is_empty());

	CHECK(AISkillSettings::remove_skill(skill_id));
	CHECK(AISkillSettings::get_skills(false).is_empty());

	AISkillSettings::set_skill_storage_for_test(original_skills);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[AI]*"
```

Expected: compile failure because `AISkillSettings` does not exist.

- [ ] **Step 3: Implement settings module**

Implement `AISkillConfig` with fields from the spec. Implement static helpers:

- `add_skill(display_name, description, content, enabled = true)`
- `add_skill_config(config)`
- `update_skill(skill_id, display_name, description, content, enabled)`
- `update_skill_config(config)`
- `remove_skill(skill_id)`
- `set_skill_enabled(skill_id, enabled)`
- `get_skill(skill_id)`
- `get_skills(enabled_only = false)`
- `is_supported_kind(kind)`
- `normalize_kind(kind)`
- test storage helpers

Use `EditorSettings` key `ai_agent/skills`. Generate IDs with `skill:<normalized-name>:<ticks>:<rand>`. Normalize unsupported or empty kind to `prompt_context` when creating normal UI entries, but preserve explicit unsupported `kind` in `set_skill_storage_for_test()` so activation rejection can be tested later.

- [ ] **Step 4: Add build wiring**

Add:

```python
SConscript("skills/SCsub")
```

to `editor/ai_component/SCsub`.

- [ ] **Step 5: Run test to verify it passes**

Run the same AI test command. Expected: the new Skill settings test passes.

- [ ] **Step 6: Commit**

```powershell
git add editor/ai_component/SCsub editor/ai_component/skills tests/editor/test_ai_agent_tools.cpp
git commit -m "feat(ai): add agent skill settings model"
```

---

### Task 2: Add Skill Index Context Provider

**Files:**
- Create: `editor/ai_component/skills/ai_skill_context_provider.h`
- Create: `editor/ai_component/skills/ai_skill_context_provider.cpp`
- Modify: `tests/editor/test_ai_agent_tools.cpp`

- [ ] **Step 1: Write failing context tests**

Add include:

```cpp
#include "editor/ai_component/skills/ai_skill_context_provider.h"
```

Add test:

```cpp
TEST_CASE("[Editor][AI] Skill context provider exposes only enabled skill metadata") {
	Array original_skills = AISkillSettings::get_skill_storage_for_test();
	AISkillSettings::clear_skills_for_test();

	AISkillSettings::add_skill("TDD", "Use when implementing changes.", "SECRET FULL BODY", true);
	AISkillSettings::add_skill("Disabled", "Hidden trigger.", "Hidden body.", false);

	Ref<AISkillIndexContextProvider> provider;
	provider.instantiate();
	Array context = provider->collect_context();
	REQUIRE(context.size() == 1);

	Dictionary doc = context[0];
	CHECK(String(doc["title"]) == "Available Agent Skills");
	String content = doc["content"];
	CHECK(content.contains("TDD"));
	CHECK(content.contains("Use when implementing changes."));
	CHECK(content.contains("agent.activate_skill"));
	CHECK(content.contains("prompt/context"));
	CHECK_FALSE(content.contains("SECRET FULL BODY"));
	CHECK_FALSE(content.contains("Disabled"));

	AISkillSettings::set_skill_storage_for_test(original_skills);
}
```

- [ ] **Step 2: Run test to verify it fails**

Expected: compile failure because `AISkillIndexContextProvider` does not exist.

- [ ] **Step 3: Implement context provider**

Create an `AIContextDocument` with:

- `title = "Available Agent Skills"`
- `source = "ai_agent/skills"`
- compact text containing safety boundary and one line per enabled skill
- no full skill content

Return an empty array when no enabled skills exist.

- [ ] **Step 4: Run test to verify it passes**

Run AI tests. Expected: context provider test passes.

- [ ] **Step 5: Commit**

```powershell
git add editor/ai_component/skills tests/editor/test_ai_agent_tools.cpp
git commit -m "feat(ai): expose agent skill index context"
```

---

### Task 3: Add Read-Only Skill Activation Tool

**Files:**
- Create: `editor/ai_component/skills/ai_activate_skill_tool.h`
- Create: `editor/ai_component/skills/ai_activate_skill_tool.cpp`
- Modify: `editor/ai_component/agent/ai_agent_profile.cpp`
- Modify: `tests/editor/test_ai_agent_tools.cpp`

- [ ] **Step 1: Write failing activation tests**

Add include:

```cpp
#include "editor/ai_component/skills/ai_activate_skill_tool.h"
```

Add tests:

```cpp
TEST_CASE("[Editor][AI] Activate skill tool returns enabled prompt context content") {
	Array original_skills = AISkillSettings::get_skill_storage_for_test();
	AISkillSettings::clear_skills_for_test();

	const String skill_id = AISkillSettings::add_skill("TDD", "Use when implementing changes.", "Full skill body.", true);
	Ref<AIActivateSkillTool> tool;
	tool.instantiate();

	Dictionary args;
	args["skill_id"] = skill_id;
	AIToolResult result = tool->execute(args);
	CHECK_FALSE(result.is_error());
	CHECK(result.content.contains("Full skill body."));
	CHECK(String(result.metadata["skill_id"]) == skill_id);
	CHECK(String(result.metadata["skill_kind"]) == "prompt_context");
	CHECK(String(result.metadata["tool_origin"]) == "agent_skill");

	AISkillSettings::set_skill_storage_for_test(original_skills);
}

TEST_CASE("[Editor][AI] Activate skill tool rejects missing disabled and unsupported skills") {
	Array original_skills = AISkillSettings::get_skill_storage_for_test();
	AISkillSettings::clear_skills_for_test();

	const String disabled_id = AISkillSettings::add_skill("Disabled", "Hidden trigger.", "Hidden body.", false);

	Dictionary unsupported;
	unsupported["id"] = "skill:exec";
	unsupported["display_name"] = "Executable";
	unsupported["description"] = "Should not run.";
	unsupported["content"] = "run me";
	unsupported["kind"] = "executable";
	unsupported["enabled"] = true;
	Array storage;
	storage.push_back(unsupported);
	AISkillSettings::set_skill_storage_for_test(storage);

	Ref<AIActivateSkillTool> tool;
	tool.instantiate();

	Dictionary missing_args;
	missing_args["skill_id"] = "missing";
	CHECK(tool->execute(missing_args).is_error());

	Dictionary disabled_args;
	disabled_args["skill_id"] = disabled_id;
	AISkillSettings::clear_skills_for_test();
	AISkillSettings::add_skill("Disabled", "Hidden trigger.", "Hidden body.", false);
	CHECK(tool->execute(disabled_args).is_error());

	Dictionary unsupported_args;
	unsupported_args["skill_id"] = "skill:exec";
	AISkillSettings::set_skill_storage_for_test(storage);
	CHECK(tool->execute(unsupported_args).is_error());

	AISkillSettings::set_skill_storage_for_test(original_skills);
}

TEST_CASE("[Editor][AI] Agent profiles allow activating prompt context skills") {
	CHECK(AIAgentProfile::get_plan_profile().allows_tool("agent.activate_skill"));
	CHECK(AIAgentProfile::get_build_profile().allows_tool("agent.activate_skill"));
	CHECK(AIAgentProfile::get_write_profile().allows_tool("agent.activate_skill"));
	CHECK(AIAgentProfile::get_review_profile().allows_tool("agent.activate_skill"));
}
```

When implementing, clean up the disabled test to keep IDs deterministic: prefer inserting explicit dictionaries for disabled and unsupported cases through `set_skill_storage_for_test()`.

- [ ] **Step 2: Run test to verify it fails**

Expected: compile failure for missing tool and profile permission assertion failure once compiled.

- [ ] **Step 3: Implement activation tool**

Implement `AIActivateSkillTool`:

- `get_name() -> "agent.activate_skill"`
- Description explains it loads full content for an enabled prompt/context AgentSkill.
- Parameters schema requires `skill_id`.
- `execute()` rejects empty ID, missing skill, disabled skill, and `kind != "prompt_context"`.
- Returns clear content header plus full stored content.
- Metadata includes `skill_id`, `skill_name`, `skill_kind`, and `tool_origin`.

- [ ] **Step 4: Add profile permission**

Add `agent.activate_skill` to `_add_read_only_tools()` in `ai_agent_profile.cpp`.

- [ ] **Step 5: Run test to verify it passes**

Run AI tests. Expected: activation and profile tests pass.

- [ ] **Step 6: Commit**

```powershell
git add editor/ai_component/skills editor/ai_component/agent/ai_agent_profile.cpp tests/editor/test_ai_agent_tools.cpp
git commit -m "feat(ai): add agent skill activation tool"
```

---

### Task 4: Wire Skills Into Agent Session

**Files:**
- Modify: `editor/ai_component/agent/ai_agent_session.h`
- Modify: `editor/ai_component/agent/ai_agent_session.cpp`
- Modify: `editor/register_editor_types.cpp`
- Modify: `tests/editor/test_ai_agent_runtime.cpp` or `tests/editor/test_ai_agent_tools.cpp`

- [ ] **Step 1: Write failing session registry test**

Add a test that constructs `AIAgentSession`, gets its tool registry, and checks `agent.activate_skill` is registered. If session construction is too heavy for the test environment, add the assertion to an existing session test that already constructs it.

```cpp
TEST_CASE("[Editor][AI] Agent session registers skill activation tool") {
	AIAgentSession session;
	Ref<AIToolRegistry> registry = session.get_tool_registry();
	REQUIRE(registry.is_valid());
	CHECK(registry->has_tool("agent.activate_skill"));
}
```

- [ ] **Step 2: Run test to verify it fails**

Expected: registry does not contain `agent.activate_skill`.

- [ ] **Step 3: Wire session**

In `AIAgentSession`:

- include `ai_skill_context_provider.h` and `ai_activate_skill_tool.h`
- add `Ref<AISkillIndexContextProvider> skill_context`
- instantiate it in constructor
- append `skill_context->collect_context()` in `_collect_context()`
- register `AIActivateSkillTool` in `_configure_tool_runtime()`

- [ ] **Step 4: Register classes**

In `editor/register_editor_types.cpp`, include and register:

- `AISkillIndexContextProvider`
- `AIActivateSkillTool`

Register `AISkillSettings` only if it is an `Object`/`RefCounted`; if it stays static-only, do not register it.

- [ ] **Step 5: Run test to verify it passes**

Run AI tests. Expected: session registry test passes.

- [ ] **Step 6: Commit**

```powershell
git add editor/ai_component/agent/ai_agent_session.* editor/register_editor_types.cpp tests/editor
git commit -m "feat(ai): wire agent skills into session"
```

---

### Task 5: Build Skills Settings UI

**Files:**
- Create: `editor/ai_component/ui/ai_skill_dialog.h`
- Create: `editor/ai_component/ui/ai_skill_dialog.cpp`
- Create: `editor/ai_component/ui/ai_settings_skills_page.h`
- Create: `editor/ai_component/ui/ai_settings_skills_page.cpp`
- Modify: `editor/ai_component/ui/ai_agent_settings_dialog.h`
- Modify: `editor/ai_component/ui/ai_agent_settings_dialog.cpp`
- Modify: `editor/register_editor_types.cpp`
- Test: `tests/editor/test_ai_model_settings.cpp`

- [ ] **Step 1: Write failing UI smoke test**

Add includes:

```cpp
#include "editor/ai_component/skills/ai_skill_settings.h"
```

Add test:

```cpp
TEST_CASE("[Editor][AI] Agent settings dialog exposes skill rows") {
	EditorSettings *settings = EditorSettings::get_singleton();
	REQUIRE(settings != nullptr);

	Array original_skills = AISkillSettings::get_skill_storage_for_test();
	AISkillSettings::clear_skills_for_test();

	AIAgentSettingsDialog dialog;
	dialog.build_for_test();
	CHECK(dialog.get_skill_table_row_count_for_test() == 0);
	dialog.add_skill_for_test("TDD", "Use when changing behavior.", "Write tests first.", true);
	CHECK(dialog.get_skill_table_row_count_for_test() == 1);
	dialog.save_settings_for_test();

	AISkillSettings::set_skill_storage_for_test(original_skills);
}
```

- [ ] **Step 2: Run test to verify it fails**

Expected: missing dialog methods and UI classes.

- [ ] **Step 3: Implement `AISettingsSkillsPage`**

Follow the visual structure of `AISettingsMCPPage`:

- title: `Skills`
- safety description explaining prompt/context-only behavior
- add button
- table rows with enabled checkbox, display name, kind, description, edit/delete actions
- test helpers: `build_for_test()`, `get_skill_table_row_count_for_test()`, `add_skill_for_test(...)`

Do not add import-from-path in v1.

- [ ] **Step 4: Implement `AISkillDialog`**

Fields:

- name `LineEdit`
- description `TextEdit` or `LineEdit`
- content `TextEdit`
- enabled `CheckBox`
- read-only kind display set to `prompt_context`

Reject empty name or empty content by showing a small error label or keeping the dialog open.

- [ ] **Step 5: Replace placeholder**

In `AIAgentSettingsDialog`:

- add `AISettingsSkillsPage *skills_page`
- include new page
- create page instead of placeholder
- connect `settings_changed` to `_save_skill_settings()`
- add signal `ai_skill_settings_changed` only if needed; otherwise `ai_settings_changed` is enough because session reads settings on each run and no background discovery is needed
- add test helper methods

- [ ] **Step 6: Register UI classes**

Register `AISettingsSkillsPage` and `AISkillDialog` in `editor/register_editor_types.cpp`.

- [ ] **Step 7: Run test to verify it passes**

Run AI tests. Expected: UI smoke test passes.

- [ ] **Step 8: Commit**

```powershell
git add editor/ai_component/ui editor/register_editor_types.cpp tests/editor/test_ai_model_settings.cpp
git commit -m "feat(ai): add agent skill settings UI"
```

---

### Task 6: Documentation And Final Verification

**Files:**
- Modify: `docs/ai_agent_architecture.md`
- Modify: `README.md` if the existing feature list needs a short update

- [ ] **Step 1: Update architecture docs**

Add a short AgentSkill section:

- skill index context
- `agent.activate_skill`
- prompt/context-only safety boundary
- future extension fields

- [ ] **Step 2: Run formatting/checks**

Run:

```powershell
git diff --check
```

Expected: no whitespace errors.

- [ ] **Step 3: Run focused AI tests**

Run:

```powershell
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[AI]*"
```

Expected: all AI tests pass.

- [ ] **Step 4: Build if test binary required recompilation was incomplete**

If the test command does not rebuild or fails due to stale binary, run:

```powershell
scons platform=windows target=editor tests=yes
```

Then rerun the AI tests.

- [ ] **Step 5: Commit docs and final fixes**

```powershell
git add docs README.md
git commit -m "docs(ai): document agent skill flow"
```

- [ ] **Step 6: Final status**

Report:

- files changed
- tests run and exact result
- any limitations, especially that v1 does not execute scripts or auto-grant tools
