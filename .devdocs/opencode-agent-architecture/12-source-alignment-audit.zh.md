# 12 Source Code Consistency Audit Report

This report examines whether the current modular documentation aligns with the current source code architecture of opencode. The conclusion is that the general direction is largely aligned, but some documentation describes the "recommended complete architecture for forking" as the "currently implemented architecture." This needs to be narrowed in subsequent revisions. In particular, details regarding tool registration, the permission system, Context Epoch, compaction, and the Session API should be corrected according to the current V2 implementation.

## Audit Scope

The core implementations compared in this audit include:

- V2 Session API: `sessions.create`, `sessions.prompt`, `sessions.resume`, `sessions.interrupt`
- Durable prompt inbox: `session_input`
- V2 Session event family: `session.next.*`
- Local Session execution/coordinator
- V2 Runner's provider turn and tool loop
- LLM runtime event stream
- V2 Tool API, ToolRegistry, Tool output bounding
- PermissionV2
- System Context Epoch
- Automatic compaction

Not all old V1 paths, UI components, or plugin system details have been fully expanded; judgments on MCP, Skill, multimodal attachments, and multi-agent are based on V2 runner parity and the current core implementation.

## Overall Conclusion

| Module | Consistency | Conclusion |
| --- | --- | --- |
| 01 Domain Model & Event Log | Partially Consistent | The durable event concept is correct, but event names, delta durable semantics, and the Context Epoch data model need correction |
| 02 Session Admission | Largely Consistent | Admission/promotion/steer/queue are correct, but `prompt` is not responsible for implicit Session creation |
| 03 Runner & LLM Runtime | Largely Consistent | One provider turn, one `llm.stream`, 25 steps, and subsequent tool loop are correct; interfaces and event names need to align with implementation |
| 04 Tool Registration & Permissions | Requires Major Correction | Current source uses opaque `Tool.make`, naming on registration, scoped overlay, and materialization identity; the `ToolDefinition` field model in the documentation is too generalized |
| 05 Context & History Projection | Partially Consistent | Context Source/Epoch concept is correct, but the current Epoch is Session-level baseline/snapshot/revision, not a provider-turn request snapshot |
| 06 Multimodal Attachments | Partially Consistent | Durable typed prompt attachments direction is correct; chatbox draft and generic resolver are more like fork recommendations and should be marked as UI/extension layer |
| 07 MCP Integration | Extension Design | MCP capabilities exist in the product, but built-in/MCP/plugin tool materialization in V2 runner is still partial/follow-up |
| 08 Skill Mechanism | Largely Consistent | Skill guidance composes with System Context; Skill scripts/tools should be marked as extensions |
| 09 Multi-Agent & Subagents | Partially Consistent | Agent/task/subagent direction is correct; capabilities like agent switch in V2 still have unavailable/follow-up items |
| 10 Compaction, Interruption & Recovery | Partially Consistent | Automatic compaction and interruption concepts are correct; manual compact and post-crash recovery should not be documented as fully implemented |
| 11 Forking Roadmap | Can Be Retained | Suitable as an extension roadmap, but need to distinguish "currently available" from "subsequent additions" |

## Key Inconsistencies and Correction Suggestions

### 1. Session API: `prompt` Does Not Implicitly Create a Session

Some parts of the documentation describe `SessionService.prompt({ sessionID? })` as capable of creating a Session when `sessionID` is absent. The current V2 core interfaces are:

```ts
type CreateInput = {
  id?: SessionID
  agent?: AgentID
  model?: ModelRef
  location: LocationRef
}

type PromptInput = {
  id?: MessageID
  sessionID: SessionID
  prompt: Prompt
  delivery?: "steer" | "queue"
  resume?: boolean
}
```

Actual semantics:

- `sessions.create(...)` creates or reuses a Session separately.
- `sessions.prompt(...)` requires an existing `sessionID`.
- `id` is the prompt message ID; it is generated if omitted.
- When `resume !== false`, `SessionExecution.wake(sessionID, admittedSeq)` is called after admission.
- Exact retries are determined by the same message ID and equivalent prompt/delivery; conflicts throw a `PromptConflictError`.

Documentation correction direction:

- Change all descriptions of `prompt` implicitly creating a Session to "the upper layer can call create first, then prompt".
- Change `SessionInputRow.status` semantics to `admittedSeq/promotedSeq`.

### 2. Durable Boundaries of Prompt Admission & Promotion Are Largely Correct

The documentation's distinction between `admitted` and `promoted` is correct, but event names should be changed to the current implementation:

```text
session.next.prompt.admitted
session.next.prompt.promoted
```

Projection semantics:

- `admitted` only enters the durable inbox.
- Only `promoted` is projected as a user message.
- The V1 compatibility path may still publish old `Prompted` events, but the V2 main path is PromptLifecycle.

### 3. SessionExecution Interface Needs to Align with Implementation

The current interface is:

```ts
interface SessionExecution {
  resume(sessionID: SessionID): Effect<void, RunError>
  wake(sessionID: SessionID, seq?: number): Effect<void, RunError>
  interrupt(sessionID: SessionID, seq?: number): Effect<void>
}
```

Actual semantics:

- `resume` explicitly runs and joins an active drain or forces a run.
- `wake` is an advisory signal; repeated wakes coalesce.
- `interrupt` interrupts the active ownership chain owned by the current process; idle/missing is a no-op.

The documentation's `getState` and `AbortController` style can serve as recommendations for fork implementation, but are not the current Effect-native core interfaces.

### 4. Runner Loop Direction Is Correct, But Interfaces and Details Need Correction

The documentation's statements "each provider turn calls `llm.stream(request)` only once", "tool results are persisted, then history reloaded before continuation", and "maximum 25 steps" are consistent with the source code.

Points needing correction:

- The current Runner interface is `run({ sessionID, force? })`, not `drain({ signal })`.
- The current cancellation mechanism is Effect interruption, not an explicit `AbortSignal`.
- Before a run starts, the Runner calls `failInterruptedTools(sessionID)` to mark pending/running local tools from the previous process as `Tool execution interrupted`.
- `wake` calls the provider only when there are pending steer/queue inputs; `force` resume attempts at least one provider turn even if no eligible input exists.
- Once a complete local `tool-call` is received during the provider stream, the Runner durably publishes `session.next.tool.called` and eagerly starts a settlement fiber; after the provider stream ends, it awaits all tool fibers.
- Provider-executed tools do not go through local registry settlement; only provider metadata and provider settlement are retained.
- On context overflow, before durable assistant output/tool activity, one compaction attempt is made, followed by rebuilding the same logical provider turn.

### 5. LLM Event Names Need to Align with `@opencode-ai/llm`

The documentation uses generalized names like `response.started` and `tool_call.completed`. The current Runner consumes events similar to:

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
  | { type: "tool-call"; id: string; name: string; input: unknown; providerExecuted?: boolean }
  | { type: "tool-result"; id: string; name: string; result: ToolResultValue; output?: ToolOutput }
  | { type: "tool-error"; id: string; name: string; message: string }
  | { type: "provider-error"; error: unknown }
```

Delta events have two layers of semantics:

- Live-only events: for UI viewing during connection.
- ended/success/failed durable events: for replay and Session history projection.

### 6. V2 Tool API Differs Most from Documentation

The documentation currently describes tools as public `ToolDefinition` with fields like `name/source/risk/permission/execute`. The current source code is not like this.

The current core tool definition is opaque:

```ts
type Definition<Input, Output>
type AnyTool = Definition<any, any>

function make<Input, Output>(config: {
  description: string
  input: Schema.Codec<any, any, never, never>
  output: Schema.Codec<any, any, never, never>
  execute: (
    input: Schema.Type<Input>,
    context: Tool.Context,
  ) => Effect<Schema.Type<Output>, ToolFailure>
  toModelOutput?: (input: {
    input: Schema.Type<Input>
    output: Output["Encoded"]
  }) => ReadonlyArray<Tool.Content>
}): Definition<Input, Output>
```

Tools do not have a built-in name; the name is determined by the record key during registration:

```ts
tools.register({
  read,
  grep,
  bash,
})
```

`Tool.Context` currently only has invocation identity:

```ts
interface Tool.Context {
  sessionID: SessionID
  agent: AgentID
  assistantMessageID: MessageID
  toolCallID: ToolCallID
}
```

Core laws:

- `Tool.make` has only one executor.
- Input/output codecs are the only schema boundaries.
- Execution sees decoded input.
- Projection sees encoded output.
- `toModelOutput` is a pure function and does not receive invocation identity.
- Tool output bounding occurs after registry settlement, not inside the tool.

Documentation correction direction:

- The Module 04 documentation should focus on `Tool.make` and opaque definitions.
- `source/risk/visibility/permission descriptor` can serve as a fork extension layer, not as current core tool fields.

### 7. ToolRegistry: Registration, Materialization, Stale Rejection Need More Accuracy

The true core of the current ToolRegistry is:

```ts
interface ToolRegistry {
  materialize(permissions?: PermissionRuleset): Effect<Materialization>
  register(tools: Readonly<Record<string, AnyTool>>): Effect<void, RegistrationError, Scope>
}

interface Materialization {
  definitions: ReadonlyArray<ToolDefinition>
  settle(input: ExecuteInput): Effect<Settlement, ToolOutputStoreError>
}
```

Key semantics:

- Registry is Location-scoped.
- Both process application tools and Location tools use the same register capability, but services and stores are separate.
- Within the same placement, the latest active registration wins.
- Closing the Scope only removes that registration; if a previous registration with the same name exists, it is revealed.
- Location registration takes precedence over process application registration.
- Provider-turn materialization captures advertised registration identity.
- During settlement, if the registration has been removed or replaced, it returns a stale tool call and does not execute the handler.
- Invalid input does not execute the tool.
- Invalid output does not produce a successful settlement.
- `materialize` removes wholly disabled tools based on agent permission rules.

This is more specific and more important than the documentation's "filtering by source listForSession".

### 8. Permission System: Not `PermissionScope once/session/workspace/global`

The permission model in the documentation is too generalized. The current `PermissionV2` is action/resource/effect rules:

```ts
type Effect = "allow" | "ask" | "deny"

type Rule = {
  action: string
  resource: string
  effect: Effect
}

type AssertInput = {
  id?: PermissionID
  sessionID: SessionID
  agent?: AgentID
  action: string
  resources: string[]
  save?: string[]
  metadata?: Record<string, unknown>
  source?: {
    type: "tool"
    messageID: string
    callID: string
  }
}

type Reply = "once" | "always" | "reject"
```

Actual rules:

- `evaluate(action, resource, ...rulesets)` uses wildcards and takes the last matching rule.
- Defaults to `ask` when no match is found.
- Agent-configured rules check `deny` first.
- Saved project approvals are appended as allow rules.
- `assert` on `ask` publishes `permission.v2.asked` and waits for a Deferred.
- `reply("always")` writes `save` resources to project saved approvals.
- `reply("reject")` rejects pending permissions for the same Session.

Documentation correction direction:

- Change once/session/workspace/global scopes to the current `once/always/reject` replies and project saved approvals.
- Matchers for paths, commands, networks, etc., can remain as recommendations for how tools construct `action/resources/metadata/save`, not as core data structures of PermissionV2.
- Permissions are not registry-injected helpers; trusted tools call `PermissionV2.assert` within execute.

### 9. Context Epoch: Currently Session Baseline, Not Provider-Turn Snapshot

The documentation currently describes Context Epoch as a snapshot per request round, containing `providerTurnID/toolNames/attachmentIDs/tokenEstimate`. The current source code does not follow this model.

The current Context Epoch is expressed as:

```ts
type SessionContextEpoch = {
  sessionID: SessionID
  baseline: string
  snapshot: SystemContext.Snapshot
  agent: AgentID
  baselineSeq: number
  replacementSeq?: number
  revision: number
}
```

Key semantics:

- An initial complete observation initializes the epoch, occurring before pending prompts become model-visible.
- Subsequent safe boundary reconciles current sources.
- Reuses baseline when unchanged.
- Publishes a `session.next.context.updated` durable system message when changed, atomically advancing snapshot/revision.
- Agent switch/model switch/compaction can request lazy baseline replacement.
- Location move resets the epoch.
- Epoch is fenced against authoritative Session Location/effective agent/revision to avoid stale context durable admission.

### 10. System Context Source Scope Needs Narrowing

Sources that have already entered the V2 baseline include:

- environment facts
- host-local date
- ambient global/upward project instructions
- selected-agent available-skill guidance
- Location-wide sources from System Context Registry

Some V1 parity items are still partial/missing, such as configured remote/nested instructions, provider-family baseline, per-prompt system/tool override, plugin transforms, and structured output policies. If documented as fully implemented, these need to be marked as follow-up.

### 11. Multimodal Attachments: Direction Correct, But Distinguish Core from UI/Extensions

The source specification already has durable typed prompt attachments in V2; Prompt carries structured fields like `files/agents/references`. The documentation's ChatDraft, data URL resolver, and ImageNormalizer are reasonable fork designs, but not all should be documented as currently implemented core paths.

More accurate framing:

- Core focuses on durable admission and provider request lowering of typed prompt attachments/references.
- UI layer handles chatbox attachment selection, drag-and-drop, paste, and preview.
- Local paths, MCP resources, and media materialization still have partial/missing items in the V2 parity table; complete normalization for all file types cannot be assumed.

### 12. MCP: Current Documentation Looks More Like a Target Design

MCP configuration, UI context, and tests exist in the opencode product, but the V2 runner parity explicitly marks "policy-filtered built-in, MCP, and plugin tools" as partial/follow-up. The MCP Client Manager, resource/prompt service, and tool wrapping described in the current module documentation are target designs for forking and should not be documented as fully implemented in V2 core.

Correction direction:

- Add a note at the top of Module 07: "The current product has MCP capabilities; V2 core materialization is still partial. The following is the design for a complete MCP module when forking."
- MCP tools should ultimately be converted to ordinary `Tool.make`/`tools.register` registrations, not special execution channels.

### 13. Skill: Guidance Aligned, Scripting Tools Are Extensions

The current V2 runner loads `SkillGuidance`, composes it with the result from the System Context Registry, and incorporates it into the Context Epoch. The documentation's direction that "Skill guidance acts as a Context Source" is correct.

Items needing correction:

- Skill scripts/Skill-provided tools should not be documented as current core paths.
- If forking to tool-ify Skills, `Tool.make`, `tools.register`, and `PermissionV2.assert` should still be used.

### 14. Multi-Agent: Direction Correct, But Some Session Operations Unavailable

Agent selection, agent prompt, agent permissions, and task/subagent align with the product architecture; however, `switchAgent`, `shell`, `skill`, `compact`, and `wait` public operations in the current V2 Session core still return `OperationUnavailable`. The documentation needs to indicate which are current V2 main paths and which are subsequent parity.

### 15. Compaction: Automatic Compaction Exists, Manual Compact Not Fully Exposed

The documentation's compaction concept is correct, but the data model does not align with the current implementation.

Current durable events:

```text
session.next.compaction.started
session.next.compaction.delta    // live-only
session.next.compaction.ended    // durable v2, contains text + recent
```

Actual semantics:

- Estimate whether the request exceeds the context window minus reserved headroom before a provider turn.
- When exceeding, select older complete turns for compaction.
- After completion, write a structured rolling summary and token-bounded recent context.
- The compaction message is projected as a model-visible checkpoint.
- Provider-native assistant/reasoning/tool messages are not retained across compaction boundaries.
- On provider context overflow, before durable output/tool activity starts, one overflow-triggered compaction attempt occurs.
- `sessions.compact(...)` currently returns `OperationUnavailable` in public V2 core; manual compact is a follow-up.

### 16. Interruption & Recovery: Interruption Exists, Post-Crash Recovery Still Deferred

The documentation is correct about principles like idle interrupt being a no-op, durable inbox being preserved, and history not being deleted on interrupt.

Items needing clarification:

- Interrupt writes `session.next.interrupt.requested`, then interrupts the current local ownership chain.
- The Runner cleans up current projected pending/running tools as `Tool execution interrupted`.
- Post-crash activity recovery is explicitly deferred; `wake` does not infer whether promoted/provider-dispatched work can be safely retried.
- An explicit `run` can continue from durable projected history.

## Recommended Priorities for Documentation Revisions

1. First revise Module 04 Tool Registration & Permissions: This module has the largest deviations and the greatest impact on forking.
2. Then revise Module 05 Context Epoch: Avoid describing per-turn request snapshots as the current implementation.
3. Then revise Modules 02/03: Align API, Runner loop, and LLM event names with the source code.
4. Then revise Module 10: Distinguish between automatic compaction, manual compact, and post-crash recovery.
5. Finally revise Modules 06/07/08/09: Layer UI/extension designs and current V2 parity.

## Core Judgments That Can Be Directly Retained from Current Documentation

- Separation of durable prompt admission and promotion.
- `steer` merges into the next safe boundary of active activity by default.
- `queue` FIFO opens future activity.
- Local drain is serialized for the same Session; different Sessions can run concurrently.
- One provider turn, one `llm.stream(request)`.
- Local tool calls are durably recorded before execution.
- Tool results are persisted, then projected history is reloaded before continuation.
- Maximum provider steps is 25.
- Permissions must be enforced server-side, not relying on UI consent.
- Context Sources must be observable, comparable, and persistable.
- Compaction does not delete the original transcript.
- Interruption and crash recovery cannot blindly replay external side effects.
