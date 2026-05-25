# NEXT Engine

NEXT Engine 是基于 Godot Engine 深度定制的 AI 游戏开发编辑器。它保留 Godot 成熟的 2D/3D 编辑、节点系统、资源管理、脚本编辑和运行调试能力，并把 AI Agent 融入编辑器工作流，让项目理解、对话协作、场景编辑、脚本修改、Shader 编写和变更审查都可以在同一个开发环境中完成。

<p align="center">
  <img src="logo.png" width="400" alt="NEXT Engine logo">
</p>

## 项目愿景

NEXT Engine 希望把 AI 从一个外部聊天窗口变成编辑器内部的开发伙伴。它可以读取项目结构、理解当前编辑器状态、调用受控工具完成具体操作，并把每一次高风险写入都纳入可确认、可审查、可撤销的流程中。

核心方向：

- 保留 Godot 原有编辑器体验和项目兼容性。
- 让 AI Agent 直接理解游戏项目、场景、脚本和资源上下文。
- 用工具调用连接模型能力与真实编辑器能力。
- 对文件写入、场景修改和外部工具调用提供权限控制。
- 通过变更审查面板让 AI 修改可追踪、可比较、可回退。
- 以解耦架构持续扩展模型、工具、MCP、上下文和 Agent 工作流。

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

AI Dock 不只是问答窗口，它是 Agent Runtime、项目上下文、工具调用和变更审查的入口。

### 模型配置

AI Settings 提供模型配置管理：

- 添加自定义模型供应商。
- 配置 Provider、Base URL、API Key、模型 ID 和显示名称。
- 同一供应商和同一模型可以保存多个配置。
- 支持新增、编辑、删除模型配置。
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
- 会话上下文只注入启用 Skill 的名称和描述列表，不默认注入全文。
- Agent 可通过只读工具 `agent.activate_skill` 按 `skill_id` 激活某个 Skill，并读取完整内容。
- 当前 Skill 只提供 prompt/context 指令，不执行脚本、不启动进程、不读取任意资源，也不会自动授予工具权限。

后续可以在保持安全边界的前提下扩展 tool bundle、资源引用或更强 Skill 类型，但这些能力不会复用当前 prompt/context 激活路径作为执行入口。

### Agent Runtime

Agent Runtime 使用 OpenAI-compatible function calling 架构：

- Runtime 负责多轮模型请求和工具调用循环。
- Provider 层负责模型配置、HTTP 请求、流式响应解析和协议转换。
- Tool Registry 统一管理工具 schema、权限和执行入口。
- Context Manager 负责整理系统提示、历史消息和项目上下文，并控制上下文预算。
- Session 层负责 UI 状态、消息持久化、会话切换和运行结果回写。

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

### 权限与安全边界

NEXT Engine 的 AI 工具调用默认遵循清晰的权限边界：

- 只读项目工具可直接用于上下文理解。
- 高风险编辑工具按 Agent Profile 控制权限。
- MCP 等外部工具默认需要用户审批。
- 工具执行结果会写回消息链路，便于追踪。
- 文件改动通过变更审查面板呈现。

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

## 开发原则

NEXT Engine 的 AI 模块遵循以下原则：

- AI 功能与 Godot 原生功能边界清晰。
- UI、Session、Runtime、Provider、Tool、Storage 各层职责分离。
- 工具优先封装编辑器内部能力，而不是绕过引擎直接修改资源。
- 高风险写入必须可追踪、可审查、可撤销。
- MCP、Skill、Rules 等扩展能力通过独立模块接入，避免侵入主流程。
- 优先保证稳定性、可维护性和可扩展性。

## 后续方向

NEXT Engine 会继续围绕游戏开发场景扩展 AI Agent 工作流：

- 更完整的模型能力描述和模型配置体验。
- 更细粒度的 Ask / Write / Review 模式。
- 更强的场景、节点、脚本、Shader 和资源编辑工具。
- 更准确的上下文预算、压缩和摘要机制。
- 更成熟的变更审查、diff、回滚和任务记录体验。
- 更强的 Skill 类型与 Rules 系统。
- 更丰富的 MCP 工具生态接入。
- 更适合游戏开发者的 AI 交互体验。
