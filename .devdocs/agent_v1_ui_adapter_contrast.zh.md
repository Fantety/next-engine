# Agent V1 UI 适配层对照文档

本文档用于准备 `ai_component/ui` 接入 `agent_v1`，重点是梳理现有 UI 操作、旧后端耦合点、`agent_v1` 可用接口，以及适配层需要输出给 UI 的 view model 和信号。

这不是 UI 接入实现文档，也不是把 `ai_component/ui` 继续包装到旧 `ai_component` Runtime/Session 体系。后续目标是：

```text
editor/ai_component/ui
  -> Agent V1 UI Adapter
    -> editor/agent_v1/session/service/AISessionService
    -> editor/agent_v1/domain/projection/AISessionProjector
    -> editor/agent_v1/domain/events/AIEventStore
    -> editor/agent_v1/config/AIConfigService
    -> editor/agent_v1/permission/AIPermissionService
    -> editor/agent_v1/mcp/AIV1MCPService
    -> editor/agent_v1/skills/AIV1SkillService
    -> editor/agent_v1/agents/AIAgentService
    -> editor/agent_v1/tools/AIV1ToolRegistry
```

相关文档：

- `.devdocs/opencode-agent-architecture/complete-system-implementation-plan.zh.md`：Phase 0-12 的系统实现边界。
- `.devdocs/agent_ui_migration_design.md`：更大范围的新 UI 迁移方案，偏向新增 `editor/agent_ui`。本文档更窄，只定义当前 `ai_component/ui` 复用时如何通过适配层对接 `agent_v1`。

## 结论

- 可以复用 `ai_component/ui` 的视觉控件、布局思路和消息字典协议。
- 不能复用 `AIAgentSession`、`AIModelSettings`、`AIMCPService`、`AISkillSettings`、`AIPlanManager`、`AIChangeSetStore` 等旧后端对象作为新链路目标。
- 适配层应作为 UI 和 `agent_v1` 之间唯一应用门面，使用 Godot 信号向 UI 推送 session、message、run state、permission、config、MCP、Skill 等变化。
- Phase 0-12 的闭环应继续在 `agent_v1` 内可验证；UI 只是适配层的消费者。
- 第一阶段不需要细粒度流式 UI delta，可以先通过 `messages_changed(session_id, messages)` 粗粒度刷新验证闭环。

## 当前 UI 操作清单

| UI 组件 | 已有操作/信号 | 旧依赖点 | Agent V1 接入策略 |
| --- | --- | --- | --- |
| `AIAgentDock` | 新建会话、删除会话、选择会话、发送、取消、显示 token、MCP/Skill 状态、权限确认 | 直接持有 `AIAgentSession`，监听旧 session 信号 | 改为持有 `AIAgentV1UIAdapter`，所有会话/运行/权限/状态通过 adapter |
| `AIComposer` | `send_requested(message, model_id, agent_profile_id, attachments)`、`agent_profile_selected(agent_profile_id)`、`cancel_requested` | `reload_models()` 读取旧 `AIModelSettings` | 保留信号；模型/agent 列表由 adapter 输入或后续重构为 adapter 拉取 |
| `AIMessageList` | `set_messages`、`add_message`、`update_message`、`remove_message`；自动分组 tool-like 消息 | 只依赖消息字典协议 | 直接消费 adapter 输出的 UI message 字典 |
| `AIMessageBubble` | 渲染 `role/content/metadata/created_at`，支持 tool、tool_group、MCP metadata、attachments | 只依赖消息字典协议和 Markdown 控件 | 直接复用；adapter 负责把 `AISessionMessage` 转换为兼容字典 |
| `AIPlanPanel` | 展示计划和任务状态 | 旧 `AIPlanManager` | 第一阶段 adapter 返回空 plan；后续从 `agent_v1` 事件/工具结果投影 |
| `AIChangeReviewPanel` | 展示、保留、回滚 change set | 旧 `AIChangeSetStore` | 第一阶段隐藏或空状态；后续从 `agent_v1` tool/change events 投影 |
| `AIAgentSettingsDialog` | Settings 顶层窗口、左侧导航、页面容器、统一保存、广播配置变更 | 直接创建旧 Settings pages，调用 `EditorSettings::save()`，发出旧配置变更信号 | 作为独立入口接 adapter/config adapter；页面只提交 view model，不直接写旧 settings |
| Settings pages | 模型、MCP、Skill、Rule、Marquee 设置 | 旧 settings singleton 和旧 MCP/Skill 服务 | 后续通过 config adapter 读写 `AIConfigService`，MCP/Skill 状态来自 V1 服务 |

## 现有 UI 消息协议

`AIAgentMessage::to_dict()` 已形成一个简单稳定的显示协议：

```text
{
  "role": "user|assistant|system|tool|context|error|tool_group",
  "content": "...",
  "metadata": {},
  "created_at": 0
}
```

`AIMessageList` 和 `AIMessageBubble` 已支持以下约定：

- `role == "tool"`：作为工具消息显示。
- `role == "tool_group"`：由 `AIMessageList` 内部把连续工具消息分组生成。
- `role == "assistant"` 且 `metadata.tool_calls` 非空：可显示 assistant 的工具调用摘要。
- `metadata.attachments`：渲染附件标签。
- MCP 工具 metadata 可包含 `tool_origin == "mcp"`、`mcp_server_id`、`mcp_server_name`、`mcp_tool_name`、`mcp_transport`、`mcp_agent_tool_name`。

因此 adapter 第一阶段应优先输出兼容该协议的 `Array<Dictionary>`，避免先改消息控件。

## Agent V1 可用接口

| 能力 | 当前接口 | 适配层用途 |
| --- | --- | --- |
| 会话创建 | `AISessionService::create(Dictionary)` / `AISessionStore::create_session(Dictionary)` | 新建 UI session，设置 `agent_id`、`location`、`title`、`metadata` |
| 会话列表 | `AISessionStore::list_sessions()` | 填充 Dock session selector |
| Prompt 提交 | `AISessionService::prompt(Dictionary)` | 接收 composer 输入，写入 durable inbox 并 wake |
| 取消运行 | `AISessionService::interrupt(Dictionary)` | Stop 按钮 |
| 权限回复 | `AISessionService::reply_permission(Dictionary)` / `AIPermissionService::reply(Dictionary)` | 权限弹窗 approve/reject |
| 消息投影 | `AISessionProjector::project_from_store()`、`get_messages()`、`get_messages_struct()` | 生成 UI message list |
| 事件订阅 | `AIEventStore::event_appended`、`durable_event_appended`、`live_event_appended` | 后续实时刷新；第一阶段可粗粒度刷新 |
| 运行状态 | `AISessionExecution::get_state()`，信号 `drain_requested/drain_settled/interrupt_requested` | 生成 run state view model |
| 配置 | `AIConfigService::get_config()`、`patch_config()`、`set_runtime_override()`，信号 `config_changed` | 模型、agent、MCP、Skill、Rule 配置 |
| 权限 pending | `AIPermissionService::get_pending_requests()`，信号 `permission_asked/permission_replied` | UI permission request |
| MCP | `AIV1MCPService::import_config()`、`refresh()`、`get_statuses()`、`get_status_summary()`、`list_resources()`、`render_prompt()` | MCP 状态、资源、prompt 入口 |
| Skill | `AIV1SkillService::import_config()`、`refresh()`、`list_manifests()`、`select()`、`read_resource()` | Skill 状态、选择、资源 |
| Agent | `AIAgentService::list()`、`resolve()`、`resolve_for_session()` | Composer agent/profile 列表和 session agent |
| 工具 | `AIV1ToolRegistry::materialize()`、`get_tool_names()` | 工具状态摘要，调试和权限展示 |

## Settings Dialog 对照

`AIAgentSettingsDialog` 不是普通页面，而是当前旧 UI 的第二个顶层入口。它现在负责：

- 维护 singleton：`AIAgentSettingsDialog::get_singleton()`。
- 构建左侧导航：`LLM`、`Marquee`、`MCP`、`Skill`、`Rules`。
- 创建旧页面：`AISettingsModelsPage`、`AISettingsNextMarqueePage`、`AISettingsMCPPage`、`AISettingsSkillsPage`、`AISettingsRulesPage`。
- 接收各页面 `settings_changed`，分别保存并广播：
  - `ai_settings_changed`
  - `ai_next_marquee_settings_changed`
  - `ai_mcp_settings_changed`
  - `ai_skill_settings_changed`
  - `ai_rule_settings_changed`
- 点击 OK 时调用 `EditorSettings::save()`，并一次性广播所有旧信号。

当前 Dock 会监听这些信号：

```text
ai_settings_changed -> reload models / refresh token usage
ai_next_marquee_settings_changed -> refresh request progress material
ai_mcp_settings_changed -> refresh MCP status
ai_skill_settings_changed -> refresh Skill status
```

接入 `agent_v1` 时应拆成两层：

| 旧职责 | 新职责 |
| --- | --- |
| Settings Dialog 直接创建旧 page | Settings Dialog 创建 UI page，但向 adapter/config adapter 注入配置接口 |
| Page 自己读写旧 settings | Page 只编辑 view model，保存交给 config adapter |
| Dialog 调 `EditorSettings::save()` | config adapter 调 `AIConfigService::patch_config()`；纯 UI 本地偏好才写 `EditorSettings` |
| Dialog 发旧 `ai_*_settings_changed` | adapter/config adapter 发 `config_changed(scope, config)`、`models_changed(models)`、`mcp_status_changed(...)`、`skill_status_changed(...)` |
| Dock 直接监听 Settings singleton | Dock 监听同一个 adapter；Settings 保存成功后通过 adapter 信号刷新 Dock |

建议新增一个配置侧门面，例如 `AIAgentV1UIConfigAdapter`，避免把所有 settings schema 操作塞进聊天 adapter。两个 adapter 可以共享同一组 `agent_v1` 服务实例：

```text
AIAgentDock
  -> AIAgentV1UIAdapter

AIAgentSettingsDialog
  -> AIAgentV1UIConfigAdapter
       -> AIConfigService
       -> AIV1MCPService
       -> AIV1SkillService
       -> AIAgentService
```

第一阶段可以先不改 Settings Dialog UI，但文档和测试应明确：后续接入时不能让 `AIAgentSettingsDialog` 继续作为旧 `AIModelSettings/AIMCPSettings/AISkillSettings/AIRuleSettings` 的保存入口。

## 推荐适配层边界

建议类名先使用 `AIAgentV1UIAdapter`。位置建议放在 `editor/agent_v1/ui_adapter` 或 `editor/agent_v1/ui` 下，而不是旧 `ai_component` 后端目录中。这样 adapter 可以依赖 `agent_v1` 服务，但不依赖具体 UI 控件。

适配层职责：

- 创建或接收 `agent_v1` 服务实例。
- 把 UI 命令转换为 `AISessionService`、`AIConfigService`、`AIPermissionService`、`AIV1MCPService`、`AIV1SkillService` 等调用。
- 把 `agent_v1` domain struct/event/config 转成 UI view model 字典。
- 通过 Godot 信号通知 UI 刷新。
- 隐藏 session wake、projector rebuild、permission pending、runtime override 等底层细节。

适配层不负责：

- 不持有输入框草稿、展开的工具详情、滚动位置等纯 UI 状态。
- 不直接执行工具，不绕过 `AIV1ToolRegistry` 和 `AIPermissionService`。
- 不把旧 `AIAgentSession` 当成后端。
- 不为 UI 临时需求改写 `agent_v1` domain source of truth。

## Adapter 命令对照

| UI 入口 | Adapter 命令 | Agent V1 调用 | 说明 |
| --- | --- | --- | --- |
| Dock 初始化 | `initialize()` | 组装 services，连接信号，加载 config | 可在测试中单独调用 |
| 新建会话 | `create_session(options)` | `AISessionService::create()` | `options.agent_id` 来自 Composer mode/profile |
| 选择会话 | `set_active_session(session_id)` | `AISessionStore::get_session()` + projector rebuild | 成功后发 `active_session_changed` 和 `messages_changed` |
| 发送按钮 | `send_message(text, model_id, agent_id, attachments)` | `AISessionService::prompt()` | 必须已有 session；无 session 时 adapter 可先 create |
| 停止按钮 | `cancel_active_run(reason)` | `AISessionService::interrupt()` | 发 `run_state_changed` |
| 删除会话 | `request_delete_session(session_id)` | 当前无 delete/archive API | 第一阶段返回 unsupported，UI 禁用或提示 |
| 权限确认 | `reply_permission(request_id, allowed, options)` | `AISessionService::reply_permission()` | `allowed` 映射为 `reply = "allow|deny"` |
| 刷新模型 | `list_models()` | `AIConfigService::get_config()` / `AIAgentService::list()` | 输出 Composer 可消费 model items |
| 刷新 MCP 状态 | `get_mcp_status()` | `AIV1MCPService::get_statuses()` / summary | 不再使用旧 `AIMCPService` |
| 刷新 Skill 状态 | `get_skill_status()` | `AIV1SkillService::list_manifests()` | 不再使用旧 `AISkillSettings` |
| 保存配置 | `patch_settings(patch)` | `AIConfigService::patch_config()` | Settings 后续接入时使用 |
| 打开 Settings | `get_settings_snapshot()` | `AIConfigService::get_config()` + V1 status/list APIs | Dialog 初始化页面 view model |
| Settings 页面保存 | `save_model_config/save_mcp_config/save_skill_config/save_rule_config/save_marquee_config` | `AIConfigService::patch_config()`，必要时 `AIV1MCPService::import_config()` / `AIV1SkillService::import_config()` | 替代旧 page 自行写 settings |

## Adapter 信号对照

| Adapter 信号 | UI 消费者 | 数据 |
| --- | --- | --- |
| `sessions_changed(sessions)` | Dock session selector | `Array<SessionViewModel>` |
| `active_session_changed(session)` | Dock selector/message list/composer | `SessionViewModel` |
| `messages_changed(session_id, messages)` | `AIMessageList::set_messages()` | `Array<UIMessage>` |
| `message_added(session_id, message)` | `AIMessageList::add_message()` | 可后续增量启用 |
| `message_updated(session_id, index, message)` | `AIMessageList::update_message()` | 可后续增量启用 |
| `message_removed(session_id, index)` | `AIMessageList::remove_message()` | 可后续增量启用 |
| `run_state_changed(state)` | Dock progress、Composer send/stop | `RunStateViewModel` |
| `token_usage_changed(usage)` | Dock token label | `TokenUsageViewModel` |
| `permission_requested(request)` | Dock permission dialog | `PermissionRequestViewModel` |
| `permission_resolved(reply)` | Dock permission dialog | `PermissionReplyViewModel` |
| `mcp_status_changed(statuses, summary)` | MCP status button/popup | V1 MCP status |
| `skill_status_changed(statuses, summary)` | Skill status button/popup | V1 skill status |
| `config_changed(scope, config)` | Dock/Composer/Settings | effective config |
| `models_changed(models)` | Composer/Settings Dialog | `Array<ModelViewModel>` |
| `rules_changed(rules)` | Settings Dialog | `Array<RuleViewModel>` |
| `marquee_changed(marquees, active_id)` | Settings Dialog/Dock progress | loading marquee view model |
| `error_reported(error)` | Dock toast/error message | normalized error |

第一阶段建议只强制实现：

- `sessions_changed`
- `active_session_changed`
- `messages_changed`
- `run_state_changed`
- `permission_requested`
- `permission_resolved`
- `error_reported`

## View Model 草案

### SessionViewModel

```text
{
  "id": "...",
  "title": "...",
  "agent_id": "main",
  "directory": "...",
  "workspace_id": "...",
  "created_at": 0,
  "updated_at": 0,
  "metadata": {}
}
```

来源：`AISessionRow::to_dictionary()`。

### UIMessage

```text
{
  "id": "...",
  "session_id": "...",
  "role": "user|assistant|system|context|tool|error",
  "content": "...",
  "created_at": 0,
  "metadata": {
    "seq": 0,
    "agent_v1_type": "...",
    "attachments": [],
    "tool_calls": []
  }
}
```

这是对当前 `AIMessageBubble` 最友好的格式。`id/session_id` 可放顶层，也可同时放入 `metadata`，以便后续增量更新定位。

### RunStateViewModel

```text
{
  "session_id": "...",
  "state": "idle|preparing|running|waiting_permission|cancelling|interrupted|failed",
  "busy": false,
  "can_send": true,
  "can_cancel": false,
  "status_text": "",
  "active_run_id": "",
  "wake_pending": false,
  "interrupted": false,
  "interrupt_reason": ""
}
```

来源：

- `AISessionExecution::get_state()`
- pending permission 数量
- 最近错误事件

### PermissionRequestViewModel

```text
{
  "request_id": "...",
  "session_id": "...",
  "action": "tool.execute",
  "resource": "...",
  "effect": "ask",
  "reason": "...",
  "source": {
    "tool_name": "...",
    "tool_origin": "builtin|mcp|skill|task",
    "mcp_server_id": "...",
    "skill_id": "..."
  }
}
```

来源：`AIPermissionService::get_pending_requests()` 和 `permission_asked` 信号。

### MCPStatusViewModel

```text
{
  "server_id": "...",
  "name": "...",
  "state": "running|stopped|disabled|failed",
  "tool_count": 0,
  "resource_count": 0,
  "prompt_count": 0,
  "last_error": ""
}
```

来源：`AIV1MCPService::get_statuses()`。

### SkillStatusViewModel

```text
{
  "skill_id": "...",
  "name": "...",
  "enabled": true,
  "source": "...",
  "tools_enabled": false,
  "metadata": {}
}
```

来源：`AIV1SkillService::list_manifests()` 和 `AIConfigService` 的 `skills` 配置。

### SettingsSnapshotViewModel

```text
{
  "models": [],
  "agents": [],
  "mcp_servers": [],
  "skills": [],
  "rules": [],
  "marquees": [],
  "active_marquee_id": "",
  "metadata": {
    "config_version": 1
  }
}
```

来源：

- `AIConfigService::get_config()`
- `AIAgentService::list()`
- `AIV1MCPService::get_statuses()`
- `AIV1SkillService::list_manifests()`

该 snapshot 是 Settings Dialog 的初始化输入。Settings pages 修改后提交 patch 给 config adapter，不再直接保存旧 singleton settings。

## 数据映射

### UI 发送到 Agent V1

`AIComposer::send_requested(message, model_id, agent_profile_id, attachments)` 应映射为 adapter 命令：

```text
send_message(text, model_id, agent_id, attachments)
```

Adapter 再组装 `AISessionService::prompt()` 输入：

```text
{
  "session_id": "...",
  "text": "...",
  "parts": [
    { "type": "text", "text": "..." },
    { "type": "attachment", "attachment_id": "..." }
  ],
  "attachments": [],
  "delivery": "steer",
  "resume": true,
  "metadata": {
    "source": "ai_component_ui",
    "ui_model_id": "...",
    "ui_agent_id": "..."
  }
}
```

注意：

- `AISessionService::prompt()` 当前要求已有 `session_id`；adapter 可在无 active session 时先 `create_session()`。
- 附件必须走 `agent_v1` `AIAttachmentResolver` / `AIAttachmentBlobStore`，不能把本地 path 直接交给 provider。
- `model_id` 第一阶段可通过 `AIConfigService::set_runtime_override()` 或 session 创建时 model metadata 实现；长期应补齐 session-scoped model selection 契约。
- `agent_profile_id` 应映射为 `agent_id`，并由 `AIAgentService` 解析。已有 session 中切换 agent 的行为需要明确：要么新建 session，要么提示当前 session 不支持切换。

### Agent V1 消息到 UI

| Agent V1 来源 | UI 输出 | 说明 |
| --- | --- | --- |
| `AISessionMessage.type == user` | `role = "user"`，`content = text` | `files/agents/references` 转 `metadata.attachments/references` |
| `assistant` text/reasoning content | `role = "assistant"`，`content = joined text` | reasoning 可进入 `metadata.reasoning` 或折叠详情 |
| `assistant` tool content | `role = "tool"` 或 assistant `metadata.tool_calls` | 第一阶段建议拆为 tool 消息，复用 tool_group |
| `system` | `role = "system"` 或隐藏 | 调试模式可显示 |
| `compaction` | `role = "context"` | 用于说明上下文压缩结果 |
| `step_failed` / provider error | `role = "error"` | `content` 是可读错误，原始 error 入 metadata |
| `permission_asked` | 不直接变消息，发 `permission_requested` | 如需审计，也可生成 tool/context 消息 |

工具消息 metadata 建议：

```text
{
  "tool_name": "...",
  "status": "pending|running|success|failed|denied",
  "input": {},
  "progress": {},
  "output": {},
  "output_paths": [],
  "error": {},
  "tool_origin": "builtin|mcp|skill|task",
  "mcp_server_id": "...",
  "mcp_server_name": "...",
  "mcp_tool_name": "...",
  "mcp_agent_tool_name": "...",
  "skill_id": "...",
  "registration_identity": {}
}
```

## 旧依赖替换表

| 当前旧依赖 | 不再使用的原因 | Agent V1 替代 |
| --- | --- | --- |
| `AIAgentSession` | 旧 Runtime/Session loop，不符合 Phase 0-12 admission/runner/event 架构 | `AISessionService` + `AISessionExecution` + `AISessionProjector` |
| `AIModelSettings` | 旧模型配置存储 | `AIConfigService` providers/default_model/agents |
| `AIMCPService` | 旧 MCP 链路 | `AIV1MCPService` + `AIV1ToolRegistry` |
| `AISkillSettings` | 旧 Skill 设置 | `AIV1SkillService` + `AIConfigService.skills` |
| `AIRuleSettings` | 旧 Rule 设置 | `AIConfigService.permissions.rules` + `AIPermissionService` |
| `AIPlanManager` | 旧 plan singleton | 后续由 `agent_v1` event/projection 或 task/tool result 投影 |
| `AIChangeSetStore` | 旧 change review store | 后续由 `agent_v1` tool/change events 投影 |
| `AIReferenceResolver` 旧路径 | 位于 UI 目录且带解析逻辑 | adapter/agent_v1 attachment resolver |
| `AIAgentSettingsDialog` 旧保存链路 | 顶层 Dialog 直接调用 `EditorSettings::save()` 并广播旧信号 | `AIAgentV1UIConfigAdapter` + `AIConfigService::patch_config()` + adapter signals |

## 阶段建议

### Step A：只实现 Adapter 骨架和测试

- 新增 adapter 类和 view model 转换函数。
- 测试 adapter 能创建 session、发送 prompt、从 projector 读取 user message。
- 测试不 include 旧 `AIAgentSession`。
- 测试 permission pending 能转换为 `PermissionRequestViewModel`。

### Step B：用 Fake/默认 runtime 验证闭环

- 不接 UI，直接在测试中调用 adapter。
- 验证 `send_message -> AISessionService::prompt -> event/projector -> messages_changed payload`。
- 验证 `cancel_active_run -> interrupt`。
- 验证 `reply_permission` 能唤醒 session。

### Step C：最小接 Dock

- `AIAgentDock` 只替换旧 `session` 成 adapter。
- Composer 信号仍由 Dock 接收，再转 adapter。
- MessageList 仍消费原消息字典。
- 删除会话、Plan、ChangeReview 可先禁用或空状态。

### Step D：接配置、MCP、Skill、Agent

- Composer 模型列表来自 `AIConfigService`。
- Agent 模式/Profile 来自 `AIAgentService::list()`。
- MCP 状态来自 `AIV1MCPService`。
- Skill 状态来自 `AIV1SkillService`。
- Settings 页后续再通过 config adapter 接入。

## 第一批测试清单

建议先补测试，再实现：

- `test_agent_v1_ui_adapter_create_session`
  - adapter `create_session()` 返回 `SessionViewModel`，并发出/可获取 session list。
- `test_agent_v1_ui_adapter_send_message_projects_user_message`
  - 先 create session，再 send text，断言 `messages_changed` payload 中有 `role == "user"`。
- `test_agent_v1_ui_adapter_rejects_prompt_without_session_or_creates_one_by_policy`
  - 明确无 active session 策略，避免 UI 层临时兜底。
- `test_agent_v1_ui_adapter_message_mapping_assistant_tool_content`
  - 构造/追加 tool events，断言输出 `role == "tool"` 且 metadata 兼容 `AIMessageBubble`。
- `test_agent_v1_ui_adapter_permission_request_mapping`
  - 通过 `AIPermissionService::assert_permission()` 产生 pending，断言 adapter 输出 permission request。
- `test_agent_v1_ui_adapter_cancel_active_run`
  - 调用 `cancel_active_run()` 后 run state 进入 interrupted/cancelling 可解释状态。
- `test_agent_v1_ui_adapter_does_not_include_old_ai_component_backend`
  - 可用边界检查或构建依赖检查，确保 adapter 不依赖 `AIAgentSession`、旧 MCP/Skill settings。

## 待确认缺口

- `agent_v1` 目前没有 session delete/archive API，UI 删除会话第一阶段应禁用或返回 unsupported。
- 模型选择当前更接近全局/runtime override，若要每个 session 或每次 prompt 独立选择模型，需要补 session-scoped model 契约。
- Plan 和 Change Review 在 `agent_v1` 中还需要稳定投影来源，第一阶段不要回连旧 singleton/store。
- 旧 `AIComposer::reload_models()` 直接读旧模型设置，接入时需要改成由 Dock/adapter 注入列表，或新增不依赖旧 settings 的加载入口。
- 权限弹窗目前是 Dock 直接处理旧 `tool_approval_requested` 字典；adapter 应定义新的 `PermissionRequestViewModel`，Dock 只负责展示和回复。
- MCP/Skill 设置页后续需要按 `AIConfigService` schema 重写保存逻辑，不能复用旧 settings 存储格式。

## 验收标准

- 不接 UI 时，adapter 测试可跑通 create session、send prompt、projection、permission reply、interrupt 的闭环。
- 接 UI 前，adapter 输出的 messages 可直接喂给 `AIMessageList::set_messages()` 渲染。
- 适配层没有旧 `AIAgentSession`、旧 `AIMCPService`、旧 settings singleton 依赖。
- UI 不直接访问 `AIEventStore`、`AISessionRunner`、`AIV1ToolRegistry` 等底层对象。
- 所有工具、MCP、Skill、Subagent 能力仍通过 Phase 0-12 的统一 `agent_v1` pipeline。
