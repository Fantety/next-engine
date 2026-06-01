# Task Plan: Fix NEXT Agent Streaming and Blocking Failures

## Goal
Fix NEXT agent operations so planning, task execution, review, and feedback planning do not block the main thread, and so provider streaming tool arguments finalize without parse errors.

## Phases
- [complete] Phase 1: Trace runtime, HTTP streaming, and milestone tool-call parsing paths.
- [complete] Phase 2: Identify root cause and compare against existing working streaming/tool parsing patterns across all NEXT agent paths.
- [complete] Phase 3: Add failing regression tests that reproduce streaming parse failure and NEXT main-thread blocking behavior.
- [complete] Phase 4: Implement the smallest targeted fix.
- [complete] Phase 5: Run focused verification and summarize results.
- [complete] Phase 6: Fix NEXT panel feedback, milestone switching, per-task execution, and active milestone progression.
- [complete] Phase 7: Extract NEXT sub-agent logic from `AIAgentNextSession` into dedicated `AIAgentBase` subclasses under `editor/ai_component/next/agents`.
- [complete] Phase 8: Design NEXT workflow persistence, interruption, and resume infrastructure based on `AIAgentSession`.
- [complete] Phase 9: Implement NEXT workflow persistence core with TDD: snapshot/store, multi-workflow session APIs, interruption checkpoints, stale-result guards, sub-agent conversation resume, and minimal workflow management UI.
- [complete] Phase 10: Fix NEXT planning freeze and partial milestone restore by tracing `ai_next.manage_project` writes and adding durable mid-run persistence.
- [complete] Phase 11: Comprehensive NEXT stutter audit: runtime callbacks, persistence writes, UI refresh, and main-thread tool execution.

## Decisions
- Preserve unrelated user/worktree changes.
- Prefer existing engine patterns and tests over new infrastructure.
- Keep the public NEXT session workflow stable while moving specialized agent setup into focused classes.
- NEXT persistence should be treated as workflow infrastructure, not UI-only behavior.

## Errors Encountered
| Error | Attempt | Resolution |
|---|---|---|
| `uv run python ...session-catchup.py` failed because the repository `pyproject.toml` has no `[project]` table. | Planning context recovery | Read existing planning files directly and continued with repository inspection. |
| Broad PowerShell pipeline reads intermittently failed with `windows sandbox: spawn setup refresh`. | Reading long test snippets | Switched to narrower `rg` and `Get-Content -TotalCount` reads. |
