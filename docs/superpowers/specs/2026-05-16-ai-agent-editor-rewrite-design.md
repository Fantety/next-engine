# Godot Editor AI Agent 重构设计

## 目标

本阶段完整重写当前 `editor/ai_component` AI 功能，建立一个高内聚、低侵入、可扩展的 Editor Agent 子系统。AI 只作为 Godot Editor 功能存在，不进入项目运行时，不影响 Godot 原生场景系统、导出流程和脚本 API。

第一阶段能力范围固定为：

- 支持 AI 聊天和流式响应。
- 支持 OpenAI-compatible Provider，优先接入现有 DeepSeek 配置。
- 支持只读项目上下文读取，包括项目目录结构、指定文本文件内容、当前编辑器基础上下文。
- 支持会话历史保存、加载、重试、取消、错误展示。
- 移除 MCPServer 和旧 MCP 协议支持。

第一阶段明确不做：

- 不支持 MCP。
- 不启动本地 HTTP Server。
- 不允许 AI 写文件、创建/删除节点、修改场景、执行编辑器操作。
- 不把 Agent 能力注册为 Godot 运行时类给游戏项目使用。

## 当前问题

当前 AI 功能主要集中在 `AIDock` 中，职责过多：

- `AIDock` 同时负责 UI、会话历史、Provider 选择、HTTP 请求、流式解析、工具调用、MCP Server 开关。
- `EngineOperator` 直接解析模型输出的 `<tool>` 内容并执行工具调用，缺少明确的权限边界。
- MCP 相关代码跨越 `editor`、`scene/main`、`core/io` 三层，污染了 Godot 原生运行时层。
- 旧响应协议依赖 XML-like 标签：`<thought>`、`<tool>`、`<final_answer>`，不适合作为稳定 Agent 消息协议。
- Provider 配置 UI 声称支持多个模型供应商，但实际请求实现基本绑定 DeepSeek/OpenAI-compatible 路径。

## 推荐架构

新的 AI 子系统仍放在 `editor/ai_component` 下，但按职责拆分：

```text
editor/ai_component/
  agent/       Agent 会话、运行状态、消息模型、上下文组装
  providers/   LLM Provider 抽象和 OpenAI-compatible 实现
  context/     只读项目上下文 Provider
  storage/     会话持久化
  ui/          AI Dock、聊天列表、输入区、设置对话框
```

`editor/ai_component` 是唯一的 AI 业务边界。除必要的 `EditorNode` 挂载点和 `register_editor_types.cpp` 注册外，不修改 Godot 原生模块行为。

## 核心组件

### Agent 层

`AIAgentSession` 管理一次会话的消息、状态和配置。它不直接操作 UI，也不直接访问 HTTP。

`AIAgentRunner` 负责一次请求生命周期：准备上下文、调用 Provider、接收流式片段、处理取消、完成、错误。

`AIAgentMessage` 表示统一消息模型，字段包括 `role`、`content`、`metadata`、`created_at`、`status`。第一阶段 role 使用 `system`、`user`、`assistant`、`context`、`error`。

`AIAgentState` 表示运行状态：`idle`、`preparing_context`、`streaming`、`cancelled`、`failed`。

### Provider 层

`AIAgentProvider` 是抽象接口，负责把统一请求转换为具体模型 API 请求。

`AIOpenAICompatibleProvider` 第一阶段支持 DeepSeek/OpenAI-compatible Chat Completions SSE。它从 EditorSettings 读取 base URL、API key、model。

Provider 只输出事件，不直接写 UI 或历史：

- `response_started`
- `response_delta`
- `response_finished`
- `request_failed`

### Context 层

只读上下文通过 `AIContextProvider` 提供，不再通过模型输出 `<tool>` 后回调执行。

第一阶段提供：

- `AIProjectTreeContextProvider`：读取 `res://` 项目结构，限制深度和最大输出长度。
- `AIFileContextProvider`：读取用户明确选择或 Agent 请求范围内的文本文件，限制文件大小和扩展名。
- `AIEditorContextProvider`：读取当前打开场景路径、当前脚本路径等安全元数据。

上下文作为请求前置材料注入 Provider，不允许模型在响应中动态触发写操作。

### Storage 层

`AIConversationStore` 替代旧 `AIChatManager`。它负责会话列表、单个会话 JSON 保存、加载和迁移。

建议新格式放在：

```text
user://ai_agent/conversations/index.json
user://ai_agent/conversations/<session_id>.json
```

旧 `user://chat_datas.json` 可以第一阶段只读迁移，也可以暂不迁移。若做迁移，必须保留失败回退，不覆盖旧数据。

### UI 层

新的 UI 仍以 Dock 形式挂入 Editor，但只负责展示和交互：

- `AIAgentDock`：顶层 Dock，连接 UI 与 `AIAgentSession`。
- `AIConversationList`：会话列表。
- `AIMessageList`：消息展示。
- `AIMessageBubble`：单条消息展示。
- `AIComposer`：输入框、模型选择、发送、取消。
- `AIAgentSettingsDialog`：Provider 和上下文设置。

UI 不直接构造 HTTP 请求，不解析 Provider 原始流，不执行工具。

## 数据流

1. 用户在 `AIComposer` 输入问题并点击发送。
2. `AIAgentDock` 调用 `AIAgentSession::send_user_message()`。
3. `AIAgentSession` 追加用户消息并请求 `AIAgentRunner` 开始执行。
4. `AIAgentRunner` 调用只读 Context Provider 组装项目上下文。
5. `AIAgentRunner` 调用 `AIAgentProvider::start_chat()`。
6. Provider 流式返回 delta。
7. `AIAgentRunner` 更新 assistant 消息，并发信号给 UI。
8. `AIConversationStore` 在完成或取消后保存会话。

## 错误和权限边界

所有错误都进入 Agent 状态机，不让 UI 猜测底层失败原因。

必须处理的错误：

- 未配置 API key。
- Provider base URL 为空或非法。
- 网络连接失败。
- HTTP 非 2xx。
- SSE JSON 解析失败。
- 用户取消。
- 上下文过大被截断。
- 文件读取被拒绝或不是文本文件。

权限边界：

- Context Provider 只能读取 `res://` 和必要的 Editor 元数据。
- 文件读取必须限制大小，建议单文件默认不超过 256 KB。
- 项目树输出必须限制节点数量和深度。
- 第一阶段不实现写操作接口，避免后续误接。

## 需要改动的现有源码文件

### Editor 集成

| 文件 | 改动 | 功能描述 |
| --- | --- | --- |
| `editor/editor_node.h` | 保留 AI Dock 和 AI Settings 成员，但类型改为新 UI 类。 | Godot Editor 主节点声明 AI 面板和设置窗口。 |
| `editor/editor_node.cpp` | 替换旧 `AIDock`、`AISettingsDialog` 的创建和菜单入口。 | 把新 AI Dock 挂入 Editor Dock 系统，把新设置窗口挂到 `gui_base`。 |
| `editor/register_editor_types.cpp` | 移除旧 MCP include、`MCPHttpServer` 注册、`MCPToolRegister::register_all_tools()`；注册新的 Agent/UI 类。 | Editor 类型注册入口，只保留 Editor AI 相关类，不注册 MCP。 |

### AI 组件入口

| 文件 | 改动 | 功能描述 |
| --- | --- | --- |
| `editor/ai_component/SCsub` | 移除 `mcp/SCsub`；新增 `agent/SCsub`、`providers/SCsub`、`context/SCsub`、`storage/SCsub`、`ui/SCsub`。 | AI 组件构建入口。 |
| `editor/ai_component/apis/SCsub` | 旧生成工具可删除或停止使用，Provider 重写后不再依赖 `tools.json` 的模型工具定义。 | 旧 API 构建入口，后续由 `providers` 替代。 |

### 旧 AI UI 和逻辑

| 文件 | 处理方式 | 功能描述 |
| --- | --- | --- |
| `editor/ai_component/ai_dock.h` | 删除或替换为兼容 include，实际使用新 `ui/ai_agent_dock.h`。 | 旧 AI Dock，当前耦合 UI、请求、历史、工具、MCP。 |
| `editor/ai_component/ai_dock.cpp` | 删除旧实现。 | 旧 AI Dock 实现。 |
| `editor/ai_component/ai_settings_dialog.h` | 删除或替换为新 `ui/ai_agent_settings_dialog.h`。 | 旧 Provider 设置窗口。 |
| `editor/ai_component/ai_settings_dialog.cpp` | 删除旧实现。 | 旧 Provider 设置窗口实现。 |
| `editor/ai_component/ai_chat_panel.*` | 删除或拆分为新 `AIComposer`。 | 旧输入区和模型选择。 |
| `editor/ai_component/ai_chat_block.*` | 删除或拆分为新 `AIMessageBubble`。 | 旧消息块展示。 |
| `editor/ai_component/ai_history_button.h` | 删除或替换为新 `AIConversationListItem`。 | 旧历史按钮。 |
| `editor/ai_component/ai_accept_dialog.*` | 第一阶段不需要，删除或仅在后续写操作确认流中重建。 | 旧确认弹窗。 |
| `editor/ai_component/ai_chat_manager.*` | 替换为 `storage/ai_conversation_store.*`。 | 旧聊天 JSON 存储。 |
| `editor/ai_component/engine_operator.*` | 第一阶段移除，不再允许模型输出驱动工具执行。 | 旧工具执行队列。 |

### 旧 API 和流解析

| 文件 | 处理方式 | 功能描述 |
| --- | --- | --- |
| `editor/ai_component/apis/openai_request_handler.*` | 替换为 `providers/ai_openai_compatible_provider.*`。 | 旧 DeepSeek/OpenAI-compatible 请求实现。 |
| `editor/ai_component/apis/ai_streaming_base.*` | 替换为 `providers/ai_agent_provider.*`。 | 旧 Provider 基类。 |
| `editor/ai_component/apis/ai_stream_processor.*` | 删除，不再依赖 XML-like 响应标签。 | 旧 `<thought>/<tool>/<final_answer>` 解析。 |
| `editor/ai_component/apis/system_prompt.md` | 重写为第一阶段 Agent 系统提示。 | Agent 行为约束提示词。 |
| `editor/ai_component/apis/system_prompt.h` | 由构建生成或迁移到新 prompt 资源。 | 生成后的系统提示头。 |
| `editor/ai_component/apis/tools.json` | 删除或停止注入 Provider。 | 旧模型工具定义。 |
| `editor/ai_component/apis/tools_json.h` | 删除或停止生成。 | 旧工具定义生成头。 |

### 旧只读工具序列化

| 文件 | 处理方式 | 功能描述 |
| --- | --- | --- |
| `editor/ai_component/tools/dir_serializer.*` | 可迁移到 `context/ai_project_tree_context_provider.*` 内部，保留只读逻辑。 | 读取项目目录结构。 |
| `editor/ai_component/tools/file_serializer.*` | 可迁移到 `context/ai_file_context_provider.*` 内部，加入大小和类型限制。 | 读取文本文件内容。 |
| `editor/ai_component/tools/gdscript_serializer.*` | 第一阶段可暂不接入。 | 旧 GDScript 序列化工具。 |
| `editor/ai_component/tools/scene_serializer.*` | 第一阶段可暂不接入。 | 旧场景序列化工具。 |
| `editor/ai_component/tools/string_tag_wrapper.*` | 删除，不再使用 observation 标签包装。 | 旧工具结果标签包装。 |
| `editor/ai_component/tools/SCsub` | 若迁移完成，可删除旧 tools 构建入口或只保留仍被 context 使用的文件。 | 旧工具构建入口。 |

### MCP 和 HTTP Server 移除

| 文件 | 处理方式 | 功能描述 |
| --- | --- | --- |
| `editor/ai_component/mcp/*` | 删除或从构建中完全移除。 | 旧 MCP 协议、MCP HTTP Server、MCP Tool 注册。 |
| `scene/main/mcp_router.*` | 删除。 | 旧 MCP HTTP 路由。 |
| `scene/main/http_server.*` | 删除或从构建中摘除。 | 为旧 MCP 引入的通用 HTTP Server。 |
| `scene/main/streamable_http_server.*` | 删除或从构建中摘除。 | 为旧 MCP SSE 引入的 HTTP Server。 |
| `scene/register_scene_types.cpp` | 移除 `http_server.h`、`streamable_http_server.h` include 和类注册。 | Scene 类型注册入口，恢复不暴露旧 HTTP Server。 |
| `core/io/http_router.*` | 删除或从构建中摘除。 | 旧 HTTP Router 基类。 |
| `core/io/http_response.*` | 删除或从构建中摘除。 | 旧 HTTP Response 封装。 |
| `core/io/http_file_router.*` | 删除或从构建中摘除。 | 旧静态文件路由。 |
| `core/io/http_request.*` | 注意与 Godot 原生 HTTPRequest 命名区分；若该文件是旧 MCP 新增 core/io 请求封装，则删除。 | 旧 HTTP Request 封装。 |
| `core/register_core_types.cpp` | 移除 `HttpRouter`、`HttpResponse`、`HttpFileRouter` 和旧 core/io `HttpRequest` 注册。 | Core 类型注册入口，避免 AI 扩展污染运行时。 |

## 需要新增的源码文件

### Agent

| 文件 | 核心功能 |
| --- | --- |
| `editor/ai_component/agent/SCsub` | 编译 Agent 层。 |
| `editor/ai_component/agent/ai_agent_message.h` | 定义 Agent 消息结构、角色、状态、序列化字段。 |
| `editor/ai_component/agent/ai_agent_session.h` | 会话接口，管理消息列表、当前 Provider、上下文策略。 |
| `editor/ai_component/agent/ai_agent_session.cpp` | 会话实现，发消息、取消、保存、加载、状态信号。 |
| `editor/ai_component/agent/ai_agent_runner.h` | 单次 Agent 执行器接口。 |
| `editor/ai_component/agent/ai_agent_runner.cpp` | 请求生命周期、上下文准备、Provider 事件转发、错误归一化。 |
| `editor/ai_component/agent/ai_agent_types.h` | 通用枚举和轻量数据结构。 |

### Providers

| 文件 | 核心功能 |
| --- | --- |
| `editor/ai_component/providers/SCsub` | 编译 Provider 层。 |
| `editor/ai_component/providers/ai_agent_provider.h` | Provider 抽象接口和信号定义。 |
| `editor/ai_component/providers/ai_provider_config.h` | Provider 配置结构，包含 name、base_url、api_key、model、timeout。 |
| `editor/ai_component/providers/ai_openai_compatible_provider.h` | OpenAI-compatible Provider 声明。 |
| `editor/ai_component/providers/ai_openai_compatible_provider.cpp` | Chat Completions SSE 请求、解析、取消、错误映射。 |
| `editor/ai_component/providers/ai_sse_parser.h` | SSE 解析器接口。 |
| `editor/ai_component/providers/ai_sse_parser.cpp` | 把 `data:` 流转换为 JSON delta 事件。 |

### Context

| 文件 | 核心功能 |
| --- | --- |
| `editor/ai_component/context/SCsub` | 编译 Context 层。 |
| `editor/ai_component/context/ai_context_provider.h` | 只读上下文 Provider 抽象接口。 |
| `editor/ai_component/context/ai_context_document.h` | 上下文文档结构，包含 title、source、content、truncated。 |
| `editor/ai_component/context/ai_project_tree_context_provider.h` | 项目树上下文声明。 |
| `editor/ai_component/context/ai_project_tree_context_provider.cpp` | 遍历 `res://`，输出受限目录结构。 |
| `editor/ai_component/context/ai_file_context_provider.h` | 文件内容上下文声明。 |
| `editor/ai_component/context/ai_file_context_provider.cpp` | 安全读取文本文件，限制大小、路径和扩展名。 |
| `editor/ai_component/context/ai_editor_context_provider.h` | 编辑器上下文声明。 |
| `editor/ai_component/context/ai_editor_context_provider.cpp` | 读取当前场景、脚本等只读 Editor 元数据。 |

### Storage

| 文件 | 核心功能 |
| --- | --- |
| `editor/ai_component/storage/SCsub` | 编译 Storage 层。 |
| `editor/ai_component/storage/ai_conversation_store.h` | 会话存储接口。 |
| `editor/ai_component/storage/ai_conversation_store.cpp` | JSON 保存、加载、索引维护、旧数据迁移入口。 |
| `editor/ai_component/storage/ai_conversation_serializer.h` | 消息和会话序列化声明。 |
| `editor/ai_component/storage/ai_conversation_serializer.cpp` | Dictionary/JSON 转换实现。 |

### UI

| 文件 | 核心功能 |
| --- | --- |
| `editor/ai_component/ui/SCsub` | 编译 UI 层。 |
| `editor/ai_component/ui/ai_agent_dock.h` | 新 AI Dock 声明。 |
| `editor/ai_component/ui/ai_agent_dock.cpp` | 新 AI Dock 实现，连接 Agent 信号和 UI 控件。 |
| `editor/ai_component/ui/ai_conversation_list.h` | 会话列表声明。 |
| `editor/ai_component/ui/ai_conversation_list.cpp` | 会话列表展示和选择。 |
| `editor/ai_component/ui/ai_message_list.h` | 消息列表声明。 |
| `editor/ai_component/ui/ai_message_list.cpp` | 消息列表增量更新和滚动。 |
| `editor/ai_component/ui/ai_message_bubble.h` | 单条消息 UI 声明。 |
| `editor/ai_component/ui/ai_message_bubble.cpp` | 用户、助手、错误、上下文消息展示。 |
| `editor/ai_component/ui/ai_composer.h` | 输入区声明。 |
| `editor/ai_component/ui/ai_composer.cpp` | 输入框、模型选择、发送、取消。 |
| `editor/ai_component/ui/ai_agent_settings_dialog.h` | 新 AI 设置窗口声明。 |
| `editor/ai_component/ui/ai_agent_settings_dialog.cpp` | Provider 设置、上下文开关、限制参数设置。 |

### Prompt

| 文件 | 核心功能 |
| --- | --- |
| `editor/ai_component/prompts/SCsub` | 编译或生成 Prompt 资源。 |
| `editor/ai_component/prompts/agent_system_prompt.md` | 第一阶段系统提示，声明只读、不可修改、不可声称已执行操作。 |
| `editor/ai_component/prompts/agent_system_prompt.h` | 生成后的系统提示头，供 Provider 请求体使用。 |

## 第一阶段实施顺序

1. 从注册和构建入口移除 MCP/HTTP Server。
2. 新增 Agent、Provider、Context、Storage、UI 目录和基础类型。
3. 实现 `AIConversationStore` 的保存/加载测试。
4. 实现 `AIProjectTreeContextProvider` 和 `AIFileContextProvider` 的只读限制测试。
5. 实现 `AIOpenAICompatibleProvider` 的请求构造、SSE 解析和取消。
6. 实现 `AIAgentSession` 和 `AIAgentRunner` 状态机。
7. 实现新 UI 并接入 `EditorNode`。
8. 删除或停用旧 AI/MCP 文件。
9. 编译验证 `scons platform=windows`。

## 验证标准

第一阶段完成时必须满足：

- `scons platform=windows` 编译通过。
- 搜索 `MCPHttpServer`、`MCPToolRegister`、`McpRouter` 无有效编译引用。
- Editor 可以打开 AI Dock。
- 未配置 API key 时 UI 给出明确错误。
- 配置 DeepSeek 后可以发送聊天并流式显示。
- 用户可以取消正在进行的请求。
- 会话历史可以保存并重新加载。
- 项目上下文读取只读且受限制。
- AI 响应中即使要求写文件或改场景，也不会触发任何本地修改操作。

## 后续阶段预留

后续可以在 Agent 层新增受控工具系统，但必须满足：

- 工具注册只在 `editor/ai_component` 内部。
- 写操作必须有用户确认、差异预览、撤销方案。
- 不重新引入 MCP，除非先完成独立协议设计和权限模型。
- 不把 Agent 工具暴露为游戏运行时类。
