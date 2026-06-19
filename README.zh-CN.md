# NEXT Engine

[English](README.md) | 简体中文

NEXT Engine 是基于 Godot Engine 的二次开发编辑器，把 AI 辅助开发直接放进编辑器里。你仍然在熟悉的场景树、脚本、资源和调试环境中工作，只是多了一个能理解项目上下文、修改内容、管理任务并审查变更的 AI 搭档。

<p align="center">
  <img src="logo.png" width="400" alt="NEXT Engine logo">
</p>

## Overview

NEXT Engine 面向 Godot 风格的游戏和交互项目开发，重点是提供编辑器原生的 AI 工作流。

- 上游项目和基础引擎：Godot Engine 4.x。
- 当前 fork 版本元数据：`NEXT Engine 4.7.1 rc`，`version.py` 中的文档分支为 `4.7`。
- 关键依赖：Godot Engine、`thirdparty/cmark/` 中的 `cmark 0.31.1`、以及 `thirdparty/fonts/AlimamaFangYuanTiVF.woff2` 中的阿里妈妈方圆体。
- 许可证：沿用 Godot Engine 的 MIT 许可证模式。第三方组件保留各自许可证。

本项目的主要二次开发集中在 AI Agent 运行时、编辑器 AI UI、用户系统、Markdown 解析/渲染链路、项目文档和相关测试。

## Features

- AI Dock：聊天、模型选择、附件、进度、token 使用、todo、权限审批和 diff 审查。
- Agent V1 后端：durable session、事件日志、投影、Runner 协调、provider-neutral 模型请求、工具结算、中断、恢复和压缩。
- 内置项目/编辑器工具：读取和搜索文件、编辑场景、编写脚本、编辑 Shader、管理 todo、收集结构化需求。
- 统一权限流：本地工具、MCP 工具、Skill 工作流和子代理都进入同一套权限体系。
- MCP 集成：支持配置 server、工具发现、资源读取、prompt 渲染、启动权限和命名空间工具 materialization。
- Skill 系统：支持发现、选择、上下文注入和按需读取专业工作流说明。
- 用户系统：`editor/user_system/` 中实现鉴权、服务端用户数据、本地 session 处理和 UI 集成。
- Runtime Markdown 链路：`core/markdown` 把 cmark 封装为 `MarkdownParser` 和 `MarkdownNode`；`scene/gui/MarkdownViewer` 用自绘 `Control` 渲染 Markdown。
- `editor/agent_v1/` 中保留公开 Agent API 说明，本地开发笔记放在 `.devdocs/` 等已忽略路径下，不随代码提交。

## Project Status

NEXT Engine 仍在快速迭代中。当前重点包括：

- 让 AI Dock 交互更自然、更可预期。
- 加强 Agent V1 的会话恢复、权限处理、工具结算和上下文管理。
- 通过可审查工具提升场景、脚本、Shader 和项目文件编辑能力。
- 持续从 editor-only Markdown 组件迁移到 runtime `MarkdownViewer`。
- 保持用户系统鉴权、服务端配置和本地运行状态的边界清晰。

公开架构参考：

- `editor/agent_v1/core/API.md`
- `editor/agent_v1/domain/API.md`
- `editor/agent_v1/best_practices.md`

## Build from Source

日常构建：

```powershell
scons platform=windows
```

开发和测试构建：

```powershell
scons platform=windows target=editor dev_build=yes tests=yes
```

Agent V1 相关测试：

```powershell
bin\next.windows.editor.dev.x86_64.console.exe --test --test-case="*[AgentV1]*"
```

MarkdownViewer 相关测试：

```powershell
bin\next.windows.editor.dev.x86_64.console.exe --test --test-case="*[MarkdownViewer]*"
```

用户系统相关测试：

```powershell
bin\next.windows.editor.dev.x86_64.console.exe --test --test-case="*[UserSystem]*"
```

## Project Layout

```text
editor/
  agent_v1/       Agent V1 后端、领域模型、运行时、工具、权限、MCP、Skill、agents、UI adapter
  agent_ui/       AI Dock、Composer、消息列表、设置页、审查面板
  user_system/    用户鉴权、profile/session 处理、服务端集成

core/
  markdown/       基于 cmark 的 MarkdownParser / MarkdownNode core 解析层

scene/gui/
  markdown_viewer*  runtime MarkdownViewer 节点及 document/layout/draw/image/highlighting helpers

thirdparty/
  cmark/          cmark 0.31.1 CommonMark parser 源码
  fonts/          内置字体，包括 AlimamaFangYuanTiVF.woff2

tests/
  editor/         Agent V1、AI UI、用户系统测试
  scene/          MarkdownViewer runtime scene 测试
```

## Contributing

- 修改本仓库前先阅读 `AGENTS.md`。
- 实现任何功能前，先查看相关代码中是否已有可复用的基础设施、领域模型、服务类和测试。
- AI 后端优先复用 `core`、`domain`、`session`、`tools`、`permission` 中已有的 Agent V1 契约，避免另起一套并行系统。
- Markdown 相关工作优先复用 `core/markdown` 和 `MarkdownViewer`，不要再新增一套 parser 或 RichTextLabel 渲染规则。
- 编辑器 UI 应作为后端状态的投影；需要恢复的状态应进入 Agent/session/event/config 层。
- 修改运行时行为、用户流程或可复用基础设施时，添加或更新聚焦测试。
- 本地设计笔记可放在 `.devdocs/` 等已忽略路径下；需要随代码公开的契约应放在实现目录旁边。

## License

NEXT Engine 沿用 Godot Engine 的 MIT 许可证。详见 `LICENSE.txt`。

NEXT Engine 名称、logo、icon、应用图标、启动图和其他品牌资产不包含在 MIT 许可证的自由使用授权中。详见 `TRADEMARKS.zh-CN.md`。

本仓库同时包含 cmark、字体等第三方组件。它们的版权和许可证条款仍归各自上游项目或仓库中对应的许可证文件所有。
