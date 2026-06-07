# Progress Log

## Session: 2026-05-27

### Phase 1: Map Surface Area
- **Status:** in_progress
- **Started:** 2026-05-27
- Actions taken:
  - Checked for existing planning files; none existed.
  - Checked git status; branch is `master` tracking `origin/master` with no shown local changes.
  - Listed repository root and files under `editor/ai_component` and `docs`.
  - Created planning files for resumable project orientation.
  - Read the architecture, MCP flow, script tooling, metadata, sign, and OpenAPI docs.
  - Re-read UTF-8 samples to verify Chinese docs were encoded correctly.
  - Read `editor/ai_component/SCsub` and searched editor registration/creation points.
- Files created/modified:
  - `task_plan.md`
  - `findings.md`
  - `progress.md`

### Phase 2: Read Design Docs
- **Status:** complete
- Actions taken:
  - Summarized the main AI Agent docs into `findings.md`.
  - Identified docs that are core to the AI component versus adjacent server/API docs.
- Files created/modified:
  - `findings.md`
  - `progress.md`

### Phase 3: Read Core Source Paths
- **Status:** complete
- Actions taken:
  - Read and searched core agent interfaces and implementations: session, runtime, runtime runner, context manager, profile, permissions.
  - Read provider/model interfaces and OpenAI-compatible runtime client/codec responsibilities.
  - Searched MCP service/discovery runner flow, review/diff storage, conversation storage, UI dock/composer, context/rules/skills/planning, and scene/script/shader service boundaries.
  - Indexed AI-related tests and build registration hooks.
- Files created/modified:
  - `findings.md`
  - `progress.md`

### Phase 4: Synthesize Handoff
- **Status:** complete
- Actions taken:
  - Verified orientation notes cover module boundaries, request flow, extension points, and validation references.
  - Prepared final summary for the user.
- Files created/modified:
  - `findings.md`
  - `progress.md`
  - `task_plan.md`

### Phase 5: Markdown Viewer Redesign
- **Status:** in_progress
- Actions taken:
  - Read current `AIMarkdownLabel`, `AIMarkdownRenderer`, `AIMessageBubble`, UI `SCsub`, and existing markdown-related tests.
  - Confirmed current implementation is RichTextLabel-centered and lacks component-level tables/code/images/copy behavior.
  - Started brainstorming workflow and visual companion setup discussion; no production code changed.
  - User clarified the target is a registered Godot UI node, not only an AI chat message renderer.
  - Confirmed runtime GUI registration point and existing runtime controls that can support MarkdownViewer implementation.
  - User selected visual direction B in the browser companion and explicitly requested remote image downloads.
  - Confirmed runtime `HTTPRequest`, image decoding, and read-only `CodeEdit`/syntax highlighter APIs relevant to MarkdownViewer.
  - User rejected a component tree built from existing controls and selected a lower-level custom renderer because of performance and memory concerns.
  - Updated redesign notes to target a runtime self-drawn `Control` with internal parse/layout/draw/hit-test state.
  - User approved the self-drawn design direction and asked that the implementation structure remain easy to extend.
  - Wrote the design spec at `docs/superpowers/specs/2026-05-27-markdown-viewer-design.md`.
  - Wrote the implementation plan at `docs/superpowers/plans/2026-05-27-markdown-viewer-implementation.md`.
  - User deferred AI component migration until the runtime node is validated in-engine.
  - User requested that builds/tests using `scons` or the editor binary are run by the user, not by the agent.
- Files created/modified:
  - `task_plan.md`
  - `findings.md`
  - `progress.md`

### Phase 6: Runtime MarkdownViewer Node Implementation
- **Status:** in_progress
- **Started:** 2026-05-28
- Actions taken:
  - Added runtime `MarkdownViewer : Control` and registered it as a scene UI class.
  - Added focused helper modules for document conversion, layout, drawing, image loading, and code highlighting.
  - Added scene tests covering registration/defaults, Markdown document blocks, pipe tables, image loading states, PNG decode, code highlighting, layout hit tests, draw culling, scrolling, and minimum size behavior.
  - Kept AI component migration deferred; no `editor/ai_component` source files were modified.
  - Fixed static integration issues found during review: native `gui_input()` override, internal image loader registration, remote-image cache reactivation when enabling downloads, HTTPRequest cleanup, and syntax-highlighting layout integration.
  - Tightened final viewer behavior found during static review: enabled clipping on the control and hid code-copy affordances when `code_copy_enabled` is false.
  - Ran non-build static checks only, per user instruction not to execute `scons` or editor test binaries.
- Files created/modified:
  - `scene/gui/markdown_viewer.h`
  - `scene/gui/markdown_viewer.cpp`
  - `scene/gui/markdown_viewer_document.h`
  - `scene/gui/markdown_viewer_document.cpp`
  - `scene/gui/markdown_viewer_layout.h`
  - `scene/gui/markdown_viewer_layout.cpp`
  - `scene/gui/markdown_viewer_draw.h`
  - `scene/gui/markdown_viewer_draw.cpp`
  - `scene/gui/markdown_viewer_image_loader.h`
  - `scene/gui/markdown_viewer_image_loader.cpp`
  - `scene/gui/markdown_viewer_code_highlighter.h`
  - `scene/gui/markdown_viewer_code_highlighter.cpp`
  - `scene/register_scene_types.cpp`
  - `tests/scene/test_markdown_viewer.cpp`

## Test Results
| Test | Input | Expected | Actual | Status |
|------|-------|----------|--------|--------|
| Orientation scope check | Read `task_plan.md`, `findings.md`, and git status | Findings cover requested directories and source references; no business code changes | Covered `editor/ai_component`, `docs`, tests/build hooks; only planning files are untracked | Passed |
| MarkdownViewer static whitespace check | `git diff --check` | No whitespace errors in tracked diff | No output | Passed |
| MarkdownViewer no rich text/component renderer check | `rg` over new `scene/gui/markdown_viewer*` files | No `RichTextLabel` or rendering child control construction | No `RichTextLabel`/child renderer construction matches | Passed |
| MarkdownViewer untracked whitespace check | `rg -n "[ \t]+$" scene\gui tests\scene\test_markdown_viewer.cpp -g "markdown_viewer*"` | No trailing whitespace in new MarkdownViewer files | No matches | Passed |
| MarkdownViewer AI component isolation check | `rg` over MarkdownViewer files and registration | No AI component references | No matches | Passed |

## Error Log
| Timestamp | Error | Attempt | Resolution |
|-----------|-------|---------|------------|
| 2026-05-27 | Windows sandbox spawn setup refresh during one parallel read group | 1 | Continued with focused reads; most requested outputs completed and UTF-8 follow-up confirmed doc content. |
| 2026-05-27 | PowerShell wildcard paths failed in `rg` with Windows path syntax error | 1 | Switched to directory or explicit-file searches. |
| 2026-05-27 | Windows sandbox spawn setup refresh while reading `progress.md` in a parallel group | 1 | Continued with relevant source reads and recorded the non-critical read failure. |

## 5-Question Reboot Check
| Question | Answer |
|----------|--------|
| Where am I? | Phase 5: Markdown Viewer redesign brainstorming. |
| Where am I going? | Confirm design, then write implementation plan and implement with tests first. |
| What's the goal? | Understand `editor/ai_component` and related `docs` for future work. |
| What have I learned? | See `findings.md`. |
| What have I done? | Created orientation plan and initial module inventory. |
