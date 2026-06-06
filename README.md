# NEXT Engine

NEXT Engine 是基于 Godot Engine 深度定制的 AI 游戏开发编辑器。它保留 Godot 成熟的 2D/3D 编辑、节点系统、资源管理、脚本编辑和运行调试能力，并把 AI Agent 融入编辑器工作流，让项目理解、对话协作、场景编辑、脚本修改、Shader 编写和变更审查都可以在同一个开发环境中完成。

<p align="center">
  <img src="logo.png" width="400" alt="NEXT Engine logo">
</p>

## 项目愿景

NEXT Engine 希望把 AI 从一个外部聊天窗口变成编辑器内部的开发伙伴。它可以读取项目结构、理解当前编辑器状态、调用受控工具完成具体操作，并把每一次高风险写入都纳入可确认、可审查、可撤销的流程中。

当前开发线基于 Godot upstream `master` 持续合并更新。Agent 功能先在 master 线上维护，后续等待 Godot 4.7 stable 发布后再合入对应稳定分支，减少 master 与稳定分支之间的渲染、平台和 thirdparty 差异带来的维护成本。

核心方向：

- 保留 Godot 原有编辑器体验和项目兼容性。
- 让 AI Agent 直接理解游戏项目、场景、脚本和资源上下文。
- 用工具调用连接模型能力与真实编辑器能力。
- 对文件写入、场景修改和外部工具调用提供权限控制。
- 通过变更审查面板让 AI 修改可追踪、可比较、可回退。
- 以解耦架构持续扩展模型、工具、MCP、Skill、Rules、上下文和 Agent 工作流。

## 双模式架构

NEXT Engine 提供两种 AI 工作模式，可通过编辑器右上角开关随时切换：

### 普通模式（Normal Mode）

单 Agent 自由对话模式。一个 MainAgent 挂载全部工具（项目上下文、场景编辑、脚本编辑、Shader 编辑等），适合快速问答、小型代码生成、场景操作和资产建议。用户通过右侧 AI Dock 发送消息，Agent 通过 function calling 调用工具执行操作，结果实时流式返回。

### NEXT 模式（NEXT Mode）

里程碑式多 Agent 协作开发模式。由 **规划 Agent** 将创意需求拆解为里程碑和任务列表，再由 **脚本 Agent**、**场景 Agent**、**着色器 Agent** 按依赖顺序分步执行，**审查 Agent** 负责验收反馈。完整工作流包括：

1. **创意启动** — 在需求描述区写下游戏想法。
2. **里程碑生成** — 规划 Agent 生成里程碑及任务列表，每个任务指派到特定 Agent。
3. **人类校准** — 审查编辑里程碑和任务，调整依赖关系和执行顺序。
4. **逐里程碑执行** — 按依赖顺序自动调用对应 Agent，场景 Agent 总在脚本和着色器之后执行。
5. **体验验收** — 在引擎内运行生成的主场景，通过反馈面板提出修改意见，形成「游玩-反馈-修改」循环。
6. **里程碑通过** — 确认完成后锁定为基线，继续下一个里程碑。

NEXT 模式支持多工作流管理（创建、加载、删除、继续执行），中断时自动保存检查点，恢复后从断点继续。所有产出自动进入资产注册表，受保护的用户资产不可被 Agent 修改。

## 功能亮点

### Godot 编辑器基础

NEXT Engine 继承 Godot Engine 的核心开发能力：

- 2D / 3D 场景编辑。
- 节点树、Inspector、资源文件系统和脚本编辑器。
- 项目管理、运行调试和编辑器 Dock 工作流。
- Windows 平台编辑器构建与运行。

AI 功能作为编辑器侧增强接入，不替代 Godot 原生工作流，也不要求开发者离开熟悉的编辑器环境。

### AI Agent Dock

右侧 AI Dock 提供面向项目开发的对话入口：

- 选择模型并发送消息。
- 支持 OpenAI-compatible Provider。
- 支持流式文本返回。
- 支持 Markdown 消息渲染。
- 支持历史会话管理。
- 会话按项目隔离，打开项目后自动恢复最近会话。
- 展示当前会话 token 使用和上下文估算信息。
- 工具调用、审批和执行结果会进入统一消息链路。
- 顶部提供 MCP 和 Skill 状态入口，以轻量列表展示名称和状态色标。
- 复杂任务执行时可在输入框上方显示可展开/收起的计划列表。
- 模式切换开关可在普通模式和 NEXT 模式间无缝切换，状态不丢失。

AI Dock 不只是问答窗口，它是 Agent Runtime、项目上下文、工具调用和变更审查的入口。

### 模型配置

AI Settings 提供模型配置管理：

- 添加自定义模型供应商。
- 配置 Provider、Base URL、API Key、模型 ID 和显示名称。
- 同一供应商和同一模型可以保存多个配置。
- 支持新增、编辑、删除模型配置。
- 高级配置支持模型请求超时、输入上下文预算、上下文文档预算、历史消息预算、工具结果预算、最少保留最近消息数、工具调用轮次和最大输出 token。
- 模型高级配置会进入 `AIProviderConfig`，并在会话运行时绑定到 Agent Runtime 与 Context Manager。
- NEXT 模式下支持为每个子 Agent（规划、脚本、场景、着色器、审查）独立分配模型配置。
- 预留并接入 MCP、Skill、Rules 等 Agent Coding 常见配置入口。

### MCP 工具接入

NEXT Engine 支持通过 MCP 扩展 Agent 能力，让外部工具以受控方式进入编辑器中的 AI 工作流。

当前 MCP 能力包括：

- 支持 `stdio`、`streamable_http`、`sse` 三类 transport。
- 支持从 JSON 导入 MCP server 配置。
- 支持在 AI Settings 中添加、编辑、启用、禁用和删除 MCP server。
- AI Dock 顶部提供轻量 MCP 状态按钮，点击后展开 server 列表。
- 设置页 MCP 列表显示状态色标：可用、不可用、检查中、禁用。
- 启动和配置变更时异步检查 MCP server，不阻塞 UI。
- 初始化失败会显示非阻塞提示，并跳过不可用 server。
- MCP tool 进入模型前会去重，避免上下文中出现重复工具。
- MCP tool 默认需要用户确认后执行。

MCP discovery、状态管理和工具快照由独立服务负责，Session 只消费可用工具快照，避免多会话重复发现和重复注册。

### AgentSkill

NEXT Engine 支持 Prompt/Context 类型的 AgentSkill，用于把可复用的工作流说明、项目约定和专业提示接入 Agent 上下文。当前 Skill 采用渐进披露流程：

- AI Settings 中可以添加、编辑、启用、禁用和删除 Skill。
- 支持选择本地 Skill 文件夹导入，系统会扫描其中的 `SKILL.md` 并生成 Skill 配置。
- 会话上下文只注入启用 Skill 的名称和描述列表，不默认注入全文。
- Agent 可通过只读工具 `agent.activate_skill` 按 `skill_id` 激活某个 Skill，并读取完整内容。
- 当前 Skill 只提供 prompt/context 指令，不执行脚本、不启动进程、不读取任意资源，也不会自动授予工具权限。
- AI Dock 顶部 Skill 列表展示 AI Settings 中已添加的 Skill，而不是仅展示本次会话激活过的 Skill。

后续可以在保持安全边界的前提下扩展 tool bundle、资源引用或更强 Skill 类型，但这些能力不会复用当前 prompt/context 激活路径作为执行入口。

### Rules

Rules 用于把用户配置的短规则稳定注入 Agent 上下文，适合保存项目偏好、回答约束和团队约定。

- AI Settings 中提供 Rules 表格，可添加、编辑、启用、禁用和删除 Rule。
- 每条 Rule 限制在 100 字以内，避免把规则系统变成大段提示词存储。
- 启用的 Rule 会由 Rules Context Provider 汇总为 `User Rules` 上下文文档。
- Context Manager 会在构建模型消息时自动拼接 Rules，使 Agent 在回答和工具调用时遵守这些约束。

### Agent 计划管理（普通模式）

Agent 计划管理用于复杂多步骤任务。模型可调用 `agent.manage_plan` 创建和更新当前任务计划，AI Dock 会在 Composer 上方同步展示计划状态。

- 同一时刻只允许一个 active plan。
- 计划由标题和任务列表组成，任务状态支持 `pending`、`in_progress`、`completed`。
- Agent 应在复杂任务开始前创建计划，并在完成具体任务后及时标记完成。
- 所有任务完成后，计划会自动归档并释放 active plan。
- 简单的一步任务不需要创建计划。
- 系统提示词会要求 Agent 在需求不明确时先追问用户确认，再继续执行。

### Agent Runtime

Agent Runtime 使用 OpenAI-compatible function calling 架构：

- Runtime 负责多轮模型请求和工具调用循环。
- Provider 层负责模型配置、HTTP 请求、流式响应解析和协议转换。
- Tool Registry 统一管理工具 schema、权限和执行入口。
- Context Manager 负责整理系统提示、历史消息和项目上下文，并控制上下文预算。
- Session 层负责 UI 状态、消息持久化、会话切换和运行结果回写。
- 普通模式的计划管理工具与普通工具共用权限、执行和消息链路，但计划状态由独立 Plan Manager 维护。
- NEXT 模式的里程碑和任务数据由 `AINextProjectState` 管理，工作流快照由 `AINextWorkflowStore` 持久化。

这种结构让模型请求、上下文、工具、权限和 UI 解耦，便于持续扩展新的工具和新的 Agent 行为。

### 项目上下文工具

AI Agent 可以读取项目上下文，更准确地理解当前开发任务：

- 查看项目文件树。
- 读取项目文件。
- 搜索项目文本。
- 获取当前编辑器上下文。

这些只读工具帮助模型理解项目结构、当前场景、相关脚本和编辑器状态，是后续自动化编辑的基础。

### 场景编辑工具

NEXT Engine 已提供一组编辑器内场景操作工具：

- 创建场景。
- 打开场景。
- 保存当前场景。
- 创建节点。
- 删除节点。
- 重命名节点。
- 移动节点。
- 设置节点属性。
- 列出节点属性。
- 创建文件夹。

场景编辑工具优先封装 Godot 编辑器内部接口和 Undo/Redo 能力，避免把场景文件当作普通文本直接修改。

### 脚本与 Shader 工具

AI Agent 可以辅助完成基础 GDScript 和 GDShader 编辑任务：

- 创建脚本。
- 删除脚本。
- 编写或覆盖脚本。
- 基于函数级定位修改 GDScript 函数。
- 将脚本绑定到节点。
- 从节点解绑脚本。
- 创建并应用 ShaderMaterial。
- 创建或修改 GDShader。

脚本和 Shader 写入会进入权限与审查流程，让开发者在享受自动化效率的同时保留确认权。

### AI 变更审查

AI Changes 面板用于集中审查 AI 产生的文件改动：

- 展示待处理的 AI 文件变更。
- 支持 Before / After 双栏 diff。
- 新增内容以绿色标识，删除内容以红色标识。
- GDScript diff 支持语法高亮。
- 支持保留变更。
- 支持撤销变更。
- 同一个文件被多次修改时，会合并为最终内容与原始内容之间的 diff。

这让 AI 文件写入不再是黑盒操作，开发者可以逐条理解、确认和回退。

### NEXT 模式：里程碑式多 Agent 协作

NEXT 模式是 NEXT Engine 的核心差异化能力，通过专业化多 Agent 团队将游戏创意逐步转化为可运行的工程产物：

**五大 Agent 协作团队**

| Agent | 职责 | 产出 |
|-------|------|------|
| 规划 Agent | 拆解需求、规划里程碑和任务、生成反馈任务 | 里程碑计划 |
| 脚本 Agent | GDScript 创建、编辑、函数级修补、节点绑定 | `.gd` 脚本 |
| 场景 Agent | 场景创建、节点组装、属性设置、挂载脚本和材质 | `.tscn` 场景 |
| 着色器 Agent | GDShader 创建编辑、ShaderMaterial 创建和应用 | `.gdshader` / 材质 |
| 审查 Agent | 审查里程碑产出、基于反馈生成修正建议 | 反馈任务 |

**NEXT 面板功能**

- 工作流管理：支持创建、加载、删除、继续执行多个工作流，中断后自动保存检查点。
- 里程碑列表：展示按阶段排列的里程碑，支持编辑、删除、移动、合并操作。
- 任务树视图：展示选中里程碑下的详细任务，显示执行者 Agent、当前状态和描述，支持拖拽排序和依赖编辑。
- 任务检查器：查看和编辑任务的标题、描述、指派 Agent、依赖关系和附件。
- 执行控制：一键运行当前里程碑，按依赖顺序自动调用各子 Agent，执行进度实时可见。
- 反馈面板：里程碑完成后可直接在引擎内游玩验收，通过自然语言提交修改意见，系统自动生成修正任务。
- 执行日志：实时记录所有 Agent 的调用、输出和状态变更，便于追踪过程。

**依赖传递与资产注册**

- 所有 Agent 产出自动进入资产注册表，场景 Agent 可从注册表自动加载已生成的脚本和材质。
- 用户导入的资产自动注册并受保护，不可被 Agent 修改，但允许生成派生资源。
- 任务通过资产 ID 显式引用依赖，确保执行顺序正确。

### 用户系统

NEXT Engine 内置用户登录与身份管理，通过远程 NexusServer API 提供服务：

- 支持手机验证码登录和密码登录两种方式。
- 登录状态持久化，支持自动 token 刷新和过期重登录。
- 账户信息展示：昵称、手机号、用户 ID、积分和礼品卡余额。
- 编辑器右上角显示用户头像入口，点击可查看账户详情和退出登录。
- 认证客户端独立运行在后台线程，不阻塞编辑器 UI。
- 设备 ID 生成并持久化，跨登出保持不变。

### MarkdownViewer 节点

NEXT Engine 新增了独立的 `MarkdownViewer` 场景节点（`Control` 子类），可在任何 UI 场景中使用：

- 完整的 Markdown 解析：标题、段落、粗体、斜体、删除线、代码块、引用、列表、表格、链接、图片、分割线。
- 代码块语法高亮：支持 GDScript、Shader 等语言的关键字、字符串、注释和数字识别。
- 图片加载：支持本地图片和远程 HTTP 图片（可配置开关）。
- 可配置属性：最大图片宽度/高度、滚动、语法高亮开关、代码复制按钮、远程图片开关、打开链接开关。
- 高效的视口裁剪渲染，适用于长文档展示。

AI Dock 和 AI Settings 中的消息渲染和帮助文档展示均基于 MarkdownViewer 实现。

### 权限与安全边界

NEXT Engine 的 AI 工具调用默认遵循清晰的权限边界：

- 只读项目工具可直接用于上下文理解。
- 高风险编辑工具按 Agent Profile 控制权限（允许 / 询问 / 拒绝）。
- MCP 等外部工具默认需要用户审批。
- 工具执行结果会写回消息链路，便于追踪。
- 文件改动通过变更审查面板呈现。
- 普通模式下由 Agent Profile 划分 `plan`、`write`、`review`、`build` 四种权限级别。
- NEXT 模式下各子 Agent 拥有与其职责匹配的工具集，遵循最小权限原则。

目标是让 AI 能真正参与开发，同时让开发者始终掌握最终控制权。

## 构建

Windows 下可使用 SCons 构建编辑器：

```powershell
scons platform=windows
```

如果构建提示 Direct3D 12、WinRT 或 AccessKit 依赖缺失，请按终端提示执行 Godot 提供的依赖安装脚本。

常用 AI 相关测试命令：

```powershell
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[AI]*"
```

## 项目结构

```
next-engine/
  editor/ai_component/       # AI 功能核心代码
    agent/                   # Agent 基类、会话、Runtime、Context Manager、MCP
    next/                    # NEXT 模式核心（项目状态、工作流、事件日志等）
    next/agents/             # NEXT 五大子 Agent（规划/脚本/场景/着色器/审查）
    ui/                      # 普通模式 UI（Dock、Composer、消息列表、变更审查等）
    ui/next/                 # NEXT 模式 UI（NEXT 面板、里程碑列表、任务树等）
    providers/               # 模型供应商、MCP 客户端、协议编解码
    tools/                   # 工具系统基类和工具注册
    tools/editor/            # 场景、脚本、Shader 编辑工具
    tools/project/           # 项目上下文工具（文件树、搜索、读取等）
    context/                 # 上下文提供者（编辑器、项目树、文件、最佳实践等）
    planning/                # 普通模式计划管理
    review/                  # 变更集存储和 Diff 服务
    skills/                  # AgentSkill 系统
    rules/                   # Rules 规则系统
    storage/                 # 会话持久化存储
  editor/user_system/        # 用户系统（登录、认证、账户管理）
  scene/gui/                 # MarkdownViewer 节点（Markdown 解析/布局/渲染）
  docs/                      # 架构文档和 API 规范
  tests/editor/              # AI 相关单元测试
```

## 开发原则

NEXT Engine 的 AI 模块遵循以下原则：

- AI 功能与 Godot 原生功能边界清晰。
- UI、Session、Runtime、Provider、Tool、Storage 各层职责分离。
- 工具优先封装编辑器内部能力，而不是绕过引擎直接修改资源。
- 高风险写入必须可追踪、可审查、可撤销。
- MCP、Skill、Rules 等扩展能力通过独立模块接入，避免侵入主流程。
- Planning、Rules、Skill、MCP 均通过独立服务或 Context Provider 接入，保持低耦合、高内聚。
- 普通模式与 NEXT 模式共享 Agent 基类、Runtime、工具集和存储层，避免重复造轮子。
- 优先保证稳定性、可维护性和可扩展性。

## 后续方向

NEXT Engine 会继续围绕游戏开发场景扩展 AI Agent 工作流：

- 更完整的模型能力描述和模型配置体验。
- 更细粒度的 Ask / Write / Review 模式。
- 更强的场景、节点、脚本、Shader 和资源编辑工具。
- 更准确的上下文预算、压缩和摘要机制。
- 更成熟的变更审查、diff、回滚和任务记录体验。
- 更强的 Skill 类型、Rules 系统和计划管理体验。
- 更丰富的 MCP 工具生态接入。
- NEXT 模式更多子 Agent 类型和更智能的依赖编排。
- 更适合游戏开发者的 AI 交互体验。
