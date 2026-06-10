# 08 Skill Mechanism

A Skill is a set of capability descriptions, workflow instructions, scripts, templates, or resources that can be dynamically incorporated into an Agent's context. It differs from a tool: tools are functions that the model can call, whereas a Skill is more like an "operational manual to follow for specific tasks." A Skill can indirectly register tools, but its core output is guidance.

> Source consistency note: The current V2 Runner loads the selected-agent Skill guidance and merges it with the System Context before entering the Context Epoch. Skill scripts or Skill-provided tools should be understood as extension layers; if implemented, they still require `Tool.make`, `tools.register`, and `PermissionV2.assert`.

## Module Responsibilities

This module is responsible for:

- Discovering Skills provided locally and by plugins.
- Parsing Skill manifests and `SKILL.md`.
- Determining which Skills are relevant to the current user request.
- Injecting the guidance of enabled Skills into the Context Epoch.
- Exposing Skill resources, scripts, and templates for use by corresponding modules.

This module is not responsible for:

- Directly calling the model.
- Directly executing Skill scripts; script execution still goes through tools and permissions.
- Injecting all Skills fully into the system prompt.

## Skill Data Model

```ts
type SkillID = string

type SkillManifest = {
  id: SkillID
  name: string
  description: string
  version?: string
  triggers?: SkillTrigger[]
  entry: string
  resources?: SkillResource[]
  tools?: SkillProvidedTool[]
  source: "user" | "plugin" | "system"
}

type SkillTrigger = {
  type: "keyword" | "pattern" | "intent" | "filetype"
  value: string
}

type SkillResource = {
  path: string
  kind: "reference" | "template" | "script" | "asset"
}

type SkillProvidedTool = {
  name: string
  command?: string[]
  inputSchema?: JSONSchema
}
```

Skill content:

```ts
type SkillDocument = {
  id: SkillID
  manifest: SkillManifest
  guidance: string
  frontmatter?: Record<string, unknown>
  loadedAt: number
  hash: string
}
```

## Skill Discovery

```ts
interface SkillDiscovery {
  discover(input: {
    roots: string[]
    pluginIDs?: string[]
  }): Promise<SkillManifest[]>
}
```

Discovery rules:

- Each Skill directory must have at least a manifest or `SKILL.md`.
- The name/description in the `SKILL.md` frontmatter can generate a manifest.
- Plugin Skills and user Skills both enter the same registry, but with different sources.
- Skills with the same name should not be silently overwritten; use source namespacing or report a conflict.

## Skill Registry

```ts
interface SkillRegistry {
  register(manifest: SkillManifest): Promise<void>
  list(input?: { source?: SkillManifest["source"] }): Promise<SkillManifest[]>
  load(skillID: SkillID): Promise<SkillDocument>
  resolve(skillID: SkillID): Promise<SkillManifest>
}
```

Loading strategy:

- Discovery reads only manifests, not large documents.
- `SKILL.md` is loaded only when a trigger matches or the user explicitly enables it.
- Resources referenced by Skills are read on demand, not batched.

## Skill Selection

```ts
interface SkillSelector {
  select(input: {
    sessionID: SessionID
    agent: AgentConfig
    prompt: SessionInputRow
    available: SkillManifest[]
  }): Promise<SelectedSkill[]>
}

type SelectedSkill = {
  skillID: SkillID
  reason: string
  priority: number
  explicit: boolean
}
```

Selection sources:

- User explicitly mentions the Skill name.
- Prompt text matches a keyword/pattern.
- Attachment filetype matches.
- Agent configuration enables by default.
- Plugin or system rules require enabling.

Selection principles:

- Explicit selection takes precedence over automatic selection.
- Automatic selection should be conservative to avoid irrelevant Skills polluting the system prompt.
- When multiple Skills are enabled simultaneously, they are sorted by priority.

## Skill Guidance Injection

The most common way to integrate Skills is as a Context Source.

```ts
interface SkillContextSource extends SystemContextSource {
  id: `skill:${SkillID}`
  domain: "skill"
}
```

Generated content:

```ts
function renderSkillGuidance(skill: SkillDocument): string {
  return [
    `## Skill: ${skill.manifest.name}`,
    skill.manifest.description,
    skill.guidance,
  ].join("\n\n")
}
```

Injection into the Context Epoch:

```text
Prompt admitted
  -> SkillSelector.select(...)
  -> SkillRegistry.load(...)
  -> ContextSourceRegistry registers selected skill sources
  -> ContextEpoch.prepare(...)
  -> system prompt contains selected guidance
```

## Skill Resources

Skills may include references, templates, scripts, and assets.

```ts
interface SkillResourceService {
  read(input: {
    skillID: SkillID
    path: string
    kind: SkillResource["kind"]
  }): Promise<SkillResourceContent>
}

type SkillResourceContent =
  | { kind: "reference"; text: string }
  | { kind: "template"; text: string }
  | { kind: "script"; path: string }
  | { kind: "asset"; attachment: FileAttachment }
```

Strategy:

- References/templates can be read on demand as context.
- Scripts are not executed directly; they must be wrapped as tools or executed by existing shell tools, subject to permissions.
- Assets can be used as attachments or generated files.

## Skill-Provided Tools

If a Skill declares tools, they should not be treated specially; still construct tool values with `Tool.make(...)` and register them via `tools.register(...)`.

```ts
function skillToolToCoreTool(input: {
  skill: SkillManifest
  tool: SkillProvidedTool
}): Tool.AnyTool {
  return Tool.make({
    description: `Tool from skill ${input.skill.name}`,
    input: schemaFromSkillTool(input.tool),
    output: SkillToolOutput,
    execute: (toolInput, context) => runSkillTool(input.skill, input.tool, toolInput, context),
  })
}
```

By default, not exposed to the model unless explicitly allowed by the user or Agent configuration.

## Skill and Agent Configuration

```ts
type AgentSkillConfig = {
  enabledSkillIDs?: SkillID[]
  disabledSkillIDs?: SkillID[]
  autoSelect: boolean
  maxSkillsPerTurn: number
}
```

Rules:

- Disabled takes precedence over auto-select.
- Enabled means default injection.
- `maxSkillsPerTurn` prevents system prompt bloat.

## Event Design

```ts
type SkillSelectedPayload = {
  sessionID: SessionID
  promptID: PromptID
  selected: SelectedSkill[]
}

type SkillLoadedPayload = {
  skillID: SkillID
  hash: string
}
```

Optionally, instead of writing separate events for Skills, record them in the source snapshot of the Context Epoch. If debugging automatic selection, consider writing a `context.skill.selected` event.

## Implementation Steps

1. Implement Skill manifest discovery.
2. Implement `SkillRegistry.load` to read `SKILL.md` on demand.
3. Implement keyword/pattern/filetype selectors.
4. Register selected skills as Context Sources.
5. Record skill sources in the Context Epoch.
6. Implement on-demand reading of Skill resources.
7. If script tools are needed, use ToolRegistry and PermissionService.

## Acceptance Criteria

- When the user explicitly mentions a Skill, that Skill's guidance appears in the current Context Epoch.
- Skills that are not triggered do not fully enter the prompt.
- The hash changes after a Skill document is modified.
- Skill resources are read on demand, without loading the entire resources directory at once.
- Skill script execution triggers permissions rather than running silently.

## Common Pitfalls

- Stuffing all Skills into the system prompt, causing context pollution and cost explosion.
- Skill scripts bypassing tool permissions.
- Automatic selection rules being too aggressive, enabling irrelevant Skills for ordinary tasks.
- Mixing Skill loading with discovery, reading many files at startup.
