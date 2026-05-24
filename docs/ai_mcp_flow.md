# NEXT Engine AI Agent MCP Flow

本文档描述当前 MCP 接入在 AI Agent 中的完整链路，包括启动时初始化检查、失败处理、MCP 列表状态展示、MCP 工具如何注册进运行时、工具 schema 如何进入模型请求，以及运行时工具调用流程。

## 范围

当前 MCP 支持三类 transport：

- `stdio`
- `streamable_http`
- `sse`

MCP 被实现为 AI Agent 工具系统的动态工具来源。MCP transport、发现服务、状态展示、Agent Session、Runtime 和模型 provider 之间保持分层：

- transport client 只负责协议 I/O。
- `AIMCPToolDiscovery` 只负责一次 discovery 的状态记录、server 检查和结果整理。
- `AIMCPToolDiscoveryRunner` 在线程中执行可能阻塞的 discovery。
- `AIMCPService` 是 MCP 的应用级服务，负责统一 refresh、状态快照、工具快照和变更信号。
- `AIAgentSession` 不再执行 MCP discovery，只消费 `AIMCPService` 的工具快照并注册到自己的 `AIToolRegistry`。
- UI 只订阅 `AIMCPService` 状态，不直接驱动 transport。

当前没有后台周期性健康检查。MCP 在 AI Dock 初始化和 MCP 设置变更时触发 refresh；refresh 正在运行时会合并后续请求，不会在 UI 线程中等待网络或进程 I/O。

## 关键文件

入口与类型注册：

- `editor/register_editor_types.cpp`
- `editor/editor_node.cpp`

MCP 服务与发现：

- `editor/ai_component/agent/ai_mcp_service.h`
- `editor/ai_component/agent/ai_mcp_service.cpp`
- `editor/ai_component/agent/ai_mcp_tool_discovery.h`
- `editor/ai_component/agent/ai_mcp_tool_discovery.cpp`
- `editor/ai_component/agent/ai_mcp_tool_discovery_runner.h`
- `editor/ai_component/agent/ai_mcp_tool_discovery_runner.cpp`
- `editor/ai_component/providers/ai_mcp_status_tracker.h`
- `editor/ai_component/providers/ai_mcp_status_tracker.cpp`

配置与 UI：

- `editor/ai_component/providers/ai_mcp_settings.h`
- `editor/ai_component/providers/ai_mcp_settings.cpp`
- `editor/ai_component/ui/ai_agent_dock.h`
- `editor/ai_component/ui/ai_agent_dock.cpp`
- `editor/ai_component/ui/ai_agent_settings_dialog.h`
- `editor/ai_component/ui/ai_agent_settings_dialog.cpp`
- `editor/ai_component/ui/ai_settings_mcp_page.h`
- `editor/ai_component/ui/ai_settings_mcp_page.cpp`
- `editor/ai_component/ui/ai_mcp_server_dialog.h`
- `editor/ai_component/ui/ai_mcp_server_dialog.cpp`

transport 与协议：

- `editor/ai_component/providers/ai_mcp_client.h`
- `editor/ai_component/providers/ai_mcp_client.cpp`
- `editor/ai_component/providers/ai_mcp_stdio_client.h`
- `editor/ai_component/providers/ai_mcp_stdio_client.cpp`
- `editor/ai_component/providers/ai_mcp_http_client.h`
- `editor/ai_component/providers/ai_mcp_http_client.cpp`
- `editor/ai_component/providers/ai_mcp_protocol.h`
- `editor/ai_component/providers/ai_mcp_protocol.cpp`

工具、Session、Runtime 和 provider：

- `editor/ai_component/tools/ai_mcp_tool.h`
- `editor/ai_component/tools/ai_mcp_tool.cpp`
- `editor/ai_component/tools/ai_tool_registry.h`
- `editor/ai_component/tools/ai_tool_registry.cpp`
- `editor/ai_component/agent/ai_agent_session.h`
- `editor/ai_component/agent/ai_agent_session.cpp`
- `editor/ai_component/agent/ai_agent_runtime.h`
- `editor/ai_component/agent/ai_agent_runtime.cpp`
- `editor/ai_component/providers/ai_openai_runtime_client.h`
- `editor/ai_component/providers/ai_openai_runtime_client.cpp`
- `editor/ai_component/providers/ai_openai_compatible_codec.h`
- `editor/ai_component/providers/ai_openai_compatible_codec.cpp`

测试覆盖：

- `tests/editor/test_ai_model_settings.cpp`
- `tests/editor/test_ai_agent_tools.cpp`
- `tests/editor/test_ai_agent_runtime.cpp`

## 启动时初始化链路

1. `editor/register_editor_types.cpp` 注册 AI Agent、MCP client、MCP status/discovery/runner/service、MCP tool 等类型。
2. `editor/editor_node.cpp` 创建 `AIAgentSettingsDialog` 和 `AIAgentDock`。
3. `AIAgentDock` 构造 UI，其中 MCP 状态是顶部一个很小的 `MCP` 按钮；默认收起，点击后展开 server 列表。
4. `AIAgentDock` 连接 `AIMCPService::status_changed`，并调用 `AIMCPService::refresh()` 启动一次 discovery。
5. `AIAgentDock::_ensure_session()` 创建单个 `AIAgentSession`。切换聊天会话不会创建新的 `AIAgentSession` 对象。
6. `AIAgentSession::AIAgentSession()` 调用 `_configure_tool_runtime()` 注册内置工具，然后调用 `_register_mcp_tools_from_service()` 读取 service 当前缓存的 MCP tools。此处不会启动 discovery。
7. `AIAgentSession` 连接 `AIMCPService::tools_changed`。当 MCP discovery 后续完成时，Session 再重建 tool runtime，把最新 MCP tools 注册进 registry。

启动时如果 discovery 尚未完成，Session 先只带内置工具可用；discovery 完成后由 `tools_changed` 驱动一次非阻塞 runtime reload。

## MCP Refresh 详细流程

`AIMCPService::refresh()` 是唯一的 discovery 入口。

```cpp
AIMCPService::refresh():
    if runner is running:
        refresh_pending = true
        return
    servers = AIMCPSettings::get_server_status_configs()
    discovery->begin_refresh(servers)
    discovered_tools.clear()
    active_refresh_request_id = next_refresh_request_id++
    emit status_changed(checking/disabled snapshot)
    runner->start(servers, active_refresh_request_id, 3000)
```

后台线程流程：

```cpp
AIMCPToolDiscoveryRunner thread:
    results = AIMCPToolDiscovery::discover_servers(servers, timeout)
    set last_results and last_finished_request_id under mutex
    running.clear()
    emit discovery_finished deferred
```

主线程收尾：

```cpp
AIMCPService::_on_discovery_finished():
    if finished_request_id != active_refresh_request_id:
        ignore stale result
        maybe run pending refresh
        return
    discovery->apply_results(results)
    discovered_tools = AIMCPToolDiscovery::build_discovered_tools(results)
    active_refresh_request_id = 0
    emit status_changed(final snapshot)
    emit tools_changed()
    if refresh_pending:
        refresh()
```

关键点：

- discovery I/O 不在 UI 线程执行。
- 多次 refresh 会 coalesce，不会并发启动多个 runner。
- request id 防止旧 discovery 结果覆盖新配置。
- `tools_changed` 只在 service 工具快照更新后发出，Session 由此重载工具运行时。

## MCP 可用性检查

一个启用的 MCP server 需要满足以下条件才会记录为 `ok`：

1. 配置结构有效：
   - `stdio` 必须有 `command`。
   - `streamable_http` / `sse` 必须有 `url`。
2. `AIMCPClientFactory::create_client()` 能创建对应 transport client。
3. `client->list_tools()` 成功。
4. `list_tools()` 内部完成 MCP `initialize`、`notifications/initialized` 和 `tools/list`。
5. `tools/list` 结果能被 `AIMCPProtocol::parse_tools_list_result()` 解析。

失败处理：

- 禁用 server 记录为 `disabled`。
- 配置无效、client 创建失败、连接失败、initialize 失败、tools/list 超时或解析失败记录为 `failed`。
- 失败 server 不会阻断其他 server discovery。
- 失败 server 的工具不会进入 `AIMCPService` 工具快照，也不会进入模型请求。
- AI Dock 通过 `EditorToaster` 给出非阻塞提示，并在 MCP 小按钮中显示失败数量。
- AI Settings 的 MCP 列表会显示状态色标：绿色 `ok`，红色 `failed`，灰色 `disabled`，黄色 `checking` 或未知。

本机 HTTP/SSE server 日志中的 `http://0.0.0.0:8000/mcp` 表示服务监听所有网卡。用户配置 URL 时建议写 `http://127.0.0.1:8000/mcp` 或 `http://localhost:8000/mcp`。当前 HTTP client 也会把配置中的 `0.0.0.0` 连接目标映射到 `127.0.0.1`。

## 设置变更链路

MCP 设置页的操作包括添加、编辑、删除、启用/禁用 server，以及 JSON 导入。

流程：

1. `AISettingsMCPPage` 写入 `AIMCPSettings` 并发出 `settings_changed`。
2. `AIAgentSettingsDialog::_save_mcp_settings()` 保存 `EditorSettings` 并发出 `ai_mcp_settings_changed`。
3. `AIAgentDock::_mcp_settings_changed()` 调用 `AIMCPService::refresh()`。
4. service 立即发出 `checking` 状态，设置页和 Dock 只刷新 UI，不等待 discovery。
5. discovery 完成后 service 发出 `status_changed` 和 `tools_changed`。
6. `AISettingsMCPPage` 订阅 `status_changed`，刷新 MCP 列表前面的状态色标。
7. `AIAgentDock` 订阅 `status_changed`，刷新 MCP 小按钮和展开列表。
8. `AIAgentSession` 订阅 `tools_changed`，调用 `reload_tool_runtime()`。

如果 Session 正在 streaming、preparing context 或 waiting tool approval，`reload_tool_runtime()` 不会立即重建，而是设置 `pending_tool_runtime_reload = true`，当前轮次结束、失败或取消后再重建。

模型/provider 设置变更走独立信号 `ai_settings_changed`，只刷新模型列表，不触发 MCP discovery。

## JSON 导入

JSON 导入入口：

- `AISettingsMCPPage::_json_import_confirmed()`
- `AIMCPSettings::import_servers_from_json()`

支持形态：

- 根对象是单个 server。
- 根对象是 server 数组。
- `servers` 是数组。
- `servers` 是 map。
- `mcpServers` 是 map。

示例：

```json
{
  "mcpServers": {
    "filesystem": {
      "command": "npx",
      "args": ["-y", "@modelcontextprotocol/server-filesystem", "."]
    },
    "remote": {
      "type": "streamable_http",
      "url": "https://example.com/mcp",
      "headers": {
        "Authorization": "Bearer token"
      }
    }
  }
}
```

映射规则：

- `display_name` 或 `name` 用作显示名；`mcpServers` map key 也会作为候选显示名。
- `transport` 或 `type` 用作 transport。
- 有 `url` 但没有显式 transport 时，默认按 `streamable_http` 处理。
- `streamablehttp`、`streamable_http`、`http` 归一化为 `streamable_http`。
- `sse` 保持为 `sse`。
- `args` 数组转换为命令行参数字符串。
- `env` 字典转换为多行 `KEY=value`。
- `headers` 字典转换为多行 `Header=Value`。

导入成功后仍走设置变更链路，由 `AIMCPService::refresh()` 异步检查可用性。

## 工具注册与去重

MCP tool descriptor 到 Agent tool 的名称转换由 `AIMCPProtocol::make_agent_tool_name()` 完成：

```text
mcp_<sanitized_server_id>_<sanitized_mcp_tool_name>
```

当前有三层去重保护：

1. `AIMCPToolDiscovery::build_discovered_tools()` 用 agent tool name 去重，重复 descriptor 不进入 service 工具快照。
2. `AIMCPService::register_discovered_tools()` 注册前调用 `AIToolRegistry::has_tool()`，已有同名工具时跳过。
3. `AIToolRegistry::register_tool()` 底层用 `HashMap<String, Ref<AITool>>` 存储，同名注册返回 `false`。

状态计数也按去重后的 agent tool name 记录，避免一个 server 返回重复 tool 时状态里显示虚高的工具数。

`AIAgentSession::_configure_tool_runtime()` 的顺序：

```cpp
_remove_dynamic_tool_permissions()
tool_registry->clear()
register built-in tools
_register_mcp_tools_from_service()
_apply_dynamic_tool_permissions()
runtime->set_tool_registry(tool_registry)
runtime->set_profile(agent_profile)
```

MCP tools 默认加入 `agent_profile.ask_tools`，并从 `allowed_tools` 移除，所以模型可以看到工具，但执行前需要用户确认。

## 工具列表如何进入模型请求

这里的“进入模型上下文”不是写入 system prompt，而是通过 OpenAI-compatible function calling 的 `tools` 字段传给 provider。

链路：

1. `AIMCPService` 保存 discovery 后的 `AIMCPDiscoveredTool` 快照。
2. `AIAgentSession::_register_mcp_tools_from_service()` 调用 `AIMCPService::register_discovered_tools(tool_registry)`。
3. 每个 discovered tool 包装为 `AIMCPTool`，注册到 `AIToolRegistry`。
4. `_apply_dynamic_tool_permissions()` 把 `mcp_` 前缀工具加入 `ask_tools`。
5. `AIAgentRuntime::run()` 调用 `_get_allowed_tool_schemas()`。
6. `_get_allowed_tool_schemas()` 从 `allowed_tools` 和 `ask_tools` 读取工具；如果同一个名字同时存在于两个集合，只加入一次。
7. 每个工具调用 `AITool::get_openai_schema()` 生成内部 schema。
8. `AIOpenAICompatibleRuntimeClient::_build_provider_tool_schemas()` 生成 provider-safe tool name，并维护 `provider_name -> internal_name` 映射。
9. `AIOpenAICompatibleCodec::_build_body()` 把 schema 数组写入请求 body 的 `tools` 字段。
10. Provider 返回 tool call 后，`_apply_tool_name_map()` 把 provider tool name 还原成内部 tool name。

请求 body 形态：

```json
{
  "model": "...",
  "stream": true,
  "messages": [],
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "mcp_filesystem_read_file",
        "description": "...",
        "parameters": {
          "type": "object",
          "properties": {}
        }
      }
    }
  ]
}
```

## Transport 行为

`AIMCPClientFactory::create_client()` 分派：

- `streamable_http` 使用 `AIMCPHTTPClient`。
- `sse` 使用 `AIMCPHTTPClient` 的 legacy SSE 路径。
- 其他情况使用 `AIMCPStdioClient`。

`stdio`：

- discovery 和 tool call 都是短生命周期。
- 每次 discovery 启动进程、initialize、tools/list，然后关闭。
- 每次 tool call 启动进程、initialize、tools/call，然后关闭。

`streamable_http`：

- 使用 HTTP POST 发送 JSON-RPC。
- 请求头包含 `Content-Type: application/json`、`Accept: application/json, text/event-stream`、`MCP-Protocol-Version: 2025-06-18`，以及用户配置的额外 headers。
- 支持 JSON 响应，也支持从 SSE body 的 `data:` 事件中解析 JSON-RPC 响应。
- 当前也是短生命周期，每次 discovery 或调用都会重新 initialize。

`sse`：

- 使用 legacy SSE MCP transport。
- 先 GET 打开 SSE stream，从 endpoint event 读取 message endpoint。
- 再通过 message endpoint POST `initialize`、`notifications/initialized`、`tools/list` 或 `tools/call`。
- 响应从 SSE stream 中按 JSON-RPC id 匹配读取。

## MCP 工具运行时调用

当模型返回 MCP tool call：

1. Provider 响应由 `AIOpenAICompatibleStreamAccumulator` 或 `AIOpenAICompatibleCodec` 解析成 `AIToolCall`。
2. provider tool name 被映射回内部 tool name。
3. `AIAgentRuntime::run()` 用 `AIToolPermissionPolicy::evaluate()` 判断权限。
4. MCP tools 默认是 `ASK`，所以 runtime 返回 `pending_approval`。
5. `AIAgentSession::_apply_runtime_result()` 进入 `AI_AGENT_STATE_WAITING_TOOL_APPROVAL`，并发出 `tool_approval_requested`。
6. 用户批准后，`AIAgentSession::approve_pending_tool()` 从 registry 获取 tool。
7. `AIMCPTool::execute()` 创建对应 transport client，用原始 MCP tool name 调用 `tools/call`。
8. transport 返回结果后转换为 `AIToolResult`。
9. Session 把结果追加为 `tool` message，再启动下一轮 runtime，把 tool result 交回模型。

如果调用阶段失败，`AIMCPTool::execute()` 返回带 `error` 的 `AIToolResult`，Session 会把失败写入 tool result message。这个失败会反馈给模型；它不同于 discovery 失败，discovery 失败的工具根本不会暴露给模型。

## MessageList 展示

MCP 工具执行结果会进入普通消息链路，但会带 metadata：

- `tool_origin = "mcp"`
- `mcp_server_id`
- `mcp_server_name`
- `mcp_tool_name`
- `mcp_transport`
- `mcp_agent_tool_name`

`editor/ai_component/ui/ai_message_bubble.cpp` 会识别这些 metadata，把 MCP tool result 渲染成工具信息块和可展开详情，而不是把执行信息裸露成普通文本或 JSON。

## 当前实现边界

已经完成：

- JSON 添加 MCP server。
- `stdio`、`streamable_http`、`sse` transport。
- `AIMCPService` 统一 discovery、状态和工具快照。
- Session 不再持有 discovery runner，也不会因多会话重复 discovery。
- 保存 MCP 设置和打开 Dock MCP 列表不阻塞主线程。
- AI Dock 可通过小按钮展开 MCP server 列表及状态。
- AI Settings MCP 列表前有状态色标。
- 初始化失败有非阻塞提示。
- MCP tool 去重后注册到 `AIToolRegistry`。
- MCP tool schema 通过 provider `tools` 字段进入模型请求。
- MCP tool 默认需要用户审批。

仍未实现：

- 持久化 MCP 健康状态。
- 后台周期性健康检查。
- server 级别自动重试、退避和 circuit breaker。
- 长连接或 session 复用。
- 跨启动的工具发现结果缓存。

后续如果要继续生产化，建议在 `AIMCPService` 上演进 health check、缓存、重试和状态输出，保持 `AIAgentSession` 只消费工具快照，transport client 只负责协议 I/O，UI 只读取 service 状态。
