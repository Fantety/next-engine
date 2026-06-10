# 05 Context Management and History Projection

This chapter describes the implementation based on the current opencode V2 System Context Epoch and Session History. Core point: The Context Epoch is not a request snapshot for each provider turn, but a session-level privileged system-context baseline, plus a model-hidden structured snapshot, baseline sequence, and revision. The Runner reconciles current sources at safety boundaries and writes a chronological system message when necessary.

## Module Responsibilities

This module is responsible for:

- Projecting Session messages from durable `session.next.*` events.
- Loading canonical model-visible history for the Runner.
- Managing SessionContextEpoch: initialize, prepare, replace, reset, current fence.
- Synthesizing the System Context Registry and selected-agent Skill Guidance into a baseline.
- Writing `session.next.context.updated` when context changes.
- Having history continue from checkpoint boundaries after compaction.

This module is not responsible for:

- Executing tools.
- Invoking providers.
- Putting tool schemas or attachments directly into epoch rows.
- Treating ephemeral deltas as replayable history.

## Current Data Model

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

Field semantics:

- `baseline`: Model-visible text of the current privileged system context.
- `snapshot`: Model-hidden structured source observation for the next reconciliation.
- `agent`: Effective agent to which the baseline belongs.
- `baselineSeq`: Session aggregate sequence when the baseline was acknowledged as valid.
- `replacementSeq`: Sequence requesting baseline replacement, typically from boundaries like agent/model switch, compaction, move, etc.
- `revision`: Optimistic concurrency fence preventing stale observations from overwriting new context.

## System Context Sources

Current Runner loads:

```text
SystemContextRegistry.load()
+ SkillGuidance.load(selectedAgent)
-> SystemContext.combine(...)
```

Primary sources entering V2 baseline:

- environment facts
- host-local date
- ambient global/upward project instructions
- selected-agent available-skill guidance
- Location-wide System Context Registry sources

Content still partial/missing in V2 parity:

- configured local/glob/remote URL instructions
- nearby nested instructions after reads
- provider-family baseline instruction
- per-prompt system text/tool override
- plugin message/system/parameter/header transforms
- structured-output policy

## Epoch Initialize

Initialization occurs before a pending prompt becomes model-visible.

```ts
function initialize(input: {
  context: Effect<SystemContext>
  sessionID: SessionID
  location: LocationRef
  agent: AgentID
}): Effect<
  | {
      baseline: string
      baselineSeq: number
      revision: number
    }
  | undefined,
  SystemContext.InitializationBlocked
>
```

Semantics:

- If an epoch already exists, returns `undefined`.
- If not, observes the complete System Context.
- If initial context is temporarily unavailable, Runner stops, prompt remains pending, retryable.
- Fences Session location and selected agent when inserting epoch.
- `baselineSeq` uses the current Session latest aggregate sequence.

Call site:

```text
runTurnAttempt
  -> select agent
  -> SessionContextEpoch.initialize(...)
  -> only then promote eligible input
```

This order guarantees that user prompts are not promoted to model-visible without a complete privileged baseline.

## Epoch Prepare

`prepare` performs reconciliation or replacement when an epoch already exists.

```ts
function prepare(input: {
  context: Effect<SystemContext>
  sessionID: SessionID
  location: LocationRef
  agent: AgentID
}): Effect<
  {
    baseline: string
    baselineSeq: number
    revision: number
  },
  InitializationBlocked | ContextSnapshotDecodeError | AgentReplacementBlocked
>
```

Flow:

```text
load current SystemContext
+ load stored epoch
  -> no stored epoch: initialize baseline
  -> stored epoch and no replacement requested: reconcile(value, snapshot)
  -> replacement requested or agent changed: replace(value, snapshot)
```

Results:

- `Unchanged`: Reuse stored baseline and baselineSeq.
- `Changed`: Publish `session.next.context.updated` and advance snapshot/revision in commit.
- `ReplacementReady`: Replace baseline, snapshot, agent, baselineSeq, clear replacementSeq.
- `ReplacementBlocked`: If agent is being replaced but new source is incomplete, block to avoid exposing old agent's privileged baseline.

## Context Updated Event

Context changes do not write `context.epoch.created` but write a chronological system message:

```text
session.next.context.updated
```

Payload:

```ts
type ContextUpdated = {
  sessionID: SessionID
  messageID: MessageID
  timestamp: DateTime
  text: string
}
```

Commit rule:

```text
events.publish(ContextUpdated, payload, {
  commit: () => advance(epoch.snapshot, revision + 1)
})
```

That is, event writing and snapshot revision advancement must complete within the same durable commit boundary.

## Replacement and Reset

Certain events request baseline replacement:

- agent switch
- model switch
- completed compaction
- selected instruction/source semantically requires replacement

```ts
function requestReplacement(sessionID: SessionID, seq: number): Effect<void>
```

Session move resets epoch:

```ts
function reset(sessionID: SessionID): Effect<void>
```

The next provider turn must reinitialize the complete baseline under the new Location.

## Fencing

Context Epoch concurrency fence prevents ABA and stale runtime:

```text
insert epoch
  -> require Session location matches current Location
  -> require selected agent absent or equals observed agent

prepare epoch
  -> update where revision == expectedRevision
  -> require selected agent still equals observed agent

before llm.stream
  -> SessionContextEpoch.current(sessionID, agent, revision)
```

If the Runner finds that agent/model/location was concurrently modified while preparing the request, it rebuilds the prepared turn rather than calling the provider with stale context.

## History Projection

The projection layer writes durable events to `session_message`, and the Runner loads history from the projection table.

```ts
interface SessionHistory {
  load(sessionID: SessionID): Effect<SessionMessage[]>

  loadForRunner(input: {
    sessionID: SessionID
    baselineSeq: number
  }): Effect<SessionMessage[]>

  entriesForRunner(input: {
    sessionID: SessionID
    baselineSeq: number
  }): Effect<Array<{ seq: number; message: SessionMessage }>>
}
```

Current loading rules:

- If a latest compaction message exists, load only messages after the compaction seq.
- If baselineSeq exists, load only chronological system messages after baselineSeq.
- Non-system messages are loaded in ascending seq order.
- Decode failures become `MessageDecodeError`.

Pseudo-code:

```ts
function messageRows(sessionID, compaction, baselineSeq) {
  return select session_message
    where session_id = sessionID
      and (
        no compaction
        or seq >= compaction.seq
        or (type == "system" and seq > baselineSeq)
      )
      and (
        no baselineSeq
        or type != "system"
        or seq > baselineSeq
      )
    order by seq asc
}
```

Meaning:

- Old system contexts before baseline do not re-enter history.
- Old ordinary messages before compaction checkpoint are replaced by the checkpoint.
- New system updates after compaction still enter history.

## Session Message Projection Principles

Relationship between important durable events and messages:

```text
session.next.prompt.promoted       -> user message
session.next.context.updated       -> system message
session.next.synthetic             -> synthetic/system-like message
session.next.step.started          -> assistant message shell
session.next.text.ended            -> assistant text content
session.next.reasoning.ended       -> assistant reasoning content
session.next.tool.called           -> assistant tool content pending/running
session.next.tool.success/failed   -> assistant tool settlement
session.next.compaction.ended      -> compaction message
```

Live-only deltas:

```text
session.next.text.delta
session.next.reasoning.delta
session.next.tool.input.delta
session.next.compaction.delta
```

These events can be used for live UI but should not serve as durable replay cursors.

## Skill Guidance and Context

Skill Guidance is currently loaded by the Runner:

```text
agent = agents.select(session.agent)
system = SystemContextRegistry.load()
skills = SkillGuidance.load(agent)
combined = SystemContext.combine(system, skills)
SessionContextEpoch.initialize/prepare(combined)
```

This means:

- Skill guidance belongs to the privileged system context.
- Skill guidance changes enter the model via epoch reconciliation/replacement.
- Skill body/tool visibility is still subject to agent permissions.

## Compaction and History

When compaction completes, a compaction message is projected, including:

- structured rolling summary
- token-bounded recent context

The History loader uses the latest compaction message as a boundary, delivering only the checkpoint and subsequent messages to the Runner. The original transcript is not deleted and remains in the event log and projection table.

## Implementation Steps

1. Implement durable event projector to project `session.next.*` into `session_message`.
2. Implement `SessionHistory.entriesForRunner(sessionID, baselineSeq)`.
3. Implement `SystemContextRegistry` and `SkillGuidance` composition.
4. Implement `SessionContextEpoch.initialize`.
5. Implement `SessionContextEpoch.prepare`: reconcile, replace, changed update.
6. Implement `requestReplacement` and `reset`.
7. Call initialize/prepare/current fence before Runner provider turn.
8. Request epoch replacement after compaction completes.

## Acceptance Criteria

- On first run, if System Context is unavailable, prompt is not promoted.
- Epoch table stores baseline, snapshot, agent, baselineSeq, revision.
- No repeated system messages when sources have not changed.
- `session.next.context.updated` is written when sources change, with atomic revision advancement.
- Agent replacement blocks when source is incomplete, not exposing old agent baseline.
- Epoch resets after Session move, reinitializing on next turn.
- Runner history does not include system contexts prior to baseline.
- Runner history continues from checkpoint boundary after compaction.

## Common Pitfalls

- Treating Context Epoch as a per-turn provider request snapshot.
- Discovering initial context unavailable after prompt promotion, allowing user input into history without baseline.
- Updating only in-memory context changes without writing chronological system messages.
- No revision fence, allowing stale observations to overwrite new agent/location context.
- Treating live deltas as replayable history.
