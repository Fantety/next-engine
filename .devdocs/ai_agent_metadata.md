# AI Agent Metadata

本文档记录 NEXT Engine AI Agent 当前自定义数据集合中的 `metadata` 结构。`metadata` 的定位是扩展字段容器：核心字段保持稳定，工具调用、上下文裁剪、token 统计等可演进信息放在 `metadata` 中，避免频繁改动会话存储格式。

## AIAgentMessage.metadata

文件：`editor/ai_component/agent/ai_agent_message.h`

`AIAgentMessage` 是会话持久化的基础消息结构，`metadata` 会通过 `AIConversationSerializer` 原样保存和恢复。

进入 provider 请求前，`AIContextManager` 会剔除只给本地 UI/统计使用的 `usage` 和 `estimated_context_usage`，避免这些字段进入上下文预算或 provider 消息。`tool_calls`、`reasoning_content` 等协议相关字段会保留。

常见结构：

```json
{
  "tool_calls": [],
  "reasoning_content": "...",
  "finish_reason": "stop",
  "usage": {
    "prompt_tokens": 120,
    "completion_tokens": 30,
    "total_tokens": 150,
    "source": "provider"
  },
  "estimated_context_usage": {
    "estimated_input_chars": 2400,
    "estimated_input_tokens": 600,
    "max_input_chars": 60000,
    "omitted_history_messages": 0,
    "truncated_tool_results": 0,
    "truncated_context_documents": 0,
    "source": "context_estimate"
  }
}
```

- `tool_calls`：assistant 消息请求执行工具时保存的工具调用数组。后续 provider 请求会通过它还原 OpenAI-compatible `tool_calls`。
- `reasoning_content`：DeepSeek 等 thinking 模型返回的推理内容。再次请求同一上下文时必须传回 provider，否则部分模型会返回协议错误。
- `finish_reason`：provider 返回的结束原因，例如 `stop`、`tool_calls`。
- `usage`：provider 实际返回的 token 用量。当前用于会话 token 累计展示。
- `estimated_context_usage`：本轮请求前由 `AIContextManager` 估算的上下文消耗和裁剪状态。它不是精确 tokenizer 结果，只用于 UI 提示和调试。

`tool_calls` 数组元素来自 `AIToolCall::to_dict()`：

```json
{
  "id": "call_xxx",
  "tool_name": "project.read_file",
  "arguments": {
    "path": "res://player.gd"
  },
  "status": "pending",
  "created_at": 1779005850,
  "updated_at": 1779005850
}
```

- `id`：provider 生成的工具调用 ID，用于和后续 tool result 对齐。
- `tool_name`：NEXT Engine 内部工具名。发送给 provider 前会映射为兼容 OpenAI 规则的名称，例如 `project.read_file` 会映射为 `project_read_file`。
- `arguments`：模型请求工具时传入的参数字典。
- `status`：runtime 内部状态，保存后可用于 UI 展示和调试。
- `created_at` / `updated_at`：工具调用生命周期时间戳。

tool 消息的 `metadata`：

```json
{
  "tool_call_id": "call_xxx",
  "tool_name": "project.read_file",
  "status": "completed",
  "truncated": false,
  "path": "res://player.gd",
  "bytes": 1024
}
```

- `tool_call_id`：关联 assistant `tool_calls` 中的调用 ID。
- `tool_name`：内部工具名。
- `status`：`completed`、`failed`、`denied` 等工具执行状态。
- `truncated`：工具输出是否被工具自身截断。
- 其他字段来自具体工具结果，例如 `path`、`bytes`、`query`、`result_count`。

## AIAgentRuntimeResponse.metadata

文件：`editor/ai_component/agent/ai_agent_runtime.h`

`AIAgentRuntimeResponse` 是 provider client 返回给 runtime 的单轮响应。其 `metadata` 不直接持久化，但会复制到新生成的 assistant message。

结构：

```json
{
  "reasoning_content": "...",
  "finish_reason": "tool_calls",
  "usage": {
    "prompt_tokens": 120,
    "completion_tokens": 30,
    "total_tokens": 150,
    "source": "provider"
  },
  "estimated_context_usage": {}
}
```

- `usage` 由 `AIOpenAICompatibleCodec` 从 provider 顶层 `usage` 字段解析。
- `estimated_context_usage` 由 `AIAgentRuntime` 在收到 provider 响应后写入，表示触发该响应的上下文估算。

## AIAgentRuntimeResult.metadata

文件：`editor/ai_component/agent/ai_agent_runtime.h`

`AIAgentRuntimeResult` 是一次完整 agent run 的结果，可能包含多轮 provider 调用和多次工具执行。

结构：

```json
{
  "last_context": {},
  "token_usage": {
    "prompt_tokens": 240,
    "completion_tokens": 60,
    "total_tokens": 300,
    "source": "provider"
  }
}
```

- `last_context`：最后一次 provider turn 使用的 `AIContextBuildResult.metadata`。
- `token_usage`：本次 runtime run 内所有 provider turn 返回 `usage` 的累计值。

## AIContextBuildResult.metadata

文件：`editor/ai_component/agent/ai_context_manager.h`

`AIContextManager` 负责把会话历史、系统提示和项目上下文整理成 provider 消息，并控制字符预算。

结构：

```json
{
  "estimated_input_chars": 2400,
  "max_input_chars": 60000,
  "max_context_chars": 12000,
  "max_history_chars": 42000,
  "max_tool_result_chars": 12000,
  "input_messages": 12,
  "output_messages": 8,
  "omitted_history_messages": 4,
  "truncated_tool_results": 1,
  "truncated_context_documents": 0
}
```

- `estimated_input_chars`：最终 provider 消息数组的字符级估算。
- `max_*`：当前上下文预算配置。
- `input_messages` / `output_messages`：裁剪前后消息数量。
- `omitted_history_messages`：因预算不足移除的历史消息数。
- `truncated_tool_results`：被裁剪内容的 tool result 数量。
- `truncated_context_documents`：被裁剪内容的项目上下文文档数量。

## AIToolResult.metadata

文件：`editor/ai_component/tools/ai_tool.h`

每个工具执行返回 `AIToolResult`。runtime 会把该 metadata 复制到 tool message metadata，并补充 `tool_call_id`、`tool_name`、`status`、`truncated`。

当前内置工具字段：

- `project.list_tree`：`path`、`entry_count`。
- `project.read_file`：`path`、`bytes`。
- `project.search_text`：`path`、`query`、`result_count`。
- `editor.get_context`：`resource_path`、`application_name`、`engine_name`、`engine_version`、`capabilities`。

## AIConversationStore Metadata

文件：`editor/ai_component/storage/ai_conversation_store.cpp`

会话文件根对象：

```json
{
  "id": "session_id",
  "title": "Chat title",
  "updated_at": 1779005850,
  "messages": []
}
```

会话列表项：

```json
{
  "id": "session_id",
  "title": "Chat title",
  "updated_at": 1779005850,
  "message_count": 8,
  "path": "user://ai_agent/projects/<scope>/conversations/<id>.json"
}
```

- `updated_at` 用于会话列表排序和显示时间。
- `message_count` 用于列表摘要和后续筛选。
- `path` 仅用于本地存储定位，不应暴露给 provider。

## Token Usage 规则

当前 token 展示分两类：

- 精确 provider 用量：来自 provider HTTP 响应顶层 `usage`，写入 assistant message 的 `metadata.usage`。
- 上下文估算：来自 `AIContextManager.metadata.estimated_input_chars`，按 `chars / 4` 粗略换算成 `estimated_input_tokens`。

`AIAgentSession` 不维护单独存储 schema，而是每次从消息 `metadata.usage` 重算当前会话累计值：

```json
{
  "prompt_tokens": 360,
  "completion_tokens": 90,
  "total_tokens": 450,
  "estimated_input_tokens": 1200,
  "estimated_input_chars": 4800,
  "message_count": 3
}
```

这样旧会话仍可正常加载；没有 `usage` 的旧消息会按 0 处理。
