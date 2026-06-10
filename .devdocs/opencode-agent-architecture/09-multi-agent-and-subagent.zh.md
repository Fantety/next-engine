# 09 Multi-Agent and Subagents

A multi-agent architecture allows a primary agent to delegate partial tasks to subagents, such as code search, document organization, test validation, and design review. When replicating, subagents should be implemented as controlled sub-sessions or sub-activities, rather than temporarily invoking another model in memory and stitching the results back into text.

> Source Code Consistency Note: Agent selection, agent prompt, agent permissions, and task/subagent are important directions for the opencode architecture; however, operations such as `switchAgent`, `shell`, `skill`, `compact`, and `wait` in the current V2 public Session core still have unavailable/follow-up items. This chapter describes the multi-agent module according to the goal of full replication.

## Module Responsibilities

This module is responsible for:

- Defining the agent configuration model.
- Supporting the primary agent in launching subagents via the `task` tool.
- Creating isolated sessions, contexts, tool sets, and permission boundaries for subagents.
- Returning subagent results to the primary agent in the form of tool results.
- Recording parent-child relationships for UI display and recovery.

This module is not responsible for:

- Replicating an entire Runner; subagents still use the same SessionRunner.
- Bypassing tool permissions.
- Allowing subagents to directly modify the parent session history.

## Agent Model

```ts
type AgentConfig = {
  id: AgentID
  name: string
  description?: string
  model: ModelRef
  systemPrompt: string
  tools: AgentToolPolicy
  permissions: AgentPermissionPolicy
  context: AgentContextPolicy
  skills?: AgentSkillConfig
  subagents?: AgentSubagentPolicy
}

type AgentToolPolicy = {
  enabled?: ToolName[]
  disabled?: ToolName[]
  groups?: string[]
}

type AgentPermissionPolicy = {
  inheritFromParent: boolean
  defaultEffect: "allow" | "ask" | "deny"
  additionalRules?: PermissionRule[]
}

type AgentContextPolicy = {
  inheritSummaryFromParent: boolean
  includeWorkspaceContext: boolean
  maxHistoryTokens?: number
}

type AgentSubagentPolicy = {
  allowedAgentIDs: AgentID[]
  maxDepth: number
  maxConcurrent: number
}
```

Design highlights:

- An agent is a configuration, not a runtime thread.
- A session is bound to an agent.
- Subagents use their own AgentConfig and can use a different model than the parent agent.

## Agent Service

```ts
interface AgentService {
  resolve(agentID?: AgentID): Promise<AgentConfig>
  resolveForSession(sessionID: SessionID): Promise<AgentConfig>
  list(): Promise<AgentConfig[]>
}
```

Resolution order:

1. User-specified agent.
2. Session-bound agent.
3. Workspace default agent.
4. System default agent.

## Task Tool

The most natural entry point for multi-agent functionality is the built-in `task` tool.

```ts
type TaskToolInput = {
  agentID?: AgentID
  description: string
  prompt: string
  expectedOutput?: string
  context?: {
    files?: string[]
    messageIDs?: MessageID[]
    attachments?: AttachmentID[]
  }
}

type TaskToolOutput = {
  childSessionID: SessionID
  summary: string
  result: string
  status: "completed" | "failed" | "interrupted"
}
```

Tool definition:

```ts
const taskTool = Tool.make({
  description: "Run a delegated subagent task and return its result.",
  input: TaskToolInput,
  output: TaskToolOutput,
  execute: runTaskTool,
})

yield* tools.register({ task: taskTool })
```

## Subagent Call Chain

```text
Main model returns task tool_call
  -> ToolSettlement.resolve("task")
  -> PermissionService.assert(agent.spawn)
  -> create child Session(parentSessionID = current session)
  -> child SessionService.prompt(resume = true)
  -> SessionExecution.wake(childSessionID)
  -> wait child settles
  -> summarize child final result
  -> assistant.tool.completed in parent session
```

Pseudocode:

```ts
async function runTaskTool(input: ToolExecutionInput<TaskToolInput>): Promise<ToolExecutionOutput<TaskToolOutput>> {
  const childAgent = await agentService.resolve(input.arguments.agentID)
  const childSession = await sessionService.create({
    agentID: childAgent.id,
    location: input.location,
    metadata: {
      parentSessionID: input.sessionID,
      parentToolCallID: input.toolCallID,
    },
  })

  await sessionService.prompt({
    sessionID: childSession.id,
    location: input.location,
    parts: buildChildPrompt(input.arguments),
    delivery: "queue",
    resume: true,
  })

  const result = await subagentMonitor.waitForSettled({
    childSessionID: childSession.id,
    signal: input.signal,
  })

  return {
    output: result.summary,
    data: {
      childSessionID: childSession.id,
      summary: result.summary,
      result: result.finalText,
      status: result.status,
    },
  }
}
```

## Subagent Context Isolation

Subagents should not automatically see the full history of the parent session. It is recommended to pass only:

- Task description.
- MessageIDs specified by the parent agent.
- Necessary files or attachments.
- Parent context summary rather than the complete conversation.

```ts
type ChildSessionContext = {
  parentSessionID: SessionID
  parentToolCallID: ToolCallID
  inheritedSummary?: string
  selectedMessageIDs: MessageID[]
  selectedAttachmentIDs: AttachmentID[]
}
```

The Context Source can read this structure to generate the system context for the subagent:

```text
[Parent Task]
description...

[Inherited Context]
summary...

[Expected Output]
...
```

## Permission Inheritance

Permission inheritance must be explicit; do not grant all parent agent permissions to the subagent by default.

```ts
type SubagentPermissionContext = {
  parentSessionID: SessionID
  childSessionID: SessionID
  parentGrants: PermissionGrant[]
  inheritedGrants: PermissionGrant[]
  additionalRules: PermissionRule[]
}
```

Inheritance strategy:

- `allow_once` is not inherited.
- `session` grants are not inherited across sessions unless the rule is marked as inheritable.
- `workspace` grants can be inherited, but their source should be recorded.
- High-risk capabilities, such as `process.exec`, `filesystem.write`, and `network.access`, should re-ask for permission.
- Subagent `agent.spawn` is disabled by default or depth-limited.

Permission request displays should indicate:

```text
Subagent <agent name> requests <tool/action>
Parent task: <description>
Risk: ...
```

## Depth and Concurrency Limits

```ts
type SubagentRuntimeLimits = {
  maxDepth: number
  maxConcurrentChildren: number
  maxTotalChildSessions: number
  timeoutMs: number
}
```

Runtime check:

```ts
function assertCanSpawnSubagent(input: {
  parentSessionID: SessionID
  agent: AgentConfig
  requestedAgentID?: AgentID
}) {
  if (currentDepth(input.parentSessionID) >= input.agent.subagents.maxDepth) {
    throw new Error("subagent depth limit reached")
  }
}
```

## Subagent Result

After a subagent completes, the parent agent sees a regular tool result.

```ts
type SubagentResult = {
  childSessionID: SessionID
  finalMessageID?: MessageID
  summary: string
  artifacts?: FileAttachment[]
  status: "completed" | "failed" | "interrupted"
}
```

Output requirements:

- `output` should be concise for the parent model to read.
- `data.childSessionID` allows the UI to display a clickable expandable section.
- Artifacts generated by the subagent can be returned as media.

## Event Design

Child sessions use regular events:

- `session.created`, with payload containing `parentSessionID`.
- `prompt.admitted/promoted`.
- assistant/tool/context events.

Parent sessions use regular tool events:

- `session.next.tool.called` with `tool = "task"`.
- `assistant.tool.started`.
- `assistant.tool.completed`, with payload containing the child session ID.

Optional addition:

```ts
type AgentSpawnedPayload = {
  parentSessionID: SessionID
  childSessionID: SessionID
  parentToolCallID: ToolCallID
  agentID: AgentID
}
```

## UI Projection

The UI can display the task tool result as a "subagent task card":

- Subagent name.
- Status.
- Summary.
- Expandable to view child session events and messages.

However, the source of truth remains the regular sessions and tool events.

## Implementation Steps

1. Implement AgentConfig and AgentService.
2. Add `parentSessionID` to Session.
3. Implement the built-in `task` tool.
4. Add `agent.spawn` to PermissionService.
5. Implement child session creation, prompt, wake, and waiting for settlement.
6. Implement the subagent context source.
7. Implement permission inheritance rules.
8. Add depth/concurrency/timeout limits.
9. Display subagent task cards in the UI projection.

## Acceptance Criteria

- The primary agent calling `task` creates a child session.
- The child session has its own Agent, Context Epoch, and event log.
- The subagent result is returned to the parent session as a tool result.
- Subagents cannot use one-time grants from the parent session by default.
- The task tool is rejected when maxDepth is reached.
- After restart, child sessions can still be found via parentSessionID.

## Common Pitfalls

- Directly invoking another model in memory without a child session, making recovery and auditing impossible.
- Subagents seeing the full parent session history, leaking irrelevant context.
- Subagents inheriting all permissions, leading to privilege escalation from the primary agent.
- The task tool bypassing the unified tool permissions.
- The parent agent waiting for a subagent without timeout and interruption handling.
