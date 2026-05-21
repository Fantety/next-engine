# AI Agent 架构与工作流程

本文档描述 NEXT Engine 当前内置 AI Agent 的代码结构、运行流程和扩展边界。目标是让后续开发者能从“用户在右侧 AI Dock 输入一句话”一直追踪到“模型请求工具、工具读取项目、模型生成最终回答、UI 和历史会话更新”的完整链路。

当前版本已经移除 MCPServer 依赖。Agent 的工具调用采用 OpenAI-compatible Chat Completions 的 function calling 方式：引擎侧把工具 schema 发给模型，模型返回 `tool_calls`，引擎执行本地工具，再把工具结果作为 `tool` 消息发回模型，直到模型给出最终文本。

## 1. 分层总览

AI 功能集中在 `editor/ai_component/` 下，按职责分为以下层：

| 层级              | 主要文件                                                                                                                                                   | 职责                                       |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------- |
| UI 层            | `ui/ai_agent_dock.*`, `ui/ai_composer.*`, `ui/ai_message_list.*`, `ui/ai_message_bubble.*`, `ui/ai_agent_settings_dialog.*`                            | 右侧 Dock、输入框、模型选择、设置页、消息渲染、loading 状态     |
| Session 编排层     | `agent/ai_agent_session.*`                                                                                                                             | 管理一个聊天会话的消息、状态、模型配置、上下文收集、请求启动、结果回写、历史保存 |
| Agent Runtime 层 | `agent/ai_agent_runtime.*`, `agent/ai_agent_runtime_runner.*`, `agent/ai_agent_profile.*`, `agent/ai_context_manager.*`                                | function calling 主循环、上下文预算、工具权限、最大轮数限制、后台线程执行 |
| Provider 适配层    | `providers/ai_openai_runtime_client.*`, `providers/ai_openai_compatible_codec.*`, `providers/ai_provider_config.h`, `providers/ai_model_settings.*` | 模型配置、HTTP 请求、OpenAI-compatible 消息格式、响应解析 |
| Tool 层          | `tools/ai_tool.*`, `tools/ai_tool_registry.*`, `tools/ai_tool_permission.*`, `tools/project/*`, `tools/editor/*`                                       | 本地工具协议、工具注册、权限判断、读取/搜索项目内容               |
| Context 层       | `context/ai_editor_context_provider.*`, `context/ai_project_tree_context_provider.*`                                                                   | 每次请求前收集只读编辑器和项目树上下文                      |
| Storage 层       | `storage/ai_conversation_store.*`, `storage/ai_conversation_serializer.*`                                                                              | 历史会话保存、读取、列表排序                           |

## 2. 用户发送消息后的主流程

### 2.1 UI 入口：AIComposer 到 AIAgentDock

文件：

- `editor/ai_component/ui/ai_composer.cpp`
- `editor/ai_component/ui/ai_agent_dock.cpp`

用户在输入框点击 `Send` 后：

1. `AIComposer::_send_pressed()` 发出 `send_requested(message, model_id)` 信号。
2. `AIAgentDock` 在构造函数中连接该信号到 `_send_requested()`。
3. `AIAgentDock::_send_requested()` 做三件事：
   - 调用 `_get_provider_config(model_id)`，从 `AIModelSettings` 读取模型的 `base_url/api_key/model`。
   - 调用 `session->configure_provider(config)`，把配置同步给 provider 和 runtime client。
   - 调用 `session->send_user_message(message)` 启动 Agent。
4. 输入框被 `composer->clear_input()` 清空。

`AIComposer::set_running()` 会在请求过程中禁用发送按钮、启用取消按钮。这个状态来自 `AIAgentSession` 的 `state_changed` 信号。

### 2.2 Session 创建用户消息和 loading 占位

文件：

- `editor/ai_component/agent/ai_agent_session.cpp`
- `editor/ai_component/ui/ai_message_bubble.cpp`

`AIAgentSession::send_user_message()` 是请求编排入口：

1. 去除输入前后空白。
2. 如果消息为空，或者当前已经在请求中，直接返回。
3. 创建 `user` 消息，写入 `messages`，发出 `message_added`。
4. 立即保存会话：`_save()`。
5. 创建一个空内容的 `assistant` 消息，作为 pending/loading 占位，写入 `messages`。
6. 记录 `active_assistant_index`，发出 `message_added`。
7. `AIMessageBubble::set_message()` 收到空内容 assistant 后只渲染空 assistant 占位。
8. 请求等待状态由 `AIAgentDock` 中放在输入框上方的 indeterminate `ProgressBar` 显示。

这一步的作用是：即使 LLM API 还没有返回，用户也能看到明确的等待状态。

### 2.3 收集只读上下文

文件：

- `editor/ai_component/agent/ai_agent_session.cpp`
- `editor/ai_component/context/ai_editor_context_provider.cpp`
- `editor/ai_component/context/ai_project_tree_context_provider.cpp`

`AIAgentSession::_collect_context()` 会收集两类上下文：

1. `AIEditorContextProvider::collect_context()`
   - 当前阶段返回安全、只读的编辑器上下文说明。
2. `AIProjectTreeContextProvider::collect_context()`
   - 扫描 `res://` 下的项目树。
   - 有最大深度、条目数和字符数限制。
   - 过长时设置 `truncated` 并追加 `[truncated]`。

这些上下文不会直接修改会话历史，而是在 runtime 请求模型时作为 system/context 消息注入。

### 2.4 启动 Agent Runtime

文件：

- `editor/ai_component/agent/ai_agent_session.cpp`
- `editor/ai_component/agent/ai_agent_runtime_runner.cpp`

`send_user_message()` 在收集上下文后会进入 `AI_AGENT_STATE_STREAMING`。当前实际接入路径是 function calling runtime：

1. 从 `messages` 复制出 `request_messages`。
2. 移除刚才用于 UI loading 的空 assistant 占位，避免把空 assistant 发给模型。
3. 调用 `runtime_runner->start(request_messages, context)`。
4. `AIAgentRuntimeRunner` 在后台线程执行 `runtime->run(messages, context_documents)`。

这里虽然状态名仍叫 `STREAMING`，但 runtime 路径当前是非流式 function calling：UI 会保持 loading，直到整个工具调用循环结束后一次性回写结果。

## 3. Agent Runtime 工具调用循环

文件：

- `editor/ai_component/agent/ai_agent_runtime.cpp`
- `editor/ai_component/agent/ai_agent_profile.cpp`
- `editor/ai_component/tools/ai_tool_registry.cpp`
- `editor/ai_component/tools/ai_tool_permission.cpp`

`AIAgentRuntime::run()` 是 Agent 核心循环。

### 3.1 构建发送给模型的消息和上下文预算

文件：

- `editor/ai_component/agent/ai_context_manager.cpp`
- `editor/ai_component/agent/ai_context_manager.h`
- `editor/ai_component/agent/ai_agent_runtime.cpp`

`AIAgentRuntime::run()` 每一轮 provider 请求前都会调用：

```cpp
AIContextBuildResult context_result = context_manager->build_messages(
        String(AIAgentPrompts::SYSTEM_PROMPT),
        result.messages,
        p_context_documents);
```

`AIContextManager` 是当前上下文管理的唯一入口，职责是把内部会话消息和只读项目上下文整理成 provider client 可以消费的消息数组：

1. 插入 system prompt，来自 `prompts/agent_system_prompt.h`。
2. 把 `AIEditorContextProvider`、`AIProjectTreeContextProvider` 收集到的只读上下文合并成一条 system/context 消息。
3. 将内部 `AIAgentMessage` 转成 provider 消息字典。
4. 按字符预算裁剪上下文文档、历史消息和工具结果。
5. 返回 `metadata`，记录 `estimated_input_chars`、`omitted_history_messages`、`truncated_tool_results`、`truncated_context_documents` 等信息，便于终端日志和后续调试。

当前预算是字符级近似，不是模型 tokenizer 级 token 预算。默认值：

| 配置项 | 默认值 | 作用 |
| --- | ---: | --- |
| `max_input_chars` | 96000 | 发送给 provider 的整体输入字符预算 |
| `max_context_chars` | 24000 | 只读编辑器/项目上下文预算 |
| `max_history_chars` | 64000 | 历史会话消息预算 |
| `max_tool_result_chars` | 16000 | 单条 tool result 内容预算 |
| `min_recent_messages` | 4 | 即使历史超预算，也尽量保留的最近消息数量 |

裁剪策略参考了 OpenCode 等 Agent 工具的常见模式：优先保留最近交互，对旧历史和工具输出做剪枝，把只读项目上下文作为受限系统上下文注入。OpenCode 官方文档在 `compaction` 配置中提供了 `auto`、`prune` 和 `reserved` 选项，其中 `prune` 用于移除旧工具输出以节省上下文：https://opencode.ai/docs/config/#compaction 。NEXT Engine 当前阶段没有做 LLM 总结式 compaction，而是先实现确定性的预算裁剪，后续可以在 `AIContextManager` 内扩展 summary/compaction。

function calling 消息有协议约束：assistant 的 `tool_calls` 必须和后续 `tool` 消息成组出现。因此 `AIContextManager` 不会单独保留孤立 tool result，也不会保留缺少结果的 assistant tool-call 组；旧历史超预算时以“普通消息”或“assistant tool-call + tool results 组”为单位删除，避免 DeepSeek/OpenAI-compatible provider 因消息序列非法而返回 400。

### 3.2 暴露允许的工具 schema

`AIAgentRuntime::_get_allowed_tool_schemas()` 根据当前 `AIAgentProfile` 过滤工具。

当前 `AIAgentProfile::get_plan_profile()` 和 `get_build_profile()` 都只允许只读工具：

- `project.list_tree`
- `project.read_file`
- `project.search_text`
- `editor.get_context`

`AIToolRegistry::get_tool_schemas()` 会把工具转成内部 OpenAI function schema：

```json
{
  "type": "function",
  "function": {
    "name": "project.read_file",
    "description": "...",
    "parameters": { "...": "..." }
  }
}
```

注意：内部工具名允许使用点号命名空间，例如 `project.read_file`。部分 OpenAI-compatible 服务商会要求 function name 只能匹配 `^[a-zA-Z0-9_-]+$`，不能包含点号。因此真正发送 HTTP 请求前，`AIOpenAICompatibleRuntimeClient::_build_provider_tool_schemas()` 会把内部工具名转换成 provider-safe 名称，例如：

```text
project.read_file -> project_read_file
```

同时 runtime client 会保存 `provider_safe_name -> internal_name` 映射。模型返回 `tool_calls` 后，`AIOpenAICompatibleRuntimeClient::_apply_tool_name_map()` 会把 `project_read_file` 恢复为内部的 `project.read_file`，再交给权限系统和 `AIToolRegistry` 执行。Tool 层、权限层、历史消息仍然统一使用内部工具名。

### 3.3 Provider 回合

`AIAgentRuntime::run()` 最多执行 `max_provider_turns` 次，默认 6 次。

每一轮：

1. 调用 `client->complete(messages, tool_schemas)`。
2. 如果 response 有 `error`，结束并返回失败。
3. 如果 response 没有 `tool_calls`：
   - 如果有最终文本，追加一条 `assistant` 消息。
   - 标记成功并结束。
4. 如果 response 有 `tool_calls`：
   - 追加一条 `assistant` 消息，内容可以为空，但 metadata 里保存 `tool_calls`。
   - 逐个执行工具。
   - 每个工具执行结果追加为 `tool` 消息。
   - 进入下一轮，把 tool 结果发回模型继续推理。

工具调用总数受 `max_tool_calls` 限制，默认 20 次。超过后失败关闭，避免模型无限循环。

### 3.4 工具权限判断

文件：

- `editor/ai_component/tools/ai_tool_permission.cpp`

每个 tool call 执行前都会经过：

```cpp
AIToolPermissionPolicy::evaluate(profile, call.tool_name, call.arguments)
```

当前策略很保守：

- profile 允许的工具：`allow`
- profile 未允许的工具：`deny`

被拒绝的工具不会执行，会生成一条 `tool` 消息，内容类似：

```text
Tool call denied for `xxx`: Tool is not allowed by the active agent profile.
```

这样模型可以收到拒绝结果，并继续生成最终回答。

## 4. Provider 与 OpenAI-compatible 协议

### 4.1 Runtime Client

文件：

- `editor/ai_component/providers/ai_openai_runtime_client.cpp`
- `editor/ai_component/providers/ai_openai_runtime_client.h`

`AIOpenAICompatibleRuntimeClient::complete()` 负责 function calling 请求：

1. 调用 `_build_chat_messages()`，把内部消息转换成 OpenAI-compatible Chat Completions 格式。
2. 调用 `_build_provider_tool_schemas()`，把内部工具名转换成 provider-safe function name，并记录映射表。
3. 调用 `AIOpenAIHTTPRuntimeTransport::request_chat_completion()` 发送 HTTP 请求。
4. 调用 `AIOpenAICompatibleCodec::parse_chat_completion()` 解析响应。
5. 调用 `_apply_tool_name_map()`，把 provider-safe 工具名恢复成内部工具名。
6. 返回 `AIAgentRuntimeResponse`。

内部消息到 OpenAI-compatible 消息的关键转换：

- 内部 assistant tool call：

```cpp
assistant.metadata["tool_calls"]
```

会转换成：

```json
{
  "role": "assistant",
  "content": "",
  "tool_calls": [
    {
      "id": "call_xxx",
      "type": "function",
      "function": {
        "name": "project_read_file",
        "arguments": "{\"path\":\"res://player.gd\"}"
      }
    }
  ]
}
```

- 内部 tool result：

```cpp
role = "tool"
metadata["tool_call_id"] = "call_xxx"
```

会转换成：

```json
{
  "role": "tool",
  "tool_call_id": "call_xxx",
  "content": "工具返回内容"
}
```

### 4.2 HTTP Transport

文件：

- `editor/ai_component/providers/ai_openai_runtime_client.cpp`

`AIOpenAIHTTPRuntimeTransport::request_chat_completion()` 使用 Godot `HTTPClient`：

1. 校验 `api_key` 和 `base_url`。
2. `base_url.parse_url()` 得到 scheme/host/port/base_path。
3. `connect_to_host()` 建立 HTTP/HTTPS 连接。
4. 组装请求头：
   - `Authorization: Bearer <api_key>`
   - `Content-Type: application/json`
   - `Accept: application/json`
5. 调用 `AIOpenAICompatibleCodec::build_body(..., stream=false)` 构造 body。
6. 请求路径由 `build_request_path()` 统一补成 `/chat/completions`。
7. 读取完整响应 body。

当前 function calling 路径是非流式，因此等待期间 UI 显示 loading，响应完成后统一更新消息。默认请求超时时间为 180 秒，用来覆盖慢模型和工具调用多轮请求的常见延迟；后续如果在设置页暴露 timeout，应继续写入 `AIProviderConfig::timeout_seconds`。

### 4.3 Codec 与 `<null>` 防护

文件：

- `editor/ai_component/providers/ai_openai_compatible_codec.cpp`
- `editor/ai_component/providers/ai_openai_compatible_codec.h`

`AIOpenAICompatibleCodec` 只负责 OpenAI-compatible Chat Completions 协议的纯数据转换，不持有网络连接、不发信号、不管理线程：

- `build_request_path()` 把 `base_url` path 统一补成 `/chat/completions`。
- `build_body()` 构造请求 JSON body。
- `parse_chat_completion()` 解析非流式响应。
- `extract_delta()` 保留为流式 delta 解析工具，后续如果重新做 streaming runtime 可复用。

`parse_chat_completion()` 解析非流式响应：

- `choices[0].message.content` 非 null 时写入 `response.content`。
- `choices[0].message.tool_calls` 会转换成内部 `AIToolCall`。
- `finish_reason` 非 null 时才写入 metadata。

这次修复的重点之一是避免 JSON `null` 被 Godot `String(Variant())` 转成字面量 `"<null>"`。因此流式 `_extract_delta()` 和非流式 `parse_chat_completion()` 都做了 `Variant::NIL` 检查。

旧的 `AIOpenAICompatibleProvider::
()` 和 `AIAgentRunner` fallback 已移除。当前 AI Dock 只有 runtime function calling 主路径，避免旧 streaming provider 与新 Agent runtime 同时存在造成职责混乱。

## 5. 当前内置工具

### 5.1 project.list_tree

文件：

- `editor/ai_component/tools/project/ai_list_project_tool.cpp`

功能：

- 列出 `res://` 下的目录和文件。
- 参数：
  - `path`
  - `max_depth`
  - `max_entries`
  - `max_chars`
- 有深度、条目和字符限制。
- 不列出隐藏文件。

### 5.2 project.read_file

文件：

- `editor/ai_component/tools/project/ai_read_file_tool.cpp`

功能：

- 读取 `res://` 下允许扩展名的文本文件。
- 参数：
  - `path`
  - `max_bytes`
- 只允许文本扩展名，例如 `gd/cs/tscn/tres/md/txt/json/cfg/shader/gdshader`。
- 有读取字节限制，超出后标记 `truncated`。

### 5.3 project.search_text

文件：

- `editor/ai_component/tools/project/ai_search_project_tool.cpp`

功能：

- 在 `res://` 下允许扩展名的文本文件中做字面量搜索。
- 参数：
  - `query`
  - `path`
  - `max_results`
  - `max_chars`
- 返回格式类似：

```text
res://player.gd:12: matching line preview
```

### 5.4 editor.get_context

文件：

- `editor/ai_component/tools/editor/ai_get_editor_context_tool.cpp`

功能：

- 返回安全的只读编辑器/项目信息。
- 包括：
  - engine name/version
  - project name
  - project path
  - capability 标记为 read-only
- 不返回 API Key、Authorization 等敏感信息。

### 5.5 工具安全边界

文件：

- `editor/ai_component/tools/project/ai_project_tool_utils.cpp`

项目工具共同使用 `AIProjectToolUtils`：

- `is_allowed_path()` 要求路径必须以 `res://` 开头，且不能包含 `..`。
- `is_allowed_text_extension()` 限制可读取/搜索的文本文件扩展名。
- `get_int_argument()` 对工具参数做最小/最大值 clamp。

当前所有工具都是只读工具，不会修改项目文件、场景或节点。

## 6. UI 消息和历史会话

### 6.1 消息渲染

文件：

- `editor/ai_component/ui/ai_message_list.cpp`
- `editor/ai_component/ui/ai_message_bubble.cpp`

`AIMessageList` 管理消息气泡：

- `add_message()`
- `update_message()`
- `remove_message()`
- `clear_messages()`

`AIMessageBubble` 根据 role 渲染标题：

- `user` -> `You`
- `assistant` -> `Assistant`
- `tool` -> `Tool: <tool_name> (<status>)`
- `error` -> `Error`

`RichTextLabel::set_use_bbcode(false)` 用于避免用户文本或模型文本里的 `[b]...[/b]` 被误当 BBCode 渲染。

### 6.2 空 assistant 占位和 loading

文件：

- `editor/ai_component/ui/ai_message_bubble.cpp`
- `editor/ai_component/agent/ai_agent_session.cpp`

当 `assistant` 内容为空时，UI 只保留 pending 占位消息，不再在气泡内显示文字动画。请求等待状态由 `AIAgentDock` 里的 `request_progress` 显示，它位于聊天输入组件上方，进入 `PREPARING_CONTEXT` 或 `STREAMING` 状态时显示，进入 `IDLE`、`FAILED` 或 `CANCELLED` 后隐藏。

runtime 完成后：

1. `AIAgentSession::_apply_runtime_result()` 检查 `active_assistant_index`。
2. 如果占位 assistant 仍为空，调用 `_remove_message_at()`。
3. 发出 `message_removed(index)`。
4. `AIAgentDock::_message_removed()` 调用 `AIMessageList::remove_message(index)`。
5. 再追加 runtime 返回的真实 assistant/tool 消息。

请求失败时同样会移除空占位，然后追加 `error` 消息。

### 6.3 历史 session 管理

文件：

- `editor/ai_component/storage/ai_conversation_store.cpp`
- `editor/ai_component/storage/ai_conversation_serializer.cpp`
- `editor/ai_component/ui/ai_agent_dock.cpp`

`AIConversationStore` 把会话保存到 user data 下：

```text
ai_agent/conversations/<session_id>.json
```

保存内容包括：

- `id`
- `title`
- `updated_at`
- `messages`

`AIAgentDock` 顶部使用 `OptionButton` 显示历史 session：

- `New` 创建新会话。
- 下拉选择历史会话时调用 `session->load_session(session_id)`。
- 读取后用 `_reload_messages_from_session()` 重建消息列表。

## 7. 模型设置与模型切换

文件：

- `editor/ai_component/ui/ai_agent_settings_dialog.cpp`
- `editor/ai_component/providers/ai_model_settings.cpp`
- `editor/ai_component/ui/ai_composer.cpp`

AI Settings 当前有四个侧边栏页面：

- Models：已实现。
- MCP：占位，待实现。
- Skills：占位，待实现。
- Rules：占位，待实现。

Models 页面支持：

- 多 provider 预设。
- 每个 provider 的 API Key。
- 每个 provider 的 Base URL。
- 预设模型开关。
- 自定义模型列表。

`AIModelSettings` 使用 `EditorSettings` 保存配置，路径形如：

```text
ai_agent/providers/<provider_id>/api_key
ai_agent/providers/<provider_id>/base_url
ai_agent/models/<provider_id>:<model>/enabled
ai_agent/providers/<provider_id>/custom_models
```

对话发送时，`AIComposer::get_selected_model()` 返回当前选择的 model id，`AIAgentDock::_get_provider_config()` 再把它解析为 `AIProviderConfig`。

## 8. 当前状态机

文件：

- `editor/ai_component/agent/ai_agent_types.h`
- `editor/ai_component/agent/ai_agent_session.cpp`

当前状态：

- `AI_AGENT_STATE_IDLE`
- `AI_AGENT_STATE_PREPARING_CONTEXT`
- `AI_AGENT_STATE_STREAMING`
- `AI_AGENT_STATE_CANCELLED`
- `AI_AGENT_STATE_FAILED`

发送消息时：

```text
IDLE
  -> PREPARING_CONTEXT
  -> STREAMING
  -> IDLE / FAILED / CANCELLED
```

虽然状态名叫 `STREAMING`，但 function calling runtime 当前是非流式；这个状态在 UI 上表示“请求进行中”。

## 9. 当前限制与后续扩展建议

当前已稳定的能力：

- 模型配置和切换。
- 历史 session。
- OpenAI-compatible function calling 基础闭环。
- 只读项目工具。
- loading 等待状态。
- `<null>` 响应防护。
- BBCode 不误渲染。

当前限制：

- function calling 路径暂不流式输出 token。
- Cancel 对 runtime 的 HTTP 请求还不是强中断，只会让 UI 进入取消状态并忽略后续结果。
- 工具当前只读，不支持编辑文件、创建节点、修改场景。
- MCP、Skills、Rules 设置页仍是占位。
- 权限策略只有 allow/deny，`ask` 还没有 UI 交互。

建议的后续扩展方向：

1. 把 `AI_AGENT_STATE_STREAMING` 拆成更准确的 `REQUESTING` / `RUNNING_TOOLS` / `STREAMING_RESPONSE`。
2. 为 runtime transport 增加取消令牌，取消时真正终止 HTTP 请求。
3. 增加工具执行事件信号，让 UI 能显示“正在读取文件/搜索项目”的实时进度。
4. 在 Tool 层新增写入类工具前，先实现权限确认 UI 和审计记录。
5. MCP 后续应作为 Tool Provider 接入 `AIToolRegistry`，不要侵入 Session/UI 主流程。
6. Skills 和 Rules 应转化为 system prompt/context/profile 的输入，而不是散落在 provider 代码里。
