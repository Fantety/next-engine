# NextEngine Improvement Guides

This directory collects follow-up improvement documents for NextEngine AI features and supporting architecture. Each document is intentionally scoped as one independent improvement topic, so future work can be planned, reviewed, and implemented without reopening the whole roadmap.

## Feature Improvements

- [NEXT Replayable Execution Timeline](next-replayable-execution-timeline.md)
- [Project Memory and Design Constraints](project-memory-design-constraints.md)
- [NEXT Milestone Acceptance System](next-milestone-acceptance-system.md)
- [NEXT Change Sandbox and Rollback](next-change-sandbox-rollback.md)
- [NEXT Asset Registry and Reference Panel](next-asset-registry-reference-panel.md)

## Architecture Improvements

- [Unify Normal and NEXT Session Base](unify-normal-next-session-base.md)
- [Unified Tool Registration Groups](unified-tool-registration-groups.md)
- [Unified AI Storage Base](unified-ai-storage-base.md)
- [Context Budget Visualization](context-budget-visualization.md)

## User System Extensions

- [Cloud Sync for AI Configuration](cloud-sync-ai-configuration.md)
- [Team Collaboration Permissions](team-collaboration-permissions.md)
- [Agent Execution Audit](agent-execution-audit.md)

## MarkdownViewer Extensions

- [MarkdownViewer as AI Report View](markdownviewer-ai-report-view.md)
- [MarkdownViewer Godot Resource Links](markdownviewer-godot-resource-links.md)
- [MarkdownViewer Code Block Actions](markdownviewer-code-block-actions.md)

## Recommended Order

1. NEXT Replayable Execution Timeline
2. NEXT Milestone Acceptance System
3. NEXT Change Sandbox and Rollback
4. Project Memory and Design Constraints
5. NEXT Asset Registry and Reference Panel
6. Context Budget Visualization
7. MarkdownViewer AI report and resource-link improvements

The architecture topics can be done opportunistically while touching nearby code. `AISessionBase` and `AIStorageBase` already exist, so those documents focus on finishing consolidation rather than starting from zero.
