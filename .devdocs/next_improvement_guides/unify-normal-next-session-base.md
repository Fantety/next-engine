# Unify Normal and NEXT Session Base

## Purpose

Keep Normal mode and NEXT mode session behavior consistent where they share responsibilities, while preserving their different product models.

## Current State

`AISessionBase` already exists and `AIAgentNextSession` inherits from it. This means the improvement is no longer "create a base class from scratch." The remaining work is to continue moving shared behavior out of mode-specific sessions when it is clearly identical.

## Shared Responsibilities

Candidates for base-level consolidation:

- project scope key
- unique id generation
- runtime signal connection helpers
- common save/load helpers
- interruption normalization patterns
- runtime message indexing utilities
- common test helpers where appropriate

## Boundaries

Do not force these into the base class:

- Normal conversation list behavior
- NEXT workflow checkpoint behavior
- NEXT milestone/task state
- Normal plan manager behavior
- UI-specific behavior

The base class should provide reusable session plumbing, not a shared product model.

## Proposed Design

Continue evolving `AISessionBase` as a small utility base:

```text
AISessionBase
- project scope
- id helpers
- runtime signal connection
- common lifecycle helpers

AIAgentSession
- conversation messages
- Normal mode provider/profile flow
- Normal tool approvals

AIAgentNextSession
- workflow state
- milestone/task execution
- agent run checkpointing
```

## Acceptance Criteria

- Shared helper code exists in one place.
- Normal and NEXT tests still pass without changing user-visible behavior.
- Base class does not need to know about milestones, conversations, or UI panels.
- New session features can use base helpers without duplicating project scope and runtime connection logic.

## Risks

- Over-generalizing the base class can make both modes harder to reason about.
- Moving behavior too aggressively can create subtle lifecycle regressions.

## First Implementation Step

Audit remaining duplicate helper methods in `AIAgentSession` and `AIAgentNextSession`, then move only exact matches or near-exact utility logic into `AISessionBase`.
