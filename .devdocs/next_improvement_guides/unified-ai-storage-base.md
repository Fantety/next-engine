# Unified AI Storage Base

## Purpose

Keep AI storage code consistent and reduce bugs in path handling, JSON serialization, and project-scoped persistence.

## Current State

`AIStorageBase` already exists and `AINextWorkflowStore` inherits from it. The improvement is to finish consolidation and make all AI stores use the same safety and JSON helpers where practical.

## Storage Areas

Relevant stores include:

- conversation storage
- NEXT workflow storage
- NEXT project storage
- future project memory storage
- future audit storage

## Shared Responsibilities

Storage classes should share:

- base directory setup
- project scope path construction
- safe path segment sanitization
- JSON read/write helpers
- temp-file atomic save pattern
- metadata loading helpers

## Proposed Design

Use `AIStorageBase` as the only place for generic file storage utilities.

Specialized stores should focus on domain concepts:

```text
AIConversationStore
- save conversation
- load conversation
- list conversations

AINextWorkflowStore
- save workflow snapshot
- load workflow snapshot
- list workflows

AIProjectMemoryStore
- save project memory
- load project memory
```

## Acceptance Criteria

- Stores do not duplicate sanitize, ensure directory, file path, or JSON helper code.
- Save operations use a temp-file pattern where data corruption would be costly.
- Existing workflow and conversation tests still pass.
- New stores can be implemented with minimal boilerplate.

## Risks

- Refactoring storage can affect user data. Avoid changing existing on-disk paths unless migration is explicit.
- Base helpers must not hide domain-specific validation.

## First Implementation Step

Audit stores for duplicated file and JSON operations, then move only common helpers into `AIStorageBase` without changing file layout.
