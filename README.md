# NEXT Engine

NEXT Engine 是基于 Godot Engine 二次开发的游戏开发引擎，目标是在保留 Godot 原有 2D/3D 编辑器能力的基础上，将 AI Agent 深度接入游戏开发工作流。

项目定位是商业化引擎产品，而不是简单的聊天窗口插件。AI 能力会逐步参与项目理解、场景编辑、脚本编写、Shader 编写、资源管理、变更审查和开发决策，让游戏开发者可以在编辑器内完成更连续的 AI 辅助开发流程。

<p align="center">
  <img src="logo.png" width="400" alt="NEXT Engine logo">
</p>

## 产品定位

NEXT Engine 的核心目标：

- 保持 Godot 原有编辑器、项目管理器、场景系统、脚本系统和资源系统稳定可用。
- 将 AI 功能设计为高内聚、低耦合的编辑器能力，尽量减少对 Godot 原始功能的侵入。
- 构建可维护、可扩展的 Agent 架构，而不是把模型请求、工具调用和 UI 逻辑混在一起。
- 优先完成可用、稳定、可持续迭代的 AI 游戏开发助手 MVP。

## 当前核心能力

### Godot 编辑器基础能力

NEXT Engine 继承 Godot Engine 的基础开发能力，包括：

- 2D / 3D 场景编辑。
- 节点树、Inspector、资源文件系统、脚本编辑器等常规编辑器工作流。
- 项目管理器、编辑器主界面和运行调试流程。
- Windows 平台编辑器构建与运行。

AI 功能作为编辑器侧增强能力接入，不替代 Godot 原有工作流。

### AI 设置

编辑器内提供 AI Settings 页面，用于管理模型配置。

当前已支持：

- 自定义模型供应商。
- 配置模型名称、Provider、Base URL、API Key、模型 ID 等信息。
- 同一供应商和同一模型可以创建多个不同配置，用户可以自定义显示名称。
- 模型配置列表支持新增、编辑、删除。
- 设置页预留 Model、MCP、Skill、Rules 等 Agent Coding 工具常见配置入口。

当前阶段仅启用模型配置。MCP、Skill、Rules 暂作为后续扩展入口保留。

### AI 对话与会话

右侧 AI Dock 已实现基础对话能力：

- 选择模型并发送消息。
- 支持 OpenAI-compatible Provider 请求。
- 支持流式文本返回。
- 支持请求中的加载状态展示。
- 支持 Markdown 消息渲染。
- 支持历史会话管理。
- 支持按项目隔离会话列表。
- 打开项目后默认恢复最近一次会话。
- 支持展示当前会话的 token 消耗统计。

### Agent Runtime

当前 Agent 使用 OpenAI-compatible function calling 架构：

- Runtime 负责多轮模型请求和工具调用循环。
- Provider 层负责模型配置、HTTP 请求、流式响应解析和协议转换。
- Tool Registry 统一管理工具 schema、权限和执行入口。
- Context Manager 负责整理系统提示、历史消息和项目上下文，并控制上下文预算。
- Session 层负责 UI 状态、消息持久化、会话切换和运行结果回写。

该结构已经移除旧的 MCPServer 依赖。MCP 后续如重新接入，应作为 Tool Provider 或工具扩展层接入，而不是侵入 Session/UI 主流程。

### 项目上下文与只读工具

AI Agent 可以通过工具获取项目上下文：

- 查看项目文件树。
- 读取项目文件。
- 搜索项目文本。
- 获取当前编辑器上下文。

这些工具用于让模型理解当前项目结构、编辑状态和相关代码内容。

### 场景编辑工具

当前已实现一组编辑器内场景操作工具，供 Agent 在 Write 模式下调用：

- 创建场景。
- 打开场景。
- 保存当前场景。
- 创建节点。
- 删除节点。
- 重命名节点。
- 移动节点。
- 设置节点属性。
- 创建文件夹。

场景编辑工具优先封装 Godot 编辑器内部接口和 Undo/Redo 能力，避免直接把场景文件当普通文本随意修改。

### 脚本与 Shader 工具

当前已实现基础 GDScript 和 GDShader 编辑能力：

- 创建脚本。
- 删除脚本。
- 编写或覆盖脚本。
- 基于函数级定位修改 GDScript 函数。
- 将脚本绑定到节点。
- 从节点解绑脚本。
- 创建并应用 ShaderMaterial。
- 创建或修改 GDShader。

高风险操作会进入权限控制和审查流程，后续会继续增强更细粒度的确认与回滚能力。

### 变更审查

AI 对脚本和 Shader 的写入会记录变更集，编辑器内提供 AI Changes 面板：

- 展示待处理的 AI 文件变更。
- 支持查看 Before / After 双栏 diff。
- 新增内容以绿色标识，删除内容以红色标识。
- GDScript diff 支持语法高亮。
- 支持保留变更。
- 支持撤销变更。
- 同一个文件被 Agent 多次修改时，会合并为最终内容与原始内容之间的 diff。

该能力用于让用户在 AI 可写入文件的情况下仍能理解、确认和回退改动。

## 当前 MVP 状态

当前 MVP 的目标是建立一个可运行的 AI 游戏开发助手闭环：

- 可以配置和切换模型。
- 可以进行基础对话。
- 可以管理历史会话。
- 可以读取项目上下文。
- 可以通过 function calling 调用工具。
- 可以执行部分场景、脚本和 Shader 编辑任务。
- 可以记录和审查 AI 文件变更。
- 不影响 Godot 原有编辑器功能。

暂未完成或暂未启用：

- MCP。
- Skill 系统。
- Rules 系统。
- 完整任务规划器。
- 更精细的权限审批 UI。
- 更完整的资源生成和资源引用工具。
- 更复杂的上下文压缩与长期记忆机制。

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

NEXT Engine 的 AI 模块开发遵循以下原则：

- AI 功能与 Godot 原生功能边界清晰。
- UI、Session、Runtime、Provider、Tool、Storage 各层职责分离。
- 工具优先封装编辑器内部能力，而不是直接绕过引擎修改资源。
- 高风险写入必须可追踪、可审查、可撤销。
- 当前阶段优先稳定性和可维护性，再逐步扩展复杂能力。

## 后续方向

后续会继续围绕 Agent Coding 工作流增强：

- 更完整的模型能力描述和模型配置体验。
- 更细粒度的 Ask / Write / Review 模式。
- 更强的场景、节点、脚本、Shader 和资源编辑工具。
- 更准确的上下文预算、压缩和摘要机制。
- 更成熟的变更审查、diff、回滚和任务记录体验。
- Skill 与 Rules 系统。
- MCP 重新设计后的可选接入。
- 更适合游戏开发者的 AI 交互体验。
