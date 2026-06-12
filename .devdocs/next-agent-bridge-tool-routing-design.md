# Next Agent Bridge 工具调用链路与协议设计

本文档描述 NextEngine 使用 `rust-godot` 开发 Godot Editor 插件，并通过外部 Tauri Agent 客户端调用 Godot 能力时，工具应该如何注册、发现、调用、授权和审计。

## 背景

NextEngine 当前 AI 功能主要位于 `editor/ai_component`，普通模式和 NEXT 模式都依赖一组面向 Godot 编辑器的工具。随着 Agent UI 和多 Agent 编排复杂度上升，将 UI 与 Agent runtime 放在 Godot 编辑器内部会持续受限于 Godot `Control` UI 体系。

新的方向是：

- Godot 编辑器插件负责暴露可靠、可审计的引擎操作能力。
- Tauri 外部客户端负责 Agent UI、工具注册、权限确认和高阶编排。
- Agent 面向强类型工具工作，而不是直接面向一个任意命令通道。

## 设计结论

推荐采用“插件发布能力，客户端动态注册工具”的混合式方案。

```text
Agent
  sees strongly typed tools
        |
Tauri Client
  registers tools, validates schemas, handles permissions, composes workflows
        |
WebSocket JSON-RPC
        |
Godot Bridge Plugin
  publishes capabilities, validates requests, executes Godot operations
        |
NextEngine / Godot Editor
```

不要采用只有一个 `send_command` 或 `send_tool` 的黑箱工具。该方式虽然实现简单，但会削弱 schema、权限、审计、测试、回放和客户端 UI 能力。

也不建议把全部工具硬编码在客户端。客户端应消费插件发布的能力快照，动态注册基础工具，再在客户端侧组合高阶 Agent 工具。

## 职责边界

### Godot Bridge Plugin

插件是能力提供者和执行器。

职责：

- 启动本地 WebSocket/HTTP 服务。
- 对外发布工具列表、能力列表、协议版本。
- 校验 token、参数、权限。
- 将外部请求切回 Godot 主线程执行。
- 调用 `EditorInterface`、`EditorUndoRedoManager`、Debugger、Viewport 等 Godot API。
- 返回结构化结果。
- 推送运行日志、错误、截图完成、场景变化等事件。

插件不负责：

- Agent prompt 编排。
- 多轮模型调用。
- 复杂产品 UI。
- 高阶任务规划。

### Tauri Client

客户端是工具注册器、权限控制器和编排器。

职责：

- 连接插件，完成握手和认证。
- 调用 `bridge.list_tools` 获取插件工具 schema。
- 将插件工具注册成 Agent 可调用工具。
- 提供客户端自有的高阶工具。
- 做二次 schema 校验和权限确认 UI。
- 记录 tool call 历史、结果、截图和日志。
- 把工具结果喂回 Agent。

### Agent

Agent 只看到明确命名、明确 schema 的工具。

Agent 不应该直接操作协议细节，也不应该只看到一个万能 `bridge.call_tool`。

## 工具分层

### 插件层基础工具

插件层工具应低阶、确定、可审计。

示例：

```text
editor.get_context
editor.get_selection
editor.select_node
scene.get_tree
scene.get_node
scene.add_node
scene.delete_node
scene.rename_node
scene.move_node
scene.set_property
scene.call_method
resource.read_text
resource.write_text
runtime.play_current
runtime.play_main
runtime.play_custom
runtime.stop
runtime.status
viewport.capture_editor_2d
viewport.capture_editor_3d
runtime.capture_game
debug.get_recent
debug.subscribe
```

### 客户端层高阶工具

客户端层工具面向 Agent 工作流，可以组合多个插件层工具。

示例：

```text
godot.run_and_collect_errors
godot.capture_and_analyze_viewport
godot.fix_current_scene_errors
godot.create_basic_player_controller
godot.implement_feature_plan
godot.review_scene_changes
```

例如 `godot.run_and_collect_errors` 可以执行：

1. `debug.subscribe`
2. `runtime.play_current`
3. 等待指定时长或运行到首帧
4. `runtime.capture_game`
5. 收集 `event.debug.output` 和 `event.debug.error`
6. `runtime.stop`
7. 返回日志、错误和截图 asset id

## 启动与握手流程

1. Godot 插件启动。
2. 插件监听 `127.0.0.1:{port}`。
3. 插件生成随机 token。
4. 插件写入连接信息文件：

```json
{
  "protocol": "next-agent-bridge",
  "protocol_version": "1.0",
  "plugin_version": "0.1.0",
  "port": 17361,
  "token": "random-256-bit-token",
  "project_path": "F:/project/C++/next-engine"
}
```

建议路径：

```text
.godot/next_agent_bridge.json
```

5. Tauri 客户端读取连接信息或扫描本地端口。
6. 客户端连接：

```text
ws://127.0.0.1:{port}/rpc
```

7. 客户端发送 `bridge.hello`。
8. 插件返回协议版本、工程信息、能力摘要。
9. 客户端调用 `bridge.list_tools`。
10. 客户端将插件工具注册进 Agent runtime。

## JSON-RPC 消息格式

协议采用 WebSocket 承载 JSON-RPC 2.0 风格消息。HTTP 只用于 health check 和下载截图等大文件。

### 请求

```json
{
  "jsonrpc": "2.0",
  "id": "req_001",
  "method": "scene.set_property",
  "params": {
    "scene_id": "current",
    "node_path": "Player/Camera2D",
    "property": "zoom",
    "value": [1.2, 1.2]
  },
  "auth": {
    "token": "..."
  }
}
```

### 成功响应

```json
{
  "jsonrpc": "2.0",
  "id": "req_001",
  "result": {
    "ok": true,
    "changed": true,
    "old_value": [1.0, 1.0],
    "new_value": [1.2, 1.2]
  }
}
```

### 错误响应

```json
{
  "jsonrpc": "2.0",
  "id": "req_001",
  "error": {
    "code": "NODE_NOT_FOUND",
    "message": "Node path does not exist.",
    "details": {
      "node_path": "Player/Camera2D"
    }
  }
}
```

### 事件

事件没有 `id`，由插件主动推送。

```json
{
  "jsonrpc": "2.0",
  "method": "event.debug.output",
  "params": {
    "session_id": 1,
    "stream": "stdout",
    "text": "Player ready\n",
    "time": "2026-06-10T12:30:00+08:00"
  }
}
```

## 工具发现协议

### bridge.hello

请求：

```json
{
  "id": "req_hello",
  "method": "bridge.hello",
  "params": {
    "client_name": "Next Agent Tauri",
    "client_version": "0.1.0",
    "supported_protocols": ["1.0"]
  },
  "auth": {
    "token": "..."
  }
}
```

响应：

```json
{
  "id": "req_hello",
  "result": {
    "protocol_version": "1.0",
    "plugin_version": "0.1.0",
    "engine": "NextEngine",
    "godot_version": "4.x",
    "project_path": "F:/project/C++/next-engine",
    "capabilities": {
      "scene_edit": true,
      "file_edit": true,
      "runtime_control": true,
      "editor_viewport_capture": true,
      "game_capture": "experimental",
      "debug_events": true,
      "undo_redo": true
    }
  }
}
```

### bridge.list_tools

插件返回基础工具列表。每个工具必须包含稳定名称、描述、输入 schema、权限级别和危险等级。

请求：

```json
{
  "id": "req_tools",
  "method": "bridge.list_tools",
  "params": {
    "include_experimental": true
  },
  "auth": {
    "token": "..."
  }
}
```

响应：

```json
{
  "id": "req_tools",
  "result": {
    "tools": [
      {
        "name": "scene.set_property",
        "title": "Set Node Property",
        "description": "Set a property on a node in the current edited scene.",
        "permission": "edit_scene",
        "danger": "medium",
        "undoable": true,
        "input_schema": {
          "type": "object",
          "properties": {
            "scene_id": {
              "type": "string",
              "default": "current"
            },
            "node_path": {
              "type": "string"
            },
            "property": {
              "type": "string"
            },
            "value": {}
          },
          "required": ["node_path", "property", "value"]
        }
      },
      {
        "name": "runtime.play_current",
        "title": "Run Current Scene",
        "description": "Run the currently edited scene.",
        "permission": "runtime_control",
        "danger": "low",
        "undoable": false,
        "input_schema": {
          "type": "object",
          "properties": {}
        }
      }
    ]
  }
}
```

### bridge.call_tool

`bridge.call_tool` 是协议内部通用调用入口。客户端可以使用它调用插件工具，但 Agent 不应只看到这个工具。

请求：

```json
{
  "id": "req_call_001",
  "method": "bridge.call_tool",
  "params": {
    "tool": "scene.set_property",
    "arguments": {
      "scene_id": "current",
      "node_path": "Player/Camera2D",
      "property": "zoom",
      "value": [1.2, 1.2]
    },
    "call_context": {
      "agent_session_id": "session_123",
      "tool_call_id": "toolu_456",
      "reason": "Adjust camera zoom for current scene."
    }
  },
  "auth": {
    "token": "..."
  }
}
```

响应：

```json
{
  "id": "req_call_001",
  "result": {
    "tool": "scene.set_property",
    "ok": true,
    "summary": "Updated Player/Camera2D.zoom.",
    "data": {
      "old_value": [1.0, 1.0],
      "new_value": [1.2, 1.2]
    },
    "audit_id": "audit_789"
  }
}
```

## Agent 工具调用完整链路

1. 插件启动并发布工具列表。
2. Tauri 客户端连接插件并拉取 `bridge.list_tools`。
3. 客户端将每个插件工具转换为 Agent runtime 可识别的工具 schema。
4. 客户端额外注册自己的高阶工具。
5. Agent 发起一次模型请求，工具列表进入模型上下文。
6. 模型选择某个工具，例如 `scene.set_property`。
7. 客户端收到 tool call。
8. 客户端根据本地权限策略判断：
   - 允许：直接调用插件。
   - 询问：弹出确认 UI。
   - 拒绝：返回 denied tool result 给 Agent。
9. 客户端调用 `bridge.call_tool`。
10. 插件验证 token、工具名、schema 和权限。
11. 插件将操作派发到 Godot 主线程。
12. 插件执行 Godot API，必要时走 UndoRedo。
13. 插件返回结构化结果。
14. 客户端记录审计日志。
15. 客户端将结果作为 tool result 喂回 Agent。
16. Agent 进入下一轮推理。

## 权限模型

权限分为插件侧硬权限和客户端侧交互权限。

插件侧负责最低安全边界：

```text
read_only
edit_scene
edit_files
runtime_control
debug_control
dangerous
```

客户端侧负责用户体验：

```text
allow
ask
deny
```

建议默认策略：

```text
read_only: allow
runtime_control: ask or allow by profile
edit_scene: ask in review mode, allow in write mode
edit_files: ask
dangerous: ask always
```

危险操作包括：

- 删除节点。
- 覆盖文件。
- 批量移动节点。
- 关闭未保存场景。
- 停止正在运行的游戏。
- 执行项目外路径操作。

## 审计记录

每次工具调用都应生成 audit record。

```json
{
  "audit_id": "audit_789",
  "time": "2026-06-10T12:30:00+08:00",
  "client": "Next Agent Tauri",
  "agent_session_id": "session_123",
  "tool_call_id": "toolu_456",
  "tool": "scene.set_property",
  "arguments": {
    "node_path": "Player/Camera2D",
    "property": "zoom"
  },
  "permission": "edit_scene",
  "decision": "allow",
  "ok": true,
  "summary": "Updated Player/Camera2D.zoom."
}
```

审计记录至少由客户端保存。插件可保存最近 N 条，供 `bridge.get_recent_audit` 查询。

## 事件订阅协议

客户端通过 `bridge.subscribe_events` 订阅事件。

请求：

```json
{
  "id": "req_sub_001",
  "method": "bridge.subscribe_events",
  "params": {
    "events": [
      "event.runtime.started",
      "event.runtime.stopped",
      "event.debug.output",
      "event.debug.error",
      "event.scene.changed",
      "event.asset.created"
    ]
  },
  "auth": {
    "token": "..."
  }
}
```

响应：

```json
{
  "id": "req_sub_001",
  "result": {
    "ok": true,
    "subscription_id": "sub_001"
  }
}
```

常用事件：

```text
event.runtime.started
event.runtime.stopped
event.runtime.status_changed
event.debug.output
event.debug.error
event.debug.warning
event.debug.breaked
event.scene.opened
event.scene.saved
event.scene.changed
event.selection.changed
event.viewport.capture_ready
event.bridge.tools_changed
```

当插件能力发生变化时，发送：

```json
{
  "jsonrpc": "2.0",
  "method": "event.bridge.tools_changed",
  "params": {
    "reason": "plugin_reloaded",
    "tool_version": 3
  }
}
```

客户端收到后重新调用 `bridge.list_tools`。

## 截图与大文件传输

截图不应默认通过 WebSocket 返回 base64。插件应创建临时 asset，并返回 asset id 和 HTTP URL。

请求：

```json
{
  "id": "req_shot_001",
  "method": "viewport.capture_editor_3d",
  "params": {
    "viewport_index": 0,
    "format": "png",
    "max_width": 1600
  },
  "auth": {
    "token": "..."
  }
}
```

响应：

```json
{
  "id": "req_shot_001",
  "result": {
    "asset_id": "shot_20260610_001",
    "mime": "image/png",
    "width": 1600,
    "height": 900,
    "url": "http://127.0.0.1:17361/assets/shot_20260610_001"
  }
}
```

HTTP asset 下载也必须校验 token，例如通过 header：

```text
Authorization: Bearer <token>
```

或短期 signed URL。

## 错误码

统一错误码：

```text
UNAUTHORIZED
FORBIDDEN
INVALID_PARAMS
METHOD_NOT_FOUND
TOOL_NOT_FOUND
PROJECT_NOT_READY
SCENE_NOT_OPEN
NODE_NOT_FOUND
RESOURCE_NOT_FOUND
PROPERTY_NOT_FOUND
OPERATION_FAILED
RUNTIME_NOT_PLAYING
SCREENSHOT_FAILED
DEBUG_SESSION_NOT_FOUND
TIMEOUT
INTERNAL_ERROR
```

客户端应将错误转换为 Agent 可读的 tool result，而不是直接中断整个 Agent 会话。

## 为什么不只暴露一个万能工具

单一 `send_command` 或 `send_tool` 的问题：

- Agent 看不到准确的参数 schema。
- 客户端无法提前判断危险等级。
- 权限确认只能做粗粒度控制。
- 工具调用历史难以审计。
- UI 无法提供工具面板、参数预览、调用回放。
- 自动测试困难。
- 插件容易演变成另一个 Agent runtime。

可以保留 `bridge.call_tool` 作为协议内部入口，但 Agent 层必须看到强类型工具。

## 为什么不把所有工具硬编码在客户端

纯客户端工具的问题：

- 插件能力变化后客户端容易过期。
- 不同 Godot/NextEngine 版本难以兼容。
- 实验能力无法按项目动态启用。
- 插件无法声明自己是否支持运行截图、debug events、UndoRedo 等能力。

因此基础工具应由插件发布，客户端消费并注册。

## rust-godot 实现注意事项

rust-godot 插件需要注意：

- Editor API 调用必须发生在 Godot 主线程。
- 网络服务线程不能直接修改 SceneTree。
- 可用 channel 将 RPC 请求投递到主线程，在 `_process` 或 deferred callable 中执行。
- 插件工具 schema 应由 Rust 结构集中定义，避免文档和实现漂移。
- 修改场景的工具优先使用 Editor UndoRedo。
- 文件写入限制在项目目录内。
- 运行时输出优先通过 Debugger 扩展点捕获，不要读取 Output 面板文本。

如果 rust-godot 对某些 EditorDebugger 扩展点覆盖不足，可以在 NextEngine C++ 侧加一层很薄的 adapter，再把事件转发给 Rust 插件。

## MVP 范围

第一阶段实现：

```text
bridge.hello
bridge.list_tools
bridge.call_tool
bridge.subscribe_events
editor.get_context
scene.get_tree
scene.get_node
scene.set_property
runtime.play_current
runtime.stop
runtime.status
viewport.capture_editor_2d
viewport.capture_editor_3d
debug.get_recent
```

第二阶段实现：

```text
scene.add_node
scene.delete_node
scene.rename_node
scene.move_node
resource.read_text
resource.write_text
runtime.capture_game
debug.subscribe live output/error events
audit records
permission confirmation integration
```

第三阶段实现：

```text
NEXT workflow specific tools
batch transactions
operation replay
multi-agent session metadata
long-running operation progress
asset registry integration
```

## 最终原则

插件是能力网关，客户端是 Agent 编排层，Agent 看到的是强类型工具。

这条边界能同时保证：

- Godot 操作可靠。
- 客户端 UI 现代化。
- 工具 schema 可发现。
- 权限和审计可控。
- 后续可以接入更多客户端，例如 CLI、MCP server 或测试 runner。
