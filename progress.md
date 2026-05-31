# Progress: AI Agent Milestone Streaming Failure

## 2026-05-30
- Created debugging plan and findings log.
- Preserved existing untracked `.gdignore`.
- Located runtime streaming parser, codec error site, and NEXT milestone session entry points.
- Expanded scope after user correction: all NEXT agent calls must avoid main-thread blocking, not only milestone generation.
- Added failing tests for all NEXT agent paths running off main thread and for invalid streamed provider tool arguments.
- Verified red tests: NEXT operations currently run on main thread; stream accumulator currently fails finalization; runtime currently executes parse-failed calls.
- Implemented NEXT async runner workflow and invalid tool-arguments failure path.
- Verification passing: focused red tests, `[NEXT]` tests, OpenAI-compatible tests, selected runtime tool/runner tests, build, and `git diff --check`.
- Broader `test_ai_agent_runtime.cpp` still has two unrelated failures: system prompt missing `one scene`, and existing progress callback snapshot expectations seeing later completed status.

## 2026-05-31
- Started Phase 6 for NEXT UX/state issues.
- Confirmed current UI has no operation/progress display, no disabled button refresh, non-clickable milestone rows, no per-task run action, and task inspector always shows the first active-milestone task.
- Confirmed session has no public single-task execution or runtime-progress surface for NEXT panel consumption.
- Added regression coverage for milestone selection/lock advancement, single-task execution, and runtime progress visibility during an active agent run.
- Implemented NEXT session selection, single-task run, runtime progress exposure, automatic next-milestone selection after lock, and UI updates for clickable milestones/tasks, button disabled states, spinner operation label, and activity feed.
- Verification passed: `scons platform=windows target=editor tests=yes`; `bin\next.windows.editor.x86_64.console.exe --test --test-case="*[Editor][AI][NEXT]*"`.
- Started Phase 7 for extracting NEXT sub-agents from `AIAgentNextSession` into maintainable classes under `editor/ai_component/next/agents`.
- Added a failing regression test asserting that each NEXT session agent is a dedicated NEXT subclass. Red verification: `scons platform=windows target=editor tests=yes -j4` fails because `editor/ai_component/next/agents/ai_next_planning_agent.h` does not exist yet.
- Implemented `AINextPlanningAgent`, `AINextScriptAgent`, `AINextSceneAgent`, `AINextShaderAgent`, and `AINextReviewAgent` under `editor/ai_component/next/agents`.
- Refactored `AIAgentNextSession` to store agents in an ID map, wire runtime callbacks generically, and leave workflow orchestration in the session.
- Green verification passed: `scons platform=windows target=editor tests=yes -j4`; `bin\next.windows.editor.x86_64.console.exe --test --test-case="*[Editor][AI][NEXT]*"`; `git diff --check`.
- Started Phase 8 for NEXT persistence, interruption, and resume infrastructure.
- Confirmed current NEXT project store exists but is not wired into `AIAgentNextSession` load/save paths, and it does not persist workflow checkpoints or event log.
- Confirmed runtime runner cannot restore or hard-cancel an in-flight provider thread. NEXT resume should persist checkpoints and restart the next safe operation, while stale runtime results are ignored.
- Clarified requirement: "interrupt" means user-initiated termination of current NEXT execution. Resume should restart from persisted stable workflow/task checkpoints, not resume the same provider call.
- Added design note: per-sub-agent run conversation history must be persisted, otherwise NEXT can only restart tasks without preserving agent context.
- Added requirement: support multiple project-scoped NEXT workflow sessions with create/list/load/delete/switch management, like chat conversations.
- Wrote implementation plan for Phase 8 at `docs/superpowers/plans/2026-05-31-next-workflow-persistence.md`, with storage, session orchestration, interruption, sub-agent resume, and UI management split into maintainable tasks.
- Encountered transient Windows sandbox `spawn setup refresh` errors while reading broad test slices through pipeline commands; switched to narrower `rg`/`Get-Content -TotalCount` reads.
- Added failing tests for workflow snapshot/store round-trip, project scope isolation, multi-workflow session management, user termination checkpoint persistence, stale-result ignoring, and sub-agent message resume.
- Implemented `AINextWorkflowSnapshot` and `AINextWorkflowStore`.
- Wired `AIAgentNextSession` to project-scoped workflow persistence, including create/list/load/delete/continue APIs, workflow metadata, run-id guarded runtime completion, and durable agent run messages.
- Added `AINextProjectState::reset_interrupted_task` and `run_id` patch support.
- Added NEXT panel workflow management controls for selecting, creating, deleting, continuing, and terminating workflows.
- Fixed workflow-switch state ownership by rebinding the planning agent to the active `AINextProjectState`.
- Verification passed: `scons platform=windows target=editor tests=yes -j4`; `bin\next.windows.editor.x86_64.console.exe --test --test-case="*[Editor][AI][NEXT]*"`; `git diff --check`.
- Added milestone/task row icon UI: milestone rows use `Milestone`, task rows use `TaskGreen` for completed/skipped, `TaskYellow` for pending/ready/in-progress, and `TaskRed` for failed/blocked.
- Added UI regression coverage for NEXT row icons. Verification passed: `scons platform=windows target=editor tests=yes -j4`; `bin\next.windows.editor.x86_64.console.exe --test --test-case="*[Editor][AI][NEXT]*"`; `git diff --check`.
- Started Phase 10 after user reported milestone generation can freeze mid-run and reopening shows only one milestone.
- Confirmed milestone generation path is tool-driven: planning agent tool call `ai_next.manage_project` with `replace_plan` mutates `AINextProjectState`; final assistant content is not parsed into milestones.
- Added a red regression test for `ai_next.manage_project` accepting valid acyclic shared dependencies. The test failed against the existing dependency-cycle checker, confirming a tool-side validation bug.
- Replaced dependency-cycle detection with task-id graph DFS using visiting/done states. Focused regression now passes.
- Added a red regression test proving planning tool writes must be saved before the final provider response. It failed with a saved milestone count of 0 while runtime was blocked on the second provider turn.
- Stored runtime progress messages into active NEXT agent-run state and checkpointed workflow immediately after completed `ai_next.manage_project` tool writes. The mid-run persistence regression now passes.
- Verification passed: `scons platform=windows target=editor tests=yes -j4`; `bin\next.windows.editor.x86_64.console.exe --test --test-case="*[Editor][AI][NEXT]*"`; `git diff --check`.
