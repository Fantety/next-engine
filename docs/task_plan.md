# Task Plan: AI Component Project Orientation

## Goal
Build a concise working understanding of `editor/ai_component` and the supporting `docs` so future changes can start from the existing architecture instead of rediscovering it.

## Current Phase
Phase 5

## Phases

### Phase 1: Map Surface Area
- [x] Capture user intent
- [x] Identify relevant directories and docs
- [x] Record initial module map in `findings.md`
- **Status:** complete

### Phase 2: Read Design Docs
- [x] Summarize architecture docs
- [x] Connect docs to source modules
- [x] Record open questions and constraints
- **Status:** complete

### Phase 3: Read Core Source Paths
- [x] Understand agent runtime/session/message flow
- [x] Understand provider/model/MCP flow
- [x] Understand tools/context/planning/review/storage/UI boundaries
- [x] Check tests and build hooks for validation reference
- **Status:** complete

### Phase 4: Synthesize Handoff
- [x] Produce a short architecture summary for the user
- [x] Include important file references
- [x] Note likely extension points and risks
- **Status:** complete

### Phase 5: Markdown Viewer Redesign
- [x] Read current Markdown label/renderer and message bubble integration
- [x] Clarify scope and design constraints
- [x] Propose implementation approach
- [ ] Get user approval before implementation
- **Status:** in_progress

## Key Questions
1. What are the main architectural boundaries inside `editor/ai_component`?
2. How does an AI request flow from UI to runtime, provider, tools, and back?
3. Which docs describe intended behavior versus what source currently implements?
4. What are the practical extension points for future work?

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| Treat this as orientation, not implementation | User asked to understand the project and docs as reference, with no requested code change. |
| Create planning files in project root | The task spans many files and should be resumable. |
| Build Markdown Viewer as a runtime self-drawn `Control` | User wants a general Godot UI node and rejected RichTextLabel/component-tree approaches for performance and memory reasons. |

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|

## Notes
- Keep findings concise and tied to concrete source files.
- Do not modify source code during orientation unless user asks for implementation.
- For Markdown Viewer work, follow design approval first, then TDD before production code.
