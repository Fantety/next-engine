# Source Code Consistency Audit Report

This report examines whether the current modular documentation aligns with the current source code architecture of opencode. The conclusion is that the general direction is largely aligned, but some documents have written the "recommended complete architecture for forking" as the "currently implemented architecture." The scope needs to be narrowed in subsequent revisions. In particular, details regarding tool registration, the permission system, Context Epoch, compaction, and the Session API should be corrected according to the current V2 implementation.

## Audit Scope

The core implementations examined in this comparison include:

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

Not all old V1 paths, UI components, or plugin system details are fully expanded; judgments on MCP, Skill, multimodal attachments, and multi-agent are based on V2 runner parity and the current core implementation.

## Overall Conclusion

| Module | Consistency | Conclusion |
| --- | --- | --- |
| 01 Domain Model & Event Log | Partially Consistent | The durable event concept is correct, but event names, delta durable semantics, and the Context Epoch data model need revision |
| 02 Session Admission | Largely Consistent | Admission/promotion/steer/queue are correct, but `prompt` does not implicitly create a Session |
| 03 Runner & LLM Runtime | Largely Consistent | One provider turn, one `llm.stream`, 25 steps, and subsequent tool loops are correct; interfaces and event names need to align with implementation |
| 04 Tool Registration & Permissions | Requires Major Revision | Current source uses opaque `Tool.make`, naming upon registration, scoped overlay, and materialization identity; the `ToolDefinition` field model in documentation is too generalized |
| 05 Context & History Projection | Partially Consistent | The Context Source/Epoch concept is correct, but the current Epoch is Session-level baseline/snapshot/revision, not a provider-turn request snapshot |
| 06 Multimodal Attachments | Partially Consistent | The durable typed prompt attachments direction is correct; chatbox drafts and generic resolvers are more like fork recommendations and should be labeled as UI/extension layer |
| 07 MCP Integration | Extension Design | MCP capabilities exist in the product, but V2 runner's built-in/MCP/plugin tool materialization is still partial/follow-up |
| 08 Skill Mechanism | Largely Consistent | Skill guidance is composed with System Context; Skill scripts/tools sections should be marked as extensions |
| 09 Multi-Agent & Subagents | Partially Consistent | The Agent/task/subagent direction is correct; capabilities like agent switching in V2 still have unavailable/follow-up items |
| 10 Compaction, Interruption & Recovery | Partially Consistent | The automatic compaction and interruption concepts are correct; manual compact and post-crash recovery cannot be described as fully implemented |
| 11 Forking Roadmap | Can Be Retained | Suitable as an extension roadmap, but needs to distinguish between "currently available" and "subsequent additions" |
| 13 Settings, Config Persistence & Import Application | Newly Aligned | Distinguish between core `Config.entries()`, app config merge/update, HTTP API, and frontend local settings |

## Key Inconsistencies & Revision Suggestions

### 1. Session API: `prompt` Does Not Implicitly Create a Session

Some parts of the documentation describe `SessionService.prompt({ sessionID? })` as being able to create a Session when `sessionID` is not provided. The current V2 core interfaces are:

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

- `sessions.create(...)` creates or reuses a Session independently.
- `sessions.prompt(...)` requires an existing `sessionID`.
- `id` is the prompt message ID; if omitted, it is generated.
- When `resume !== false`, `SessionExecution.wake(sessionID, admittedSeq)` is called after admission.
- Exact retries are determined by the same message ID and equivalent prompt/delivery; conflicts throw `PromptConflictError`.

Documentation revision direction:

- Change all descriptions of `prompt` implicitly creating a Session to "upper layer can first create, then prompt."
- Change `SessionInputRow.status` to reflect `admittedSeq/promotedSeq` semantics.

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

- `resume` is explicit execution; it joins active drain or forces a run.
- `wake` is an advisory signal; repeated wakes coalesce.
- `interrupt` interrupts the active ownership chain owned by the current process; idle/missing is a no-op.

The `getState` and `AbortController` style in the documentation can be fork implementation suggestions but are not the current Effect-native core interfaces.

### 4. Runner Loop Direction Is Correct, But Interfaces and Details Need Revision

The documentation's statements that "each provider turn calls `llm.stream(request)` only once," "reload history after tool result persistence and then continue," and "maximum of 25 steps" are consistent with the source code.

Points needing revision:

- The current Runner interface is `run({ sessionID, force? })`, not `drain({ signal })`.
- The current cancellation mechanism is Effect interruption, not an explicit `AbortSignal`.
- Before starting a run, the Runner calls `failInterruptedTools(sessionID)`, marking pending/running local tools from the previous process as `Tool execution interrupted`.
- `wake` calls the provider only when pending steer/queue inputs exist; `force` resume attempts at least one provider turn even without eligible input.
- Upon receiving a complete local `tool-call` in the provider stream, the Runner durably publishes `session.next.tool.called` and eagerly starts a settlement fiber; after the provider stream ends, it awaits all tool fibers.
- Provider-executed tools do not go through local registry settlement; only provider metadata and provider settlement are retained.
- Before durable assistant output/tool activity, context overflow attempts one compaction and then rebuilds the same logical provider turn.

### 5. LLM Event Names Need to Align with `@opencode-ai/llm`

The documentation uses generalized names like `response.started`, `tool_call.completed`. The current Runner consumes events similar to:

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

- Live-only events: for connected UI to view.
- Ended/success/failed durable events: for replay and Session history projection.

### 6. V2 Tool API Has the Largest Deviation from Documentation

The documentation currently describes tools as public `ToolDefinition` with `name/source/risk/permission/execute` fields. The current source code is different.

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

Tools do not have a built-in name; the name is determined by the record key upon registration:

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
- Input/output codec is the only schema boundary.
- Execution sees decoded input.
- Projection sees encoded output.
- `toModelOutput` is a pure function and does not receive invocation identity.
- Tool output bounding occurs after registry settlement, not inside the tool.

Documentation revision direction:

- The 04 document should focus on `Tool.make` and opaque definition.
- `source/risk/visibility/permission descriptor` can be a fork extension layer, not current core tool fields.

### 7. ToolRegistry: Registration, Materialization, Stale Rejection Need to Be More Accurate

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
- Process application tools and Location tools use the same register capability, but services and stores are separate.
- The latest active registration wins within the same placement.
- Closing the Scope only removes that registration; if a previous registration with the same name exists, it reveals the previous one.
- Location registration takes precedence over process application registration.
- Provider-turn materialization captures advertised registration identity.
- If a registration has been removed or replaced at settlement time, the tool call is marked stale, and the handler is not executed.
- Invalid input does not execute the tool.
- Invalid output does not produce successful settlement.
- `materialize` removes wholly disabled tools based on agent permission rules.

This is more specific and important than the documentation's "filter by source listForSession."

### 8. Permission System: Not `PermissionScope once/session/workspace/global`

The permission model in the documentation is too generic. The current `PermissionV2` is based on action/resource/effect rules:

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
- Agent configured rules deny first.
- Saved project approvals are appended as allow rules.
- `assert` encountering `ask` publishes `permission.v2.asked` and waits for a Deferred.
- `reply("always")` writes the `save` resources to project saved approvals.
- `reply("reject")` rejects pending permissions for the same Session.

Documentation revision direction:

- Change once/session/workspace/global scope to the current `once/always/reject` reply and project saved approvals.
- Matchers for paths, commands, networks, etc., can remain as suggestions for how tools construct `action/resources/metadata/save`, rather than being core data structures of PermissionV2.
- Permissions are not registry-injected helpers; trusted tools call `PermissionV2.assert` themselves within execute.

### 9. Context Epoch: Currently Session Baseline, Not Provider-Turn Snapshot

The documentation currently describes Context Epoch as a snapshot per request round, containing `providerTurnID/toolNames/attachmentIDs/tokenEstimate`. The current source code does not use this model.

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

- The initial complete observation initializes the epoch before pending prompts become model-visible.
- Subsequent safe boundaries reconcile current sources.
- Reuses baseline when unchanged.
- When changed, publishes `session.next.context.updated` durable system message and atomically advances snapshot/revision.
- Agent switch/model switch/compaction can request lazy baseline replacement.
- Location move resets the epoch.
- Epoch fenced against authoritative Session Location/effective agent/revision to avoid stale context durable admission.

### 10. System Context Source Scope Needs Narrowing

Sources that have entered the V2 baseline currently include:

- environment facts
- host-local date
- ambient global/upward project instructions
- selected-agent available-skill guidance
- Location-wide sources from System Context Registry

Some V1 parity items remain partial/missing, such as configured remote/nested instructions, provider-family baseline, per-prompt system/tool override, plugin transforms, structured output policy, etc. If the documentation describes these as fully implemented, they need to be marked as follow-up.

### 11. Multimodal Attachments: Direction Correct, But Distinguish Core from UI/Extensions

V2 already has durable typed prompt attachments in the source specification; Prompt carries structured fields like `files/agents/references`. ChatDraft, data URL resolver, and ImageNormalizer in the documentation are reasonable fork designs, but they cannot all be described as currently implemented paths in the core.

More accurate scope:

- Core focuses on durable admission of typed prompt attachments/references and provider request lowering.
- UI layer handles chatbox attachment selection, drag-and-drop, paste, and preview.
- Local paths, MCP resources, and media materialization still have partial/missing items in the V2 parity table; full normalization for all file types cannot be assumed.

### 12. MCP: Current Documentation Looks More Like a Target Design

MCP configuration, UI context, and tests exist in the opencode product, but V2 runner parity explicitly marks "policy-filtered built-in, MCP, plugin tools" as partial/follow-up. The MCP Client Manager, resource/prompt service, and tool wrapping described in the current module documentation are target designs for forking and cannot be described as fully implemented in V2 core.

Revision direction:

- Add a note at the top of the 07 document: "The current product has MCP capabilities; V2 core materialization is still partial. The following is the design for forking a complete MCP module."
- MCP tools should ultimately be converted to ordinary `Tool.make`/`tools.register` registrations, not special execution channels.

### 13. Skill: Guidance Aligned, Scripts/Tools Are Extensions

The current V2 runner loads `SkillGuidance` and composes it with the results from the System Context Registry into the Context Epoch. The documentation's direction of "Skill guidance as a Context Source" is correct.

Points needing revision:

- Skill scripts/Skill-provided tools should not be described as current core paths.
- If forking to toolify Skills, still use `Tool.make`, `tools.register`, and `PermissionV2.assert`.

### 14. Multi-Agent: Direction Correct, But Some Session Operations Unavailable

Agent selection, agent prompt, agent permissions, and task/subagent direction align with the product architecture; however, `switchAgent`, `shell`, `skill`, `compact`, and `wait` public operations in the current V2 Session core still return OperationUnavailable. The documentation needs to indicate which are current V2 main paths and which are subsequent parity.

### 15. Compaction: Automatic Compaction Exists, Manual Compact Not Fully Exposed

The compaction concept in the documentation is correct, but the data model does not align with the current implementation.

Current durable events:

```text
session.next.compaction.started
session.next.compaction.delta    // live-only
session.next.compaction.ended    // durable v2, contains text + recent
```

Actual semantics:

- Before a provider turn, estimate whether the request exceeds the context window minus reserved headroom.
- When exceeding, select older complete turns for compaction.
- After completion, write structured rolling summary and token-bounded recent context.
- Compaction message is projected as a model-visible checkpoint.
- Provider-native assistant/reasoning/tool messages are not retained across compaction boundaries.
- When provider context overflows and durable output/tool activity has not yet started, one overflow-triggered compaction is attempted.
- `sessions.compact(...)` currently returns OperationUnavailable in the public V2 core; manual compact is a follow-up.

### 16. Interruption & Recovery: Current Interruption Exists, Post-Crash Recovery Still Deferred

The documentation's principles, such as idle interrupt no-op, preserving durable inbox, and not deleting history when not interrupted, are correct.

Clarifications needed:

- Interrupt writes `session.next.interrupt.requested` and then interrupts the current local ownership chain.
- The Runner cleans up current projected pending/running tools to `Tool execution interrupted`.
- Post-crash activity recovery is explicitly deferred; wake does not infer whether promoted/provider-dispatched work can be safely retried.
- Explicit `run` can continue from durable projected history.

## Recommended Documentation Revision Priorities

1. First, revise 04 Tool Registration & Permissions: This is the module with the largest deviation and greatest impact on forking.
2. Next, revise 05 Context Epoch: Avoid describing per-turn request snapshots as the current implementation.
3. Next, revise 02/03: Align API, Runner loop, and LLM event names with the source code.
4. Next, revise 10: Distinguish between automatic compaction, manual compact, and post-crash recovery.
5. Finally, revise 06/07/08/09: Layer UI/extension designs and current V2 parity.

### 17. Settings, Config Persistence & Import Application

The newly added configuration document is consistent with the current source code:

- Core V2 `Config.Service` only exposes `entries()`, returning documents/directories sorted from lowest to highest priority.
- Core Location config is read once when opened; subsequent calls reuse the snapshot until the Location/instance is rebuilt.
- App `Config.Service` handles merging global, project, remote, environment, and managed configs, providing `get/update/updateGlobal/invalidate`.
- HTTP `PATCH /config` writes to `config.json` in the current instance directory, then marks the instance for disposal, taking effect upon instance rebuild.
- Provider and Skill are not directly modified by ConfigService; instead, config plugins import entries into the Catalog and Skill sources.
- Frontend settings use a local `settings.v3` persisted store, separate from the server-side opencode config file.

## Core Judgments That Can Be Directly Retained in the Current Documentation

- Separation of durable prompt admission and promotion.
- `steer` merges into the next safe boundary of active activity by default.
- `queue` FIFO opens future activity.
- Local drain is serial for the same Session; different Sessions can be concurrent.
- One provider turn, one `llm.stream(request)`.
- Local tool calls are durably recorded before execution.
- After tool results are persisted, reload projected history and then continue.
- Maximum provider steps is 25.
- Permissions must be enforced on the server side, not relying on UI awareness.
- Context Sources need to be observable, comparable, and durable.
- Compaction does not delete the original transcript.
- Interruption and crash recovery cannot blindly replay external side effects.
