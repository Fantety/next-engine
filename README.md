# NEXT Engine

NEXT Engine 是基于 Godot Engine 的二次开发编辑器，把 AI 工作流直接放进编辑器里。你仍然在熟悉的场景树、脚本、资源和调试环境中工作，只是多了一个能理解项目上下文、帮你改内容、整理任务并审查变更的 AI 搭档。

<p align="center">
  <img src="logo.png" width="400" alt="NEXT Engine logo">
</p>

## 适合谁

- 正在使用 Godot / NEXT Engine 做游戏或交互项目的开发者
- 想让 AI 理解项目结构、脚本、场景和资源的人
- 需要在编辑器里审查 AI 改动、保留人工确认的人
- 想接入 MCP、Skill、权限规则和自定义模型的高级用户

## 快速开始

1. 打开项目。
2. 在 AI Settings 里配置模型和提供方。
3. 打开 AI Dock，描述你要做的事。
4. 附上相关文件，让 AI 先理解上下文，再开始修改。
5. 在变更审查里查看 diff，决定保留还是回退。

## 你可以做什么

- 让 AI 读取项目文件、搜索文本、查看编辑器上下文
- 让 AI 生成、修改和审查场景、脚本、Shader 和项目文件
- 通过 AI Dock 管理会话、模型、附件、任务和变更审查
- 用 Markdown 方式查看 AI 回复和项目说明
- 在复杂任务里把工作拆成 todo，再逐步推进

## 安全与权限

- 需要确认的操作会在编辑器里弹窗审批
- 删除项目文件前会检查它是否正在打开，或是否被其它场景引用
- 场景修改、脚本绑定和文件删除都尽量保持可审查、可回退

## 主要能力

- AI Dock：聊天、模型选择、附件、进度、token 使用、任务列表、diff 审查
- MarkdownViewer：用于展示 AI 消息和其他 Markdown 内容
- 用户系统：账号、权限和个性化配置
- 模型配置：支持 OpenAI-compatible 提供方、Base URL、API Key、模型 ID 等
- 扩展能力：支持 MCP、Skill 和权限规则

## 项目结构

- `editor/agent_v1/`：AI Agent、工具、权限、MCP、Skill、会话
- `editor/agent_ui/`：AI Dock、Composer、消息列表和审查面板
- `editor/user_system/`：用户系统
- `scene/gui/`：`MarkdownViewer` 等 GUI 节点
- `.devdocs/`：架构说明和设计记录
- `tests/editor/`：编辑器和 AI 相关测试

## 从源码构建

日常构建：

```powershell
scons platform=windows
```

开发和测试：

```powershell
scons platform=windows target=editor dev_build=yes tests=yes
```

AI 相关测试：

```powershell
bin\next.windows.editor.dev.x86_64.console.exe --test --test-case="*[AgentV1]*"
```

## 当前状态

NEXT Engine 仍在快速迭代中，重点方向包括更自然的 AI Dock 交互、更稳定的场景和脚本编辑、更清晰的变更审查，以及更完整的模型和权限体系。
