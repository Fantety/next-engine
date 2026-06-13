# Task Plan: Agent README Rewrite

## Goal
Understand `editor/agent_v1` and `editor/agent_ui`, then rewrite the relevant README so it accurately describes the current implementation.

## Current Phase
Phase 5

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

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|

## Notes
- Update this plan after each phase.
