# NEXT Engine

NEXT Engine 是基于 Godot Engine 深度定制的游戏开发引擎，目标是在传统 2D / 3D 游戏开发流程中集成 AI Agent 能力，让 AI 可以理解项目、参与编辑器工作流，并逐步辅助完成脚本编写、资源管理、场景分析、项目维护与开发决策。

本项目面向商业化产品开发，当前重点是构建稳定、可维护、可扩展的 AI 游戏开发助手，而不是简单叠加聊天窗口。

<p align="center">
  <img src="logo.png" width="400" alt="NEXT Engine logo">
</p>

## 核心功能

### Godot 编辑器能力

NEXT Engine 保留 Godot Engine 原有的核心编辑器能力，包括：

- 2D / 3D 场景编辑。
- 节点、资源、脚本、项目设置等常规工作流。
- Windows 编辑器构建与运行。
- Godot 原有项目管理器、编辑器主界面和导出体系。

AI 功能以编辑器扩展能力的形式接入，尽量减少对 Godot 原始功能的干扰，保持引擎主体稳定。

### AI Settings

编辑器中提供 AI Settings 页面，用于管理 AI 功能的基础配置。

当前已支持：

- 配置 AI Provider。
- 配置 API 地址。
- 配置 API Key。
- 选择和切换模型。
- 使用预设模型配置。
- 保存基础模型设置。

侧边栏已预留常见 Agent Coding 工具配置入口：

- 模型。
- MCP。
- Skill。
- Rules。

其中 MCP、Skill、Rules 当前为后续扩展入口，暂未启用完整功能。

### AI 对话

当前版本已实现基础 AI 对话能力：

- 在编辑器中打开 AI 助手面板。
- 向当前配置的模型发送消息。
- 接收并展示模型回复。
- 支持模型切换后的对话请求。
- 支持基础错误提示。
- 支持历史 session 管理。

对话能力是后续 Agent 工作流的基础，后续会继续扩展为可执行任务、读取上下文、调用工具、生成修改建议和应用代码变更的完整流程。

### Agent Runtime

项目已开始引入更清晰的 Agent Runtime 架构，用于替代早期混乱的 AI 接入逻辑。

当前基础能力包括：

- Agent 会话管理。
- Runtime Loop。
- Provider 请求适配。
- OpenAI 兼容接口调用。
- 工具调用协议解析。
- 工具执行结果回填到会话。
- AI 消息、工具消息、错误消息的基础渲染。

当前阶段优先保证结构稳定和职责清晰，为后续扩展多模型、多工具、多任务执行打基础。

### 项目上下文工具

当前已实现基础只读项目工具，用于让 AI 初步获取项目上下文。

现阶段重点是安全读取和稳定执行，不直接开放危险写入能力。后续会逐步加入更完整的权限控制、任务审批、代码修改和结果验证流程。

## 当前 MVP 状态

当前版本的 MVP 目标是让编辑器内 AI 助手具备最小可用闭环：

- 可以配置模型。
- 可以切换模型。
- 可以创建和管理历史会话。
- 可以进行基础对话。
- 可以处理基础工具调用结果。
- 可以读取部分项目上下文。
- 不影响 Godot 原有编辑器功能。

暂未完成或暂未启用：

- MCP Server。
- Skill 系统。
- Rules 规则系统。
- 完整 ReAct / Function Calling 策略切换。
- 自动代码修改。
- 多步骤任务执行。
- 项目级权限审批。
- 复杂任务计划与回滚机制。

## 后续方向

NEXT Engine 后续会围绕 Agent Coding 工作流继续完善：

- 更完整的模型配置和模型能力描述。
- 更稳定的 Function Calling 工具调用流程。
- 项目文件读取、搜索、分析工具。
- 场景、节点、脚本、资源的编辑器上下文感知。
- 任务级 AI Session。
- 可审批的代码修改流程。
- Skill 与 Rules 系统。
- MCP 能力重新设计后再接入。
- 更适合游戏开发者的 AI 交互体验。

## 构建说明

Windows 下可以使用 SCons 构建编辑器：

```powershell
scons platform=windows d3d12=no winrt=no accesskit=no
```

如果需要启用 Direct3D 12、WinRT 或 AccessKit，需要先安装对应依赖。

常用测试命令：

```powershell
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[AI]*"
```

## 项目定位

NEXT Engine 不是单纯的 Godot 换皮版本，而是面向 AI 辅助游戏开发的引擎级产品实验。

当前阶段的重点是：

- 保持 Godot 原有功能稳定。
- 将 AI 功能拆分为高内聚模块。
- 建立可维护的 Agent 架构。
- 先完成可靠 MVP，再扩展复杂工具能力。

