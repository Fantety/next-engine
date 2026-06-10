# 03 Agent Runner and LLM Runtime

This chapter describes the tool calling loop and model runtime according to the current opencode V2 Runner. The core responsibilities of the Runner are: starting from a durable Session state, promoting input within safe boundaries, preparing the System Context Epoch, assembling an `LLM.request(...)`, executing one `llm.stream(request)`, publishing `session.next.*` events, settling local tools, and reloading history with continuation when necessary.

## Current Interface

The public interface of the V2 Runner is not `drain(signal)`, but rather:

```ts
interface SessionRunner {
  run(input: {
    sessionID: SessionID
    force?: boolean
  }): Effect<void, RunError>
}
```

Call entry points:

```text
SessionExecution.resume(sessionID)
  -> runner.run({ sessionID, force: true })

SessionExecution.wake(sessionID, seq?)
  -> runner.run({ sessionID, force: false })
```

Semantics:

- `wake` is an advisory signal after durable inbox writes; does not invoke the provider when there is no pending steer/queue.
- `resume` is explicit recovery; joins an active drain or starts a force run, attempting at least one provider turn even without eligible input.
- Cancellation relies on Effect interruption, not an explicit `AbortSignal` parameter.

## Runner Outer Loop

```ts
const MAX_STEPS = 25

function run(input: { sessionID: SessionID; force?: boolean }) {
  const hasSteer = SessionInput.hasPending(input.sessionID, "steer")
  const hasQueue = hasSteer ? false : SessionInput.hasPending(input.sessionID, "queue")
  if (input.force !== true && !hasSteer && !hasQueue) return

  failInterruptedTools(input.sessionID)

  let promotion = hasSteer ? "steer" : hasQueue ? "queue" : undefined
  let openActivity = input.force === true || hasSteer || hasQueue

  while (openActivity) {
    let needsContinuation = true

    for (let step = 0; step < MAX_STEPS; step++) {
      needsContinuation = runTurn(input.sessionID, promotion)
      promotion = "steer"

      if (!needsContinuation) {
        needsContinuation = SessionInput.hasPending(input.sessionID, "steer")
      }
      if (!needsContinuation) break
    }

    if (needsContinuation) throw new StepLimitExceededError(input.sessionID, MAX_STEPS)

    openActivity = SessionInput.hasPending(input.sessionID, "queue")
    promotion = openActivity ? "queue" : undefined
  }
}
```

Key points:

- A maximum of 25 provider turns per activity.
- Local tool results or steer input entered during active drain trigger continuation.
- After the current activity is settled, if there is queue input, only one queued input is promoted to open the next activity.
- At the start of a run, any pending/running local tools left over from the previous process are durably marked as `Tool execution interrupted`.

## Single Provider Turn

The sequence of a current provider turn:

```text
get Session
  -> fence Location
  -> select effective Agent
  -> initialize Context Epoch if missing
  -> promote eligible input if this turn has promotion
  -> prepare/reconcile Context Epoch
  -> re-read Session and fence agent/model changes
  -> resolve model
  -> load runner history from baselineSeq
  -> tools.materialize(agent.permissions)
  -> build LLM.request
  -> automatic compaction check
  -> llm.stream(request)
  -> publish LLM events
  -> start local tool settlements eagerly
  -> flush fragments
  -> await tool fibers
  -> fail unsettled tools if needed
  -> return needsContinuation
```

Pseudocode:

```ts
function runTurnAttempt(sessionID: SessionID, promotion?: "steer" | "queue") {
  const session = getSession(sessionID)
  if (!sameLocation(session.location, currentLocation)) interrupt()

  const agent = agents.select(session.agent)

  const initialized = SessionContextEpoch.initialize(
    loadSystemContext(agent),
    session.id,
    session.location,
    agent.id,
  )

  if (promotion === "steer") {
    const cutoff = SessionInput.latestSeq(session.id)
    SessionInput.promoteSteers(session.id, cutoff)
  }

  if (promotion === "queue") {
    const cutoff = SessionInput.latestSeq(session.id)
    SessionInput.promoteNextQueued(session.id)
    SessionInput.promoteSteers(session.id, cutoff)
  }

  const system =
    initialized ??
    SessionContextEpoch.prepare(loadSystemContext(agent), session.id, session.location, agent.id)

  const current = getSession(sessionID)
  if (agentOrModelChanged(current, session, agent)) rebuildPreparedTurn()

  const model = models.resolve(session)
  const entries = SessionHistory.entriesForRunner(session.id, system.baselineSeq)
  const toolMaterialization = tools.materialize(agent.permissions)

  const request = LLM.request({
    model,
    providerOptions: { openai: { promptCacheKey: cacheKey(session.id) } },
    system: [agent.system, system.baseline].filter(Boolean).map(SystemPart.make),
    messages: toLLMMessages(entries.map((entry) => entry.message), model),
    tools: toolMaterialization.definitions,
  })

  if (compaction.compactIfNeeded({ sessionID: session.id, entries, model, request })) {
    rebuildPreparedTurn()
  }

  streamOneProviderTurn(request, toolMaterialization)
}
```

## LLM Runtime Request

The current Runner uses the request builder from `@opencode-ai/llm`:

```ts
const request = LLM.request({
  model,
  providerOptions,
  system: SystemPart[],
  messages: LLMMessage[],
  tools: ModelToolDefinition[],
})
```

`system` consists of two parts:

```text
selected agent system prompt
+ SessionContextEpoch baseline
```

`messages` come from the projected Session history, lowered to provider-neutral format via `toLLMMessages(...)`.

`tools` come from `ToolRegistry.materialize(agent.permissions).definitions`, while materialization also returns a `settle(...)` function bound to the same advertised snapshot.

## LLMEvent Types

The events consumed by the Runner follow those from `@opencode-ai/llm`:

```ts
type LLMEvent =
  | { type: "step-start" }
  | { type: "text-start"; id: string }
  | { type: "text-delta"; id: string; text: string }
  | { type: "text-end"; id: string }
  | { type: "reasoning-start"; id: string; providerMetadata?: ProviderMetadata }
  | { type: "reasoning-delta"; id: string; text: string }
  | { type: "reasoning-end"; id: string; providerMetadata?: ProviderMetadata }
  | { type: "tool-input-start"; id: string; name: string }
  | { type: "tool-input-delta"; id: string; name: string; text: string }
  | { type: "tool-input-end"; id: string; name: string }
  | { type: "tool-call"; id: string; name: string; input: unknown; providerExecuted?: boolean; providerMetadata?: ProviderMetadata }
  | { type: "tool-result"; id: string; name: string; result: ToolResultValue; output?: ToolOutput; providerExecuted?: boolean }
  | { type: "tool-error"; id: string; name: string; message: string }
  | ProviderErrorEvent
```

The provider adapter is responsible for normalizing provider-specific streams from OpenAI, Anthropic, Gemini, Bedrock, etc., into these events.

## Event Publishing

The Runner does not construct UI messages directly but uses a publisher to write `session.next.*` events.

### Assistant Step

Published when the first assistant message ID is needed:

```text
session.next.step.started
```

Published when the provider turn ends:

```text
session.next.step.ended
```

Published on failure:

```text
session.next.step.failed
```

### Text and Reasoning

```text
text-start     -> session.next.text.started
text-delta     -> session.next.text.delta      live-only
text-end/flush -> session.next.text.ended      durable

reasoning-start     -> session.next.reasoning.started
reasoning-delta     -> session.next.reasoning.delta live-only
reasoning-end/flush -> session.next.reasoning.ended durable
```

Delta events are for connected UIs; durable replay relies on ended events.

### Tool Input and Tool Call

```text
tool-input-start -> session.next.tool.input.started
tool-input-delta -> session.next.tool.input.delta live-only
tool-input-end   -> session.next.tool.input.ended durable
tool-call        -> session.next.tool.called
```

The `session.next.tool.called` payload stores:

```ts
type ToolCalledPayload = {
  sessionID: SessionID
  assistantMessageID: MessageID
  callID: string
  tool: string
  input: Record<string, unknown>
  provider: {
    executed: boolean
    metadata?: ProviderMetadata
  }
}
```

### Tool Settlement

After local settlement is complete, the Runner converts the settlement into `LLMEvent.toolResult(...)` and passes it to the publisher, which ultimately writes:

```text
session.next.tool.success
session.next.tool.failed
```

Provider-executed tool results are also projected as success/failed, but with `provider.executed = true`, while preserving settlement-side provider metadata.

## Local Tool Call Loop

The key point of the current implementation is eager local-tool execution:

```ts
for await (const event of llm.stream(request)) {
  publish(event)

  if (event.type !== "tool-call") continue
  if (event.providerExecuted) continue

  needsContinuation = true
  const assistantMessageID = publisher.assistantMessageID(event.id)

  FiberSet.run(toolFibers,
    toolMaterialization
      .settle({ sessionID, agent, assistantMessageID, call: event })
      .flatMap((settlement) =>
        publish(LLMEvent.toolResult({
          id: event.id,
          name: event.name,
          result: settlement.result,
          output: settlement.output,
        }), settlement.outputPaths ?? [])
      )
  )
}

publisher.flush()
awaitToolFibers(toolFibers)
```

Why durable publish before execution:

- If the process crashes before the tool side effects, the history will show that the model indeed requested the tool.
- If the process crashes while a tool is running, the next run will mark the projected running/pending tool as interrupted.
- Provider-local call IDs may repeat, so settlement events must include the assistant message ID.

## Continuation Conditions

`runTurn` returns a boolean:

- `true`: continuation is needed, e.g., a local tool was executed in this turn, or a new steer input was received during the active turn.
- `false`: the current activity is settled.

In the loop:

```text
runTurn returns false
  -> check pending steer
  -> if no steer, current activity settled
  -> check pending queue
  -> if queue exists, promote next queued input as new activity
```

## Context Overflow and Compaction

The Runner estimates context pressure using the full request before calling the provider:

```text
if request exceeds model context window - reserved headroom
  -> automatic compaction
  -> rebuild prepared turn from durable state
```

If the provider returns a context overflow and durable assistant output or tool activity has not yet started:

```text
provider overflow before durable output
  -> try one overflow-triggered compaction
  -> rebuild same logical provider turn once
```

If durable output/tool activity already exists, the provider work cannot be replayed and is treated as a normal failure.

## Interruption and Failure Handling

The current implementation propagates cancellation through Effect interruption.

Handling rules:

- Stream interruption or tool fiber interruption clears tool fibers.
- Unsettled tools publish `session.next.tool.failed` with the message `Tool execution interrupted`.
- Unsettled tools are also failed after a provider error.
- Hosted/provider tools are failed when the provider does not return a tool result.
- A rejected question tool interrupts the loop, rather than treating the rejection as a model-visible tool output.

## Implementation Steps

1. Implement `SessionRunner.run({ sessionID, force? })`.
2. Integrate `SessionInput.hasPending/promoteSteers/promoteNextQueued`.
3. Integrate `SessionContextEpoch.initialize/prepare/current`.
4. Implement `SessionHistory.entriesForRunner`.
5. Integrate `ToolRegistry.materialize`.
6. Use a fake `LLMClient.stream` to get text-start/delta/end working.
7. Integrate tool-input and tool-call event publishing.
8. Call materialized `settle` after a local tool-call.
9. Await tool fibers before continuation.
10. Integrate automatic compaction and overflow retry.
11. Integrate interruption and `failUnsettledTools`.

## Acceptance Criteria

- When there is no pending input and it is not a force run, wake does not invoke the provider.
- Force resume attempts a provider turn even without pending input.
- `llm.stream(request)` is called only once per provider turn.
- Text deltas are live-only, but replay can recover the full message from text ended.
- Local tool calls write `session.next.tool.called` first, then execute the tool.
- After tool completion, the next model turn sees the tool result.
- Provider-executed tools preserve provider metadata and do not go through the local registry.
- Throw `StepLimitExceeded` when 25 steps still require continuation.
- Context overflow before output triggers only one compaction retry.
- After interruption, pending/running tools are marked as interrupted.

## Common Pitfalls

- Doing the tool loop inside the provider adapter, making events, permissions, and Context Epoch unrecoverable.
- Using deltas as durable replay cursors.
- Reusing old history after tool completion without reloading from projected history.
- Failing to capture materialization identity, causing old calls to execute new handlers after tool registration changes.
- Blindly replaying a turn that has already produced durable output after provider overflow.
