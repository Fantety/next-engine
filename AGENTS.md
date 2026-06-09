该项目（NextEngine）是 Godot Engine 的二次开发项目。

主要改动集中在：
- editor/ai_component AI 相关功能
- editor/user_system 新增用户系统
- scene/gui 新增 MarkdownViewer 节点
- .devdocs/ 相关文档存放位置

AI 功能当前只保留单 MainAgent 工作流：
- 所有工具挂载在 MainAgent 下。
- 右侧 AI Dock 负责会话、模型选择、工具审批、计划显示和变更审查入口。
- 不再维护旧的多 Agent 规划工作流，也不要恢复对应的独立规划目录、面板或类型实现。
