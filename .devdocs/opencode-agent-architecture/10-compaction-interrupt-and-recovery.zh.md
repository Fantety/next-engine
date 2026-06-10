# 10 Compaction, Interruption, and Recovery

This module is responsible for keeping long sessions sustainable and making the execution process interpretable and recoverable after interruption, failure, or restart. It relies on the event log, Session Inbox, Context Epoch, and the safety boundaries of the Runner.

> Source consistency note: The current V2 already has automatic compaction and one compaction retry triggered by provider overflow; the `sessions.compact(...)` public core route still returns OperationUnavailable. Post-crash activity recovery is also explicitly deferred; startup or wake will not blindly retry ambiguous provider work.

## Module Responsibilities

This module is responsible for:

- Triggering compaction when history becomes too long, compressing early conversations into a summary.
- Exposing the compaction result as a subsequent context source.
- Handling interruption requests, stopping provider streams or tool execution.
- Cleaning up hanging states on startup, such as running tools or active steps.
- Clearly defining which work can be automatically recovered after a crash and which must wait for explicit user recovery.

This module is not responsible for:

- Replacing HistorySelector.
- Blindly retrying provider calls after a crash.
- Deleting historical events.

## Compaction Overview

The goal of compaction is not to rewrite history, but to provide a shorter context representation for future provider turns without deleting sources of truth.

```text
Event Log full history
  -> choose compaction input messages
  -> call summarizer model
  -> write compaction completed event
  -> Context Source exposes summary
  -> HistorySelector can omit compacted messages
```

## Compaction Data Model

```ts
type CompactionID = string

type CompactionRow = {
  id: CompactionID
  sessionID: SessionID
  inputMessageIDs: MessageID[]
  outputMessageID?: MessageID
  summary: string
  state: "running" | "completed" | "failed"
  tokenBefore: number
  tokenAfter?: number
  createdAt: number
  completedAt?: number
}
```

Events:

```ts
type CompactionStartedPayload = {
  compactionID: CompactionID
  inputMessageIDs: MessageID[]
  tokenBefore: number
}

type CompactionCompletedPayload = {
  compactionID: CompactionID
  summary: string
  tokenAfter: number
}

type CompactionFailedPayload = {
  compactionID: CompactionID
  error: ToolError
}
```

## Trigger Timing

```ts
interface CompactionPolicy {
  shouldCompact(input: {
    sessionID: SessionID
    model: ModelInfo
    selectedHistory: SelectedHistory
    fullHistoryTokenEstimate: number
  }): Promise<CompactionDecision>
}

type CompactionDecision =
  | { action: "none"; reason: string }
  | { action: "compact"; reason: string; messageIDs: MessageID[] }
```

Common trigger conditions:

- Full history exceeds a certain percentage of the model's context window.
- Recent turns using the `recent` strategy in HistorySelector consistently drop early context.
- User explicitly requests "compact context."
- About to switch to a model with a smaller context window.

Do not compact during a provider stream. Compaction should occur at safety boundaries:

- Before a new provider turn.
- After activity has settled.
- When explicitly commanded by the user.

## Input Selection

Compaction input should not include incomplete tool calls. Recommended selection:

- Completed early user/assistant/tool messages.
- Fully paired tool calls and tool results.
- Exclude the most recent N conversation turns.
- Exclude messages that are not yet settled in the current activity.

```ts
interface CompactionInputSelector {
  select(input: {
    sessionID: SessionID
    history: SessionMessage[]
    keepRecentTurns: number
  }): Promise<SessionMessage[]>
}
```

## Summary Output Format

The summary should be oriented toward subsequent Agent use, not human readability.

Recommended structure:

```text
## Conversation Summary

### User Goal
...

### Decisions Made
...

### Files / Artifacts Mentioned
...

### Tool Results
...

### Open Tasks
...

### Constraints and Preferences
...
```

The summary must retain:

- User goals and preferences.
- Decisions made.
- Key files, interfaces, and data models.
- Facts from tool results that affect subsequent behavior.
- Pending matters.

Do not retain:

- Extensive meaningless small talk.
- Complete tool outputs.
- Large portions of content that can be re-read from the file system.

## Compaction Service

```ts
interface CompactionService {
  maybeCompact(input: {
    sessionID: SessionID
    model: ModelInfo
    signal: AbortSignal
  }): Promise<CompactionResult | undefined>

  compact(input: {
    sessionID: SessionID
    messageIDs: MessageID[]
    signal: AbortSignal
  }): Promise<CompactionResult>
}

type CompactionResult = {
  compactionID: CompactionID
  summary: string
  tokenBefore: number
  tokenAfter: number
}
```

Call chain:

```text
Runner before provider turn
  -> HistoryProjection.projectMessages
  -> CompactionPolicy.shouldCompact
  -> CompactionService.compact if needed
  -> EventStore.append("session.compaction.completed")
  -> ContextSource "compaction summary" updates
  -> HistorySelector.select
```

## Compaction and History Selection

After HistorySelector sees the compaction summary:

- Use the summary as a replacement for early history.
- Retain recent messages that were not compacted.
- Do not delete original events.

```ts
type CompactedHistoryMarker = {
  type: "context"
  sourceID: `compaction:${CompactionID}`
  content: string
}
```

## Interruption Model

Interruption is divided into "interrupt requested" and "interrupted."

```ts
type InterruptRequest = {
  id: string
  sessionID: SessionID
  reason?: string
  createdAt: number
}
```

Events:

```ts
type InterruptRequestedPayload = {
  interruptID: string
  reason?: string
}

type InterruptedPayload = {
  interruptID?: string
  assistantMessageID?: MessageID
  activeToolCallID?: ToolCallID
  reason?: string
}
```

## Interruption Call Chain

```text
UI/CLI interrupt
  -> SessionService.interrupt(sessionID)
  -> EventStore.append("session.interrupt.requested")
  -> SessionExecution.interrupt(sessionID)
  -> active AbortController.abort(reason)
  -> Runner catches abort
  -> EventStore.append("session.interrupted")
```

`SessionExecution.interrupt` only interrupts the active drain owned by the current process. When there is no active drain:

- Can still write `session.interrupt.requested`.
- Does not error.
- Next Runner drain sees idle state and handles it.

## Provider Stream Interruption

Handling rules:

- Preserve already received text/reasoning deltas.
- Do not execute incomplete tool call deltas.
- If the provider has already returned a complete tool call but the tool hasn't been executed, treat it as canceled/failed.
- Write `session.interrupted`.

## Tool Execution Interruption

Tool implementations must accept an `AbortSignal`.

```ts
async function execute(input: ToolExecutionInput<unknown>) {
  input.signal.throwIfAborted()
  // long running work checks signal periodically
}
```

When interrupting a tool:

- Terminate child processes, network requests, browser operations, or child agent waits.
- Write `assistant.tool.failed` or `assistant.tool.canceled`.
- The tool result text sent to the model should indicate user interruption.

## Startup Cleanup

A process crash may leave running states. Upon startup, local running states that cannot continue need to be cleaned up.

```ts
interface StartupRecovery {
  cleanup(): Promise<StartupCleanupResult>
}

type StartupCleanupResult = {
  interruptedSessions: SessionID[]
  failedToolCalls: ToolCallID[]
  notes: string[]
}
```

Cleanup rules:

- If `assistant.step.started` has no completed following it, mark as interrupted or failed.
- If `assistant.tool.started` has no completed/failed following it, mark as failed with reason "process restarted."
- `permission_pending` can be retained, waiting for user response or cancellation.
- Admitted prompts are not automatically discarded.

## Post-Crash Recovery Strategy

Do not automatically retry provider calls on startup. Reasons:

- The previous provider may have already caused external side effects.
- Tools may have already executed but results were not persisted.
- Cloud model requests may have already been billed or partially output.

Recommended strategy:

- Restore Session state to idle after startup.
- UI prompts "Last run was interrupted, you may continue."
- When the user explicitly sends continue or resume, start a new provider turn from the durable inbox and history projection.

## Implementation Steps

1. Implement `InterruptRequest` and `SessionService.interrupt`.
2. Have `SessionExecution` save AbortController for the active drain.
3. Runner and tool execution check AbortSignal.
4. Implement `StartupRecovery.cleanup`.
5. Implement `CompactionPolicy` and input selector.
6. Implement summarizer provider call.
7. Register compaction summary as a Context Source.
8. Have HistorySelector support compacted strategy.

## Acceptance Criteria

- After provider stream interruption, existing output is preserved and state is interrupted.
- After tool execution interruption, no running tool state remains.
- After process restart, running steps/tools are cleaned up to an interpretable terminal state.
- Admitted input is not lost due to crash.
- After compaction, original events still exist, and subsequent models use the summary.
- Tool calls/results are not split apart by compaction.

## Common Pitfalls

- Automatically retrying provider turns after a crash, causing duplicate external side effects.
- Deleting already output content upon interruption, destroying sources of truth.
- Overwriting original history during compaction, making auditing impossible.
- Summaries overly focused on human narrative, losing details needed for subsequent development.
- Tools not accepting AbortSignal, causing UI to show interrupted while background processes still run.
