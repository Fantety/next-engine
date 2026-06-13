# Task Plan: Agent README Rewrite

## Goal
Understand `editor/agent_v1` and `editor/agent_ui`, then rewrite the relevant README so it accurately describes the current implementation.

## Current Phase
Phase 12 complete

## Phases

### Phase 1: Requirements & Discovery
- [x] Capture user request
- [x] Identify README target
- [x] Inspect existing agent code and docs
- [x] Document findings in `findings.md`
- **Status:** complete

### Phase 2: Structure & Content Plan
- [x] Decide README structure
- [x] Map implementation details to reader-facing sections
- **Status:** complete

### Phase 3: Rewrite README
- [x] Update README content
- [x] Keep terminology aligned with existing code
- **Status:** complete

### Phase 4: Verification
- [x] Review changed README
- [x] Check git diff
- **Status:** complete

### Phase 5: Delivery
- [x] Summarize changes and verification
- **Status:** complete

### Phase 6: Skill/MCP Chain Inspection
- [x] Trace Skill configuration, UI, service, context, and tool registration chain
- [x] Trace MCP configuration, UI, service, discovery, resource, prompt, and tool registration chain
- [x] Identify relevant tests
- **Status:** complete

### Phase 7: Skill/MCP Verification
- [x] Run targeted Skill tests
- [x] Run targeted MCP tests
- [x] Record pass/fail evidence
- **Status:** complete

### Phase 8: Skill/MCP Result
- [x] Summarize whether Skill and MCP currently work
- [x] Note any caveats or failing areas
- **Status:** complete

### Phase 9: Agent System Prompt Design
- [x] Inspect current default prompt and `best_practices.md`
- [x] Decide injection point that preserves config overrides
- [x] Identify existing tests to extend
- **Status:** complete

### Phase 10: Prompt Tests
- [x] Add failing tests for optimized fixed prompt
- [x] Add failing tests for fixed `best_practices.md` injection
- [x] Confirm tests fail before production changes
- **Status:** complete

### Phase 11: Prompt Implementation
- [x] Update fixed Agent system prompt
- [x] Generate and append `editor/agent_v1/best_practices.md` from a build-time header
- [x] Preserve project/global agent prompt overrides
- **Status:** complete

### Phase 12: Prompt Verification
- [x] Run targeted tests
- [x] Run relevant broader Agent tests if needed
- [x] Check diff hygiene
- **Status:** complete

## Key Questions
1. Which README should be rewritten?
2. What responsibilities do `editor/agent_v1` and `editor/agent_ui` implement?
3. What setup, architecture, and extension points should the README highlight?

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| Use current code as the source of truth | User explicitly asked to understand existing modules before rewriting docs. |
| Rewrite root `README.md` | There is no README under `editor/agent_v1` or `editor/agent_ui`, and the root README already describes NEXT Engine but contains stale agent structure details. |
| Structure README around product overview, architecture, UI, tools, safety, build, and project structure | This maps current implementation to both users and developers without duplicating API docs. |
| Revise README to prioritize user-facing product explanation | User clarified the README should introduce the project from the user's perspective rather than a development/architecture perspective. |
| Verify Skill/MCP via both code-path tracing and targeted tests | User asked to confirm whether Skill and MCP can work now; static inspection alone is not enough. |
| Generate `editor/agent_v1/best_practices.gen.h` with SCons and include it in config prompt assembly | This hardcodes the bundled best-practices prompt into the program while avoiding runtime file reads. |
| Append bundled best practices after configured agent prompts | This preserves project/global/user prompt overrides while ensuring fixed Godot guidance is always present. |

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| `bin\next.windows.editor.x86_64.console.exe --test` is not built with tests enabled | 1 | Used `bin\next.windows.editor.dev.x86_64.console.exe` for targeted verification. |
| Dock status-panel test emits RID/ObjectDB leak and AIComposer theme timing warnings after passing | 1 | Recorded as a cleanup/theming caveat; doctest exit code remains 0 with all assertions passing. |
| Removing `core/os/os.h` from `ai_config_service.cpp` broke the build because later code still uses `OS` | 1 | Restored the include and rebuilt successfully. |
| Running two editor test binaries in parallel caused one startup cleanup message (`Failed to delete files`) | 1 | Reran the prompt tests serially for clean doctest evidence. |
| `best_practices.gen.h` stayed stale after `best_practices.md` changed | 1 | Added an explicit SCons dependency from `agent_v1/config` objects to the generated header. |

## Notes
- Update this plan after each phase.
