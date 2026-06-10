# 01 Domain Model and Event Log

This chapter describes the source-of-truth model for the current opencode V2 Core. The core idea is: a Session's recoverable state comes from the durable event log, `session_input` durable inbox, projected `session_message`, and Session-level Context Epoch. Connected UIs can subscribe to live-only deltas, but replay and recovery rely only on durable boundary events.

## Module Responsibilities

This module is responsible for:

- Defining foundational entities such as Session, Location, Prompt, Message, Attachment, Tool Call, and Context Epoch.
- Persisting an append-only durable event log.
- Projecting durable events into `session_input` and `session_message`.
- Providing a replayable Session events cursor.
- Distinguishing between durable events and live-only ephemeral deltas.

This module is NOT responsible for:

- Invoking providers.
- Executing tools.
- Making authorization decisions.
- Managing UI draft-state attachments.

## Core Entities

### Location

```ts
type LocationRef = {
  directory: string
  workspaceID?: string
}
```

Semantics:

- Runner, ToolRegistry, Permission state, filesystem, and System Context are all Location-scoped.
- SessionExecution starts from Session ID only, reads the Session location at the beginning of drain, and enters the corresponding Location runtime.
- Session move resets the Context Epoch, preventing privileged context from the old Location from being reused.

### Session

```ts
type SessionInfo = {
  id: SessionID
  projectID: string
  directory: string
  path: string
  workspaceID?: string
  title: string
  agent?: AgentID
  model?: ModelRef
  cost: number
  tokens: TokenUsage
  time: {
    created: number
    updated: number
  }
}
```

Semantics:

- `sessions.create({ id?, location, agent?, model? })` creates or reuses a Session.
- Reusing an existing Session ID returns the existing Session identity.
- Current V2 `prompt` requires an existing Session and does not implicitly create one.

### Prompt

The current prompt is structured input, not a plain string:

```ts
type Prompt = {
  text: string
  files: FileAttachment[]
  agents: AgentReference[]
  references: PromptReference[]
}
```

Semantics:

- `text` is the user's natural language input.
- `files` are durable typed attachments.
- `agents` and `references` are structured entry points for prompt/reference expansion; some native expansion remains partial/follow-up in V2 parity.

### Session Input

`session_input` is the durable prompt admission inbox.

```ts
type SessionInputAdmitted = {
  admittedSeq: number
  id: MessageID
  sessionID: SessionID
  prompt: Prompt
  delivery: "steer" | "queue"
  timeCreated: DateTime
  promotedSeq?: number
}
```

Semantics:

- `admittedSeq` comes from the aggregate sequence of `session.next.prompt.admitted`.
- When `promotedSeq` exists, it indicates the prompt has been projected into a model-visible user message.
- Exact retries of the same message ID require equivalent Session, prompt, and delivery; otherwise, a conflict occurs.

### Session Message

Projected messages are used by the UI and Runner history.

```ts
type SessionMessage =
  | UserMessage
  | AssistantMessage
  | SystemMessage
  | CompactionMessage

type UserMessage = {
  id: MessageID
  type: "user"
  text: string
  files: FileAttachment[]
  agents: AgentReference[]
  references: PromptReference[]
  time: { created: DateTime }
}

type AssistantMessage = {
  id: MessageID
  type: "assistant"
  content: AssistantContent[]
}

type AssistantContent =
  | { type: "text"; text: string }
  | { type: "reasoning"; text: string; providerMetadata?: unknown }
  | { type: "tool"; id: ToolCallID; name: string; state: ToolState; provider?: ProviderToolMetadata }
```

Semantics:

- Assistant message ID is the durable owner of tool settlement.
- Tool call ID comes from the provider and may repeat across turns, so it cannot uniquely locate a durable tool record.

### Tool State

Event projection currently turns tools into tool state within assistant content.

```ts
type ToolState =
  | { status: "pending"; input?: unknown }
  | { status: "running"; input: unknown; progress?: ToolOutput }
  | { status: "success"; input: unknown; output: ToolOutput; outputPaths?: string[] }
  | { status: "failed"; input?: unknown; error: { type: "unknown"; message: string } }
```

Semantics:

- `pending/running` local tools are marked as `Tool execution interrupted` at the start of a new run.
- Provider-executed tools retain `provider.executed = true` and provider metadata.

### Context Epoch

The current Context Epoch is Session-level baseline, not a provider-turn row.

```ts
type SessionContextEpoch = {
  sessionID: SessionID
  baseline: string
  snapshot: SystemContextSnapshot
  agent: AgentID
  baselineSeq: number
  replacementSeq?: number
  revision: number
}
```

Semantics:

- `baseline` is the privileged system context.
- `snapshot` is the source observation hidden from the model.
- Source changes write `session.next.context.updated` and advance the revision.
- Replacement/reset is used for agent/model/compaction/location move boundaries.

## Durable Event Store

The event store is a source of truth projected synchronously by aggregate sequence.

```ts
type EventRow = {
  id: EventID
  aggregateID: SessionID
  seq: number
  type: EventType
  data: unknown
  timestamp?: DateTime
}
```

Constraints:

- `seq` increases monotonically within the same Session aggregate.
- Durable event replay cursor is based on aggregate sequence.
- Live-only deltas do not advance the durable cursor.

## Current `session.next.*` Event Family

### Prompt Lifecycle

```text
session.next.prompt.admitted
session.next.prompt.promoted
```

Meaning:

- admitted: writes to durable inbox, not visible to the model.
- promoted: projects a user message, visible to the model.

### Step

```text
session.next.step.started
session.next.step.ended
session.next.step.failed
```

`step.started` creates an assistant message shell and records agent/model/snapshot information.

### Text

```text
session.next.text.started
session.next.text.delta   live-only
session.next.text.ended   durable
```

`text.delta` is for the live UI; replay uses `text.ended`.

### Reasoning

```text
session.next.reasoning.started
session.next.reasoning.delta   live-only
session.next.reasoning.ended   durable
```

The reasoning durable boundary retains provider metadata. During history replay, provider-native metadata is only replayed when the historical assistant model matches the continuation model; after a model switch, reasoning text is visible as ordinary assistant text, and provider-native metadata is omitted.

### Tool

```text
session.next.tool.input.started
session.next.tool.input.delta   live-only
session.next.tool.input.ended   durable
session.next.tool.called
session.next.tool.progress
session.next.tool.success
session.next.tool.failed
```

Tool events must carry:

```ts
type ToolEventBase = {
  sessionID: SessionID
  assistantMessageID: MessageID
  callID: string
}
```

`tool.called` payload:

```ts
type ToolCalled = ToolEventBase & {
  tool: string
  input: Record<string, unknown>
  provider: {
    executed: boolean
    metadata?: ProviderMetadata
  }
}
```

`tool.success` payload:

```ts
type ToolSuccess = ToolEventBase & {
  structured: Record<string, unknown>
  content: ToolOutputContent[]
  outputPaths?: string[]
  result?: unknown
  provider: {
    executed: boolean
    metadata?: ProviderMetadata
  }
}
```

`tool.failed` payload:

```ts
type ToolFailed = ToolEventBase & {
  error: { type: "unknown"; message: string }
  result?: unknown
  provider: {
    executed: boolean
    metadata?: ProviderMetadata
  }
}
```

### Context

```text
session.next.context.updated
```

Context updates are projected as chronological system messages. There is no `context.epoch.created` durable event; epoch baseline/snapshot exists in the epoch table.

### Compaction

```text
session.next.compaction.started
session.next.compaction.delta   live-only
session.next.compaction.ended   durable
```

The current durable ended v2 includes summary text and serialized recent context.

### Interrupt

```text
session.next.interrupt.requested
```

Interrupt requested is a durable event; actual cancellation of the active local ownership chain is done via SessionExecution/Effect interruption.

### Permission

Permission events are not part of `session.next.*` but are also durable events:

```text
permission.v2.asked
permission.v2.replied
```

## Boundary Between Ephemeral and Durable

Current live-only events:

```text
session.next.text.delta
session.next.reasoning.delta
session.next.tool.input.delta
session.next.compaction.delta
```

These events:

- Can be pushed in real-time to connected UIs.
- Are not part of the replayable Session stream.
- Do not advance the durable cursor.
- Cannot be relied upon to restore full state after reconnection.

Replayable state must come from durable events such as ended/success/failed/promoted/context.updated.

## Projection Responsibilities

The Projector consumes durable events and updates the read model:

```text
session.next.prompt.admitted
  -> insert session_input row

session.next.prompt.promoted
  -> mark session_input.promoted_seq
  -> insert user session_message

session.next.context.updated
  -> insert system session_message

session.next.text.ended
  -> append assistant text content

session.next.reasoning.ended
  -> append assistant reasoning content

session.next.tool.called
  -> append/update assistant tool content pending/running

session.next.tool.success/failed
  -> settle assistant tool content

session.next.compaction.ended
  -> insert compaction session_message
  -> request Context Epoch replacement
```

## Replay API

The Session events API returns a durable-only stream:

```ts
interface SessionEventsAPI {
  events(input: {
    sessionID: SessionID
    after?: EventCursor
  }): Stream<CursorEvent<DurableSessionEvent>>
}
```

Design points:

- Replay and live tail use the same durable cursor.
- Tail wakeups are advisory and edge-triggered; consumers re-query durable rows upon receiving a wakeup.
- If future UI APIs want to mix live deltas, they must make it clear that these deltas do not advance the durable cursor.

## Runner History

The Runner does not read raw events directly but reads the projected history:

```ts
SessionHistory.entriesForRunner(sessionID, baselineSeq)
  -> latest compaction boundary
  -> session_message rows after boundary
  -> system messages only after baselineSeq
  -> decode to SessionMessage
```

This ensures:

- System context before the baseline does not repeatedly enter the request.
- Old transcripts before compaction are replaced by checkpoints.
- Durable chronological updates are still retained by seq.

## Implementation Steps

1. Implement the durable EventStore, storing by aggregate sequence.
2. Define `session.next.*` durable definitions and live-only delta definitions.
3. Implement the projector to write durable events to `session_input` and `session_message`.
4. Implement `sessions.events({ after })` durable-only replay.
5. Implement `SessionHistory.entriesForRunner`.
6. Implement the `SessionContextEpoch` table and initialize/prepare/reset.
7. Construct requests in the Runner based solely on projected history and epoch baseline.

## Acceptance Criteria

- After reconnection, Session messages can be restored using only durable events.
- Live-only deltas do not advance the replay cursor.
- Admitted prompts do not appear in model history; only promoted prompts appear.
- Tool success/failed must be able to locate tool content via assistantMessageID + callID.
- Context updates are projected as system messages.
- Compaction does not delete old events, but Runner history continues from the checkpoint boundary.
- After process restart, pending/running tools can be marked as interrupted.

## Common Pitfalls

- Treating text deltas as a durable replay source.
- Using only provider call ID to locate tool settlement, ignoring assistant message ID.
- Writing Context Epoch as a per-turn request snapshot, losing baseline replacement/fencing semantics.
- Going directly to model history after prompt admission, bypassing promotion.
- Provider-executed tool result overwriting call-side metadata, causing loss of continuation identifiers.
