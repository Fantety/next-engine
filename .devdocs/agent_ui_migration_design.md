# Agent UI 迁移设计

本文档描述从旧 `editor/ai_component/ui` 迁移到新 `editor/agent_ui` 的设计方案。目标不是移动旧文件，而是在保留旧 `ai_component` 结构的前提下，为 `agent_v1` 复刻一套同等编辑器 UI，并通过适配层隔离 UI 与 Agent 后端服务。

## 背景

当前旧 AI Agent UI 位于：

```text
editor/ai_component/ui
```

该目录内既包含纯 UI 控件，也包含直接访问旧后端服务的逻辑。例如：

- `AIAgentDock` 直接持有 `AIAgentSession`。
- `AIComposer` 直接读取 `AIModelSettings` 和引用解析逻辑。
- 设置页直接读写 `AIModelSettings`、`AIMCPSettings`、`AISkillSettings`、`AIRuleSettings`。
- MCP 页面和 Dock 直接监听 `AIMCPService`。
- `AIChangeReviewPanel` 直接访问 `AIChangeSetStore`。
- `AINextMarqueeSettings` 这类设置/存储逻辑放在 UI 目录中。

这些耦合在旧 `ai_component` 中可以继续保留，但不应进入 `agent_v1` 的新 UI。`agent_v1` 已经有独立的 domain、session、config、permission、runtime、tools 等基础设施，新 UI 应围绕这些服务建立清晰边界。

旧 UI 职责核对清单：

- Dock 主界面、session 选择、新建、删除、状态按钮、进度条、token 用量。
- Composer 输入、模型选择、Agent 模式选择、发送/停止。
- 消息列表、消息气泡、Markdown 渲染、工具调用详情。
- Plan 面板，展示当前计划和任务状态。
- 附件栏、引用 token 高亮、引用解析。
- 变更审查、文本 diff 查看器。
- 设置窗口、模型/MCP/Skill/Rule/Loading marquee 页面。
- 模型、MCP server、Skill、Requirement form 等编辑弹窗。
- 空状态/placeholder 页面和测试辅助接口。

## 目标

新增一套 `agent_v1` 专用编辑器 UI：

```text
editor/agent_ui
```

核心目标：

- 保留旧 `editor/ai_component`，不移动、不删除、不依赖其旧后端。
- 在 `editor/agent_ui` 中复刻旧 UI 的主要交互结构和视觉体验。
- 为 `agent_v1` 实现 UI 适配层，UI 只依赖适配层暴露的 view model、命令和信号。
- 适配层负责把 UI 操作翻译成 `agent_v1` 的 session/config/permission/runtime/domain 调用。
- 新 UI 不直接 include `editor/ai_component/...`。
- 新 UI 不直接操作 event store、runner、provider、permission、tool registry 等底层服务。
- 支持旧 UI 与新 UI 并存，便于对照、灰度和回退。

## 非目标

本迁移不包含：

- 删除旧 `editor/ai_component/ui`。
- 迁移旧 `AIAgentSession`、`AIModelSettings`、`AIMCPSettings` 等后端实现。
- 让新 UI 继续包装旧 `ai_component` 后端。
- 一次性实现所有 MCP、Skill、Rule、Diff、权限和工具能力的最终形态。
- 改写 `agent_v1` domain model 以迎合 UI 临时状态。

## 总体架构

新结构以 `agent_ui` 为 UI 应用层，以 `agent_v1` 为业务和运行时层：

```text
editor/agent_ui
  -> adapter facade
    -> editor/agent_v1/session/service/AISessionService
    -> editor/agent_v1/config/AIConfigService
    -> editor/agent_v1/permission/AIPermissionService
    -> editor/agent_v1/domain/projection/AISessionProjector
    -> editor/agent_v1/domain/events/AIEventStore
    -> editor/agent_v1/runtime/AILLMRuntimeRegistry
    -> editor/agent_v1/tools/AIV1ToolRegistry
```

UI 层职责：

- 构建 Godot 编辑器控件。
- 管理控件布局、选中项、展开/折叠、按钮状态和提示文本。
- 将用户操作转发给适配层。
- 监听适配层信号并刷新显示。
- 维护纯 UI 临时状态，例如输入草稿、当前展开的工具详情、当前选中的设置页。

适配层职责：

- 组装并持有 `agent_v1` 服务实例。
- 暴露 UI 友好的 session list、message list、model list、pending permission、token usage、运行状态。
- 将 UI 命令转换为 `AISessionService::create()`、`prompt()`、`interrupt()`、`reply_permission()` 等调用。
- 从 projector/event/session service 中同步状态，发出 UI 可消费的信号。
- 负责把 `agent_v1` 的领域对象转换为 UI view model。
- 隐藏底层服务的生命周期、错误格式、重试策略和投影刷新细节。

`agent_v1` 职责：

- 保持 provider-neutral domain/event/projection 契约。
- 处理 session 创建、prompt admission、promotion、runner、permission、runtime 和工具执行。
- 不保存 UI 草稿状态。
- 不直接依赖 UI 控件。

## 目录设计

建议新增：

```text
editor/agent_ui
  SCsub
  adapter
    SCsub
    agent_ui_adapter.h
    agent_ui_adapter.cpp
    agent_ui_types.h
    agent_ui_types.cpp
    agent_ui_session_presenter.h
    agent_ui_session_presenter.cpp
    agent_ui_config_adapter.h
    agent_ui_config_adapter.cpp
    agent_ui_marquee_config_adapter.h
    agent_ui_marquee_config_adapter.cpp
    agent_ui_attachment_adapter.h
    agent_ui_attachment_adapter.cpp
    agent_ui_plan_presenter.h
    agent_ui_plan_presenter.cpp
  dock
    SCsub
    agent_ui_dock.h
    agent_ui_dock.cpp
  widgets
    SCsub
    agent_ui_composer.h
    agent_ui_composer.cpp
    agent_ui_message_list.h
    agent_ui_message_list.cpp
    agent_ui_message_bubble.h
    agent_ui_message_bubble.cpp
    agent_ui_markdown_label.h
    agent_ui_markdown_label.cpp
    agent_ui_markdown_renderer.h
    agent_ui_markdown_renderer.cpp
    agent_ui_attachment_bar.h
    agent_ui_attachment_bar.cpp
    agent_ui_reference_text_edit.h
    agent_ui_reference_text_edit.cpp
    agent_ui_text_diff_viewer.h
    agent_ui_text_diff_viewer.cpp
    agent_ui_plan_panel.h
    agent_ui_plan_panel.cpp
  dialogs
    SCsub
    agent_ui_settings_dialog.h
    agent_ui_settings_dialog.cpp
    agent_ui_model_profile_dialog.h
    agent_ui_model_profile_dialog.cpp
    agent_ui_mcp_server_dialog.h
    agent_ui_mcp_server_dialog.cpp
    agent_ui_skill_dialog.h
    agent_ui_skill_dialog.cpp
    agent_ui_requirement_form_dialog.h
    agent_ui_requirement_form_dialog.cpp
  settings
    SCsub
    agent_ui_settings_models_page.h
    agent_ui_settings_models_page.cpp
    agent_ui_settings_mcp_page.h
    agent_ui_settings_mcp_page.cpp
    agent_ui_settings_skills_page.h
    agent_ui_settings_skills_page.cpp
    agent_ui_settings_rules_page.h
    agent_ui_settings_rules_page.cpp
    agent_ui_settings_marquee_page.h
    agent_ui_settings_marquee_page.cpp
    agent_ui_settings_placeholder_page.h
    agent_ui_settings_placeholder_page.cpp
  review
    SCsub
    agent_ui_change_review_panel.h
    agent_ui_change_review_panel.cpp
```

命名建议：

- 新类统一使用 `AgentUI` 或 `AIAgentUI` 前缀，避免与旧 `AIAgentDock`、`AIComposer`、`AISettingsModelsPage` 等 ClassDB 名称冲突。
- 示例：`AIAgentUIDock`、`AIAgentUIComposer`、`AIAgentUIAdapter`。
- 文件名使用 `agent_ui_*.h/.cpp`，与目录名保持一致。

## 构建入口

新增：

```python
# editor/agent_ui/SCsub
SConscript("adapter/SCsub")
SConscript("dock/SCsub")
SConscript("widgets/SCsub")
SConscript("dialogs/SCsub")
SConscript("settings/SCsub")
SConscript("review/SCsub")
```

在 `editor/SCsub` 中添加：

```python
SConscript("agent_ui/SCsub")
```

构建顺序应放在 `agent_v1` 之后：

```python
SConscript("agent_v1/core/SCsub")
SConscript("agent_v1/config/SCsub")
SConscript("agent_v1/domain/SCsub")
SConscript("agent_v1/permission/SCsub")
SConscript("agent_v1/tools/SCsub")
SConscript("agent_v1/runtime/SCsub")
SConscript("agent_v1/session/SCsub")
SConscript("agent_ui/SCsub")
```

## 编辑器入口

旧入口继续存在：

- `AIAgentSettingsDialog`
- `AIAgentDock`

新入口建议新增：

- `AIAgentUISettingsDialog`
- `AIAgentUIDock`

初期可以通过编译开关或 EditorSettings 控制启用哪个 Dock：

```text
editor/agent_ui/enabled
```

迁移初期推荐并存策略：

- 旧 Dock 保持默认。
- 新 Dock 可以先使用独立菜单项或实验开关打开。
- 新 UI 功能完整后，再切换默认入口。
- 旧 Dock 不再新增功能，只保留维护。

入口改动核对：

- `editor/register_editor_types.cpp`
  - include 新 `editor/agent_ui/...` 头文件。
  - 注册需要进入 ClassDB 的 UI 类。
  - `agent_v1` 服务类已在旧文件中注册，新 UI adapter 只有需要脚本/测试/ObjectDB 访问时才注册。
- `editor/editor_node.h`
  - 前向声明 `AIAgentUIDock`、`AIAgentUISettingsDialog`。
  - 增加新 Dock 和新 Settings 指针，或用实验开关二选一持有。
- `editor/editor_node.cpp`
  - 根据 `editor/agent_ui/enabled` 创建旧 `AIAgentDock` 或新 `AIAgentUIDock`。
  - AI 设置按钮应打开与当前 Dock 匹配的 settings dialog。
  - 默认 Dock layout key 不应与旧 Dock 冲突，除非进入 Phase 6 切换默认入口。
- EditorSettings
  - 实验期开关默认关闭。
  - 切换默认入口前需要明确旧设置和新配置的迁移策略。

## 组件映射

旧 UI 与新 UI 的映射：

| 旧组件 | 新组件 | 迁移策略 |
| --- | --- | --- |
| `AIAgentDock` | `AIAgentUIDock` | 复刻布局和交互；后端调用全部改为 `AIAgentUIAdapter` |
| `AIComposer` | `AIAgentUIComposer` | 复刻输入、模型选择、模式选择、发送/停止；模型列表来自适配层 |
| `AIMessageList` | `AIAgentUIMessageList` | 可复用显示逻辑思想；消息数据改为 view model |
| `AIMessageBubble` | `AIAgentUIMessageBubble` | 复刻消息气泡、工具详情、错误展示 |
| `AIMarkdownLabel` | `AIAgentUIMarkdownLabel` | 可复刻 Markdown 展示，不依赖旧后端 |
| `AIPlanPanel` | `AIAgentUIPlanPanel` | 不接旧 `AIPlanManager`；由 adapter 输出 plan view model |
| `AIAttachmentBar` | `AIAgentUIAttachmentBar` | 只管理 UI 草稿附件；提交时交给 attachment adapter |
| `AIReferenceTextEdit` | `AIAgentUIReferenceTextEdit` | 只做引用 token 高亮和输入体验 |
| `AIReferenceResolver` | `AIAgentUIAttachmentAdapter` | 不放 widgets；负责把草稿引用转换为 `agent_v1` prompt input |
| `AIChangeReviewPanel` | `AIAgentUIChangeReviewPanel` | 第一阶段可隐藏或显示空状态；后续接入 `agent_v1` 工具变更事件 |
| `AITextDiffViewer` | `AIAgentUITextDiffViewer` | 可复刻纯 UI diff 查看器；数据来自 change review view model |
| `AIAgentSettingsDialog` | `AIAgentUISettingsDialog` | 仅负责导航和保存动作转发 |
| `AISettingsModelsPage` | `AIAgentUISettingsModelsPage` | 通过 config adapter 读写模型配置 |
| `AIModelProfileDialog` | `AIAgentUIModelProfileDialog` | 只收集表单；profile schema 由 config adapter 转换 |
| `AISettingsMCPPage` | `AIAgentUISettingsMCPPage` | 后续接入 `agent_v1` MCP/config/tool source |
| `AIMCPServerDialog` | `AIAgentUIMCPServerDialog` | 只编辑 server view model；不调用旧 MCP settings |
| `AISettingsSkillsPage` | `AIAgentUISettingsSkillsPage` | 后续接入 `agent_v1` config/context source |
| `AISkillDialog` | `AIAgentUISkillDialog` | 只编辑 skill view model；不调用旧 skill settings |
| `AISettingsRulesPage` | `AIAgentUISettingsRulesPage` | 后续接入 `AIConfigService` 或 permission/config rules |
| `AISettingsNextMarqueePage` | `AIAgentUISettingsMarqueePage` | 复刻 loading marquee 配置 UI；存储通过 config adapter |
| `AISettingsPlaceholderPage` | `AIAgentUISettingsPlaceholderPage` | 保留通用空页面，避免未完成页面硬编码在 dialog 中 |
| `AIRequirementFormDialog` | `AIAgentUIRequirementFormDialog` | 后续用于 requirement/permission 类交互；不直接执行工具 |
| `AINextMarqueeSettings` | `AIAgentUIMarqueeConfigAdapter` | 设置存储放适配/配置层，不放 widgets |

## 适配层设计

### AIAgentUIAdapter

`AIAgentUIAdapter` 是 UI 唯一直接调用的应用门面。

建议继承：

```cpp
class AIAgentUIAdapter : public RefCounted
```

它持有：

- `Ref<AISessionService>`
- `Ref<AISessionStore>`
- `Ref<AIEventStore>`
- `Ref<AISessionProjector>`
- `Ref<AIConfigService>`
- `Ref<AIPermissionService>`
- `Ref<AILLMRuntimeRegistry>`
- `Ref<AIV1ToolRegistry>`

生命周期建议：

- `AIAgentUIDock` 创建并持有一个 `Ref<AIAgentUIAdapter>`。
- `AIAgentUISettingsDialog` 可接收同一个 adapter，或通过明确的 editor-level owner 注入。
- 不建议让 UI 子控件自行创建 `AISessionService`、`AIConfigService` 等服务。
- 不建议在 Phase 0/1 引入新的全局 singleton；如果后续需要跨 Dock/Settings 共享，应创建明确的 `AIAgentUIServiceHub` 或由 `EditorNode` 持有。
- adapter 析构或 Dock 退出时应停止/interrupt 正在运行的 session execution，避免后台任务持有已销毁 UI 回调。

它发出 UI 信号：

```text
sessions_changed
active_session_changed(session_id)
messages_changed(session_id)
message_added(session_id, message)
message_updated(session_id, message_id, message)
run_state_changed(state)
token_usage_changed(usage)
permission_requested(request)
permission_resolved(request_id)
config_changed(scope)
error_reported(error)
```

第一阶段可以不做细粒度增量更新，先用 `messages_changed` 重刷消息列表。后续再优化到 `message_added/message_updated`。

### UI 命令接口

建议第一阶段暴露：

```cpp
Array list_sessions() const;
Dictionary get_active_session() const;
String get_active_session_id() const;
bool set_active_session(const String &p_session_id);
Dictionary create_session(const Dictionary &p_options = Dictionary());
bool can_delete_sessions() const;
Dictionary request_delete_session(const String &p_session_id);

Array get_messages(const String &p_session_id) const;
Dictionary get_run_state() const;
Dictionary get_token_usage(const String &p_session_id) const;

Dictionary send_prompt(const Dictionary &p_prompt);
Dictionary cancel_active_run();

Array get_pending_permissions() const;
Dictionary reply_permission(const String &p_request_id, bool p_allowed, const Dictionary &p_options = Dictionary());

Array list_models() const;
Dictionary get_model_config(const String &p_model_id) const;
Dictionary save_model_config(const Dictionary &p_model);
bool remove_model_config(const String &p_model_id);

Dictionary get_settings_snapshot() const;
Dictionary patch_settings(const Dictionary &p_patch);
```

UI 只理解这些接口，不知道底层是 event、projector、runner 还是 config service。

当前 `agent_v1` 现状需要注意：

- `AISessionService` 已有 `create()`、`prompt()`、`reply_permission()`、`interrupt()`、`promote_eligible()`。
- `AISessionStore` 已有 `create_session()`、`get_session()`、`list_sessions()`。
- `AISessionStore` 当前没有 delete/archive API，所以新 UI 的删除会话按钮第一阶段应禁用，或由 `request_delete_session()` 返回明确 unsupported 结果。
- 如果要恢复旧 UI 的删除会话体验，应先在 `agent_v1/session/service` 增加 archive/delete 契约，并定义 event/projection 如何处理被删除会话。
- `AIEventStore` 已有 `event_appended`、`durable_event_appended`、`live_event_appended` 信号，适配层可以订阅这些信号做 UI 刷新。

### Prompt 输入格式

`send_prompt()` 的输入由 UI adapter 定义，避免 UI 直接拼 `AISessionService::prompt()` 的内部字段：

```text
{
  "session_id": "...",
  "text": "...",
  "agent_id": "ask|plan|write",
  "model_id": "...",
  "attachments": [
    {
      "kind": "file",
      "path": "res://...",
      "label": "..."
    }
  ],
  "metadata": {
    "source": "agent_ui"
  }
}
```

适配层负责转换为 `AISessionService::prompt()` 需要的输入：

- session resolution
- root/location
- prompt parts
- attachment resolver input
- model/provider config
- agent id
- metadata

### 模型和 Agent 选择

旧 `AIComposer` 可以在每次发送时选择模型和模式。`agent_v1` 当前运行时行为需要单独处理：

- Session 的 agent 来自 `AISessionRow::agent_id`，创建 session 时可通过 `agent_id` 设置；为空时 runner 使用 `main`。
- Runner 当前从 `AIConfigService::get_default_provider()` 和 `get_provider_config(provider)["model"]` 取得 provider/model。
- `AISessionService::prompt()` 会保存 prompt metadata，但 runner 不直接把 UI 的 `model_id` 当成本轮 provider/model。

因此第一阶段建议：

- Composer 的 agent/mode 选择在创建 session 时写入 `agent_id`；如果用户在已有 session 中切换 agent，需要 adapter 明确创建新 session 或返回“当前 session 不支持切换 agent”。
- Composer 的 model 选择由 adapter 转成 `AIConfigService::patch_config(..., "runtime")` 的 runtime override，再唤醒/运行 session。
- adapter 在发送后保留本次 runtime override，直到运行完成；如果多个 session 并发运行，需要先限制为单 active run，或为 `agent_v1` 扩展 session-scoped model selection。
- 长期方案应把 session 级模型选择持久化进 `agent_v1` session/config 契约，而不是依赖全局 runtime override。

### Message View Model

UI 不直接展示 `agent_v1` domain struct。适配层输出统一消息 view model：

```text
{
  "id": "...",
  "session_id": "...",
  "role": "user|assistant|system|tool|error",
  "text": "...",
  "state": "complete|streaming|pending|failed",
  "time_created": 0,
  "metadata": {},
  "parts": [],
  "tool_calls": [
    {
      "id": "...",
      "name": "...",
      "status": "pending|running|approved|denied|complete|failed",
      "input": {},
      "output": {},
      "error": ""
    }
  ]
}
```

这样 `AIAgentUIMessageBubble` 可以稳定渲染，不需要理解 `AISessionMessage`、`AIToolState`、provider stream event 等后端细节。

### Run State View Model

适配层输出：

```text
{
  "state": "idle|preparing|running|waiting_permission|cancelling|failed",
  "busy": false,
  "can_send": true,
  "can_cancel": false,
  "status_text": "",
  "active_session_id": ""
}
```

Dock 和 Composer 只根据这个状态启用/禁用按钮。

## 会话流程

### 创建会话

UI：

1. 用户点击新建会话。
2. `AIAgentUIDock` 调用 `adapter->create_session()`。
3. adapter 调用 `AISessionService::create()` 或 session store 创建 session。
4. adapter 刷新 session list，发出 `sessions_changed` 和 `active_session_changed`。
5. Dock 选择新会话，消息列表清空。

### 删除或归档会话

旧 UI 支持删除当前会话，但 `agent_v1` 当前只有 session 创建、读取和列表能力，尚未定义删除或归档语义。

第一阶段：

1. Dock 可以保留删除按钮位置。
2. adapter 通过 `can_delete_sessions()` 返回 `false`。
3. UI 禁用按钮或显示“暂不支持删除”的提示。

后续补齐时需要先在 `agent_v1` 增加：

- `AISessionStore::delete_session()` 或 `archive_session()`。
- 对应的 durable event 或 session snapshot 更新策略。
- projector 和 session list 对 archived/deleted 状态的过滤规则。
- 测试覆盖删除当前会话、删除非当前会话、删除运行中会话的行为。

### 发送 Prompt

UI：

1. `AIAgentUIComposer` 发出 `send_requested(text, model_id, agent_id, attachments)`。
2. `AIAgentUIDock` 调用 `adapter->send_prompt(prompt)`。
3. adapter 构造 `AISessionService::prompt()` 输入。
4. `AISessionService` 负责 admission、attachment resolve、event append、promotion/runner。
5. adapter 刷新 projector，发出消息和运行状态变化信号。
6. Dock 清空输入框，并刷新消息列表。

### 取消运行

UI：

1. 用户点击 Stop。
2. Dock 调用 `adapter->cancel_active_run()`。
3. adapter 调用 `AISessionService::interrupt()`。
4. session runner / execution 根据 cancel token 或 interrupt event 停止。
5. adapter 发出 `run_state_changed` 和 `messages_changed`。

### 切换会话

UI：

1. 用户从下拉列表选择 session。
2. Dock 调用 `adapter->set_active_session(id)`。
3. adapter 确认 session 存在，刷新 projector 中对应消息。
4. Dock 调用 `adapter->get_messages(id)` 并重建消息列表。

## 权限流程

`agent_v1` 中权限由 `AIPermissionService` 管理。UI 不直接执行工具，也不直接改 tool state。

流程：

1. runner/tool registry 请求权限。
2. `AIPermissionService` 产生 pending request。
3. adapter 轮询或监听 pending request，并发出 `permission_requested`。
4. Dock 显示权限确认弹窗。
5. 用户 approve/reject。
6. Dock 调用 `adapter->reply_permission(request_id, allowed)`。
7. adapter 调用 `AISessionService::reply_permission()` 或 `AIPermissionService::reply()`。
8. runner 继续或记录拒绝。
9. adapter 刷新消息和状态。

第一阶段可只支持简单 approve/reject。后续再扩展：

- always allow for session
- always allow for project
- show diff before write
- requirement form

## 配置流程

旧 UI 中模型、MCP、Skill、Rule 分别有静态 settings 类。新 UI 不复用这些旧类。

新设置页通过 `AIAgentUIConfigAdapter` 访问 `AIConfigService`：

```cpp
Array list_model_profiles() const;
Dictionary save_model_profile(const Dictionary &p_profile);
bool remove_model_profile(const String &p_profile_id);

Array list_mcp_servers() const;
Dictionary save_mcp_server(const Dictionary &p_server);
bool remove_mcp_server(const String &p_server_id);

Array list_skills() const;
Dictionary save_skill(const Dictionary &p_skill);
bool remove_skill(const String &p_skill_id);

Array list_rules() const;
Dictionary save_rule(const Dictionary &p_rule);
bool remove_rule(const String &p_rule_id);
```

这些方法内部以 `AIConfigService::patch_config()` 更新配置。配置 schema 应尽量接近 `agent_v1` 已有 config 契约，而不是复制旧 `AIModelSettings` 的存储路径。

`AIConfigService` 当前默认配置形态是：

```text
{
  "default_provider": "fake",
  "default_model": "fake-model",
  "providers": {
    "fake": {
      "type": "fake",
      "model": "fake-model"
    }
  },
  "agents": {
    "main": {
      "provider": "fake",
      "model": "fake-model",
      "system": []
    }
  },
  "permissions": {
    "rules": []
  },
  "mcp": {
    "servers": {}
  },
  "skills": {
    "sources": [],
    "guidance": []
  },
  "metadata": {
    "version": 1
  }
}
```

配置页设计约束：

- 模型页面第一阶段应围绕 `default_provider`、`default_model`、`providers` 展示和编辑。
- 如果 UI 需要“多个模型 profile”体验，先由 config adapter 把 provider entries 投影成 profile view model；不要直接引入旧 `AIModelSettings` 的 profile 存储格式。
- 如果确实需要持久化多个 profile，应先扩展 `agent_v1/config` schema，再让 UI 适配新 schema。
- MCP 设置应写入 `mcp.servers`，当前默认是 Dictionary，不是 Array。
- Skill 设置应优先写入 `skills.sources` / `skills.guidance`，这两项已被 `AIContextSourceRegistry` 消费。
- Rule/权限设置应写入 `permissions.rules`，这两项已被 `AISessionRunner` 和 `AIV1ToolRegistry` 消费。
- UI 设置页只操作 view model，由 config adapter 做 schema 适配和迁移。

## 附件和引用

旧 `AIReferenceResolver` 位于 UI 目录，但它实际包含引用解析和附件生成逻辑。新 UI 应拆成两层：

- UI 层：识别输入框中的引用 token，仅用于高亮和草稿展示。
- 适配层/domain 层：解析附件、生成 blob ref、构造 prompt parts。

建议：

```text
agent_ui/widgets/agent_ui_reference_text_edit.*
  - 只做 token 高亮和插入。

agent_ui/adapter/agent_ui_attachment_adapter.*
  - 将 UI 草稿附件转换为 AISessionService prompt input。
```

约束：

- UI 草稿可以持有本地 path。
- 进入 `agent_v1` durable event 的附件必须由 attachment resolver/blob store 处理。
- provider adapter 不接收原始本地 path。

## Plan 面板

旧 `AIPlanPanel` 直接订阅 `AIPlanManager`。新 UI 不复用旧 planning 模块。

第一阶段：

- 保留 Plan 面板位置。
- adapter 暴露 `get_active_plan()`，没有 `agent_v1` plan 数据时返回空 view model。
- UI 在无计划时隐藏面板，行为与旧 UI 一致。

建议 plan view model：

```text
{
  "id": "...",
  "session_id": "...",
  "title": "Plan",
  "status": "active|archived|complete",
  "tasks": [
    {
      "id": "...",
      "title": "...",
      "status": "pending|in_progress|completed"
    }
  ]
}
```

后续补齐时应优先把 plan 表示为 `agent_v1` domain/event/projection 的一部分，或作为 tool result view model 投影出来。不要在新 UI 中重新引入旧 `AIPlanManager` singleton。

## 变更审查

旧 `AIChangeReviewPanel` 依赖 `AIChangeSetStore`。新 UI 不复用旧 store。

第一阶段：

- 保留面板位置。
- 当 `agent_v1` 没有 change set 投影时显示空状态。
- 不接入旧 `AIChangeSetStore`。

第二阶段：

- 工具写文件、编辑场景、修改资源时产生 `agent_v1` tool/change events。
- 适配层把这些 events 投影为 change review view model。
- Review 面板显示 pending changes。

建议 view model：

```text
{
  "id": "...",
  "session_id": "...",
  "title": "...",
  "path": "...",
  "kind": "file|scene|shader|script",
  "status": "pending|kept|reverted",
  "diff": {
    "old_text": "...",
    "new_text": "..."
  }
}
```

## MCP、Skill、Rule 迁移策略

### MCP

旧 MCP 在 `ai_component` 中已有完整服务链路。新 UI 不应调用 `AIMCPService`。

第一阶段：

- 设置页可先读写 `AIConfigService` 中的 MCP server 配置。
- Dock 的 MCP 状态按钮可显示配置数量和“未连接/待实现”状态。

第二阶段：

- 在 `agent_v1` 实现 MCP discovery/service/tool materialization。
- adapter 暴露 `list_mcp_statuses()`。
- UI 才展示实时 MCP 状态和 tool count。

### Skill

第一阶段：

- Skill 设置页读写 config 中的 skill 列表。
- Runner/context source 后续从 config/context source registry 消费。

第二阶段：

- 适配层展示 skill enabled/configured 状态。
- `agent_v1` tool registry 提供 activate skill 能力时，消息列表展示对应工具调用。

### Rule

Rule 应作为 config/context source 的一部分，不再使用旧 `AIRuleSettings`。

第一阶段：

- 设置页提供添加、编辑、删除、启用/禁用。
- config adapter 写入 `permissions.rules`。

第二阶段：

- `AIContextSourceRegistry` 从 rules 构造 system context source。

## 实时更新策略

第一阶段采用简单策略：

- 发送 prompt 后重刷 session list 和 message list。
- 运行中通过定时器或 execution callback 刷新 projector。
- 权限 pending 通过 adapter refresh 检测。
- UI 只依赖 `messages_changed` 和 `run_state_changed`。

第二阶段优化：

- adapter 订阅 `AIEventStore::event_appended`、`durable_event_appended`、`live_event_appended`。
- 将 `AIStreamEvent`、live event、projection delta 转换为 UI delta。
- 消息列表支持增量 append/update，减少闪烁。

## ClassDB 注册

新 UI 需要在 `editor/register_editor_types.cpp` 注册新类：

```cpp
GDREGISTER_CLASS(AIAgentUIDock);
GDREGISTER_CLASS(AIAgentUISettingsDialog);
GDREGISTER_CLASS(AIAgentUIComposer);
GDREGISTER_CLASS(AIAgentUIMessageList);
GDREGISTER_CLASS(AIAgentUIMessageBubble);
GDREGISTER_CLASS(AIAgentUIMarkdownLabel);
GDREGISTER_CLASS(AIAgentUIAttachmentBar);
GDREGISTER_CLASS(AIAgentUIReferenceTextEdit);
GDREGISTER_CLASS(AIAgentUIPlanPanel);
GDREGISTER_CLASS(AIAgentUIChangeReviewPanel);
GDREGISTER_CLASS(AIAgentUITextDiffViewer);
GDREGISTER_CLASS(AIAgentUIModelProfileDialog);
GDREGISTER_CLASS(AIAgentUIMCPServerDialog);
GDREGISTER_CLASS(AIAgentUISkillDialog);
GDREGISTER_CLASS(AIAgentUIRequirementFormDialog);
GDREGISTER_CLASS(AIAgentUISettingsModelsPage);
GDREGISTER_CLASS(AIAgentUISettingsMCPPage);
GDREGISTER_CLASS(AIAgentUISettingsSkillsPage);
GDREGISTER_CLASS(AIAgentUISettingsRulesPage);
GDREGISTER_CLASS(AIAgentUISettingsMarqueePage);
```

适配层如果只在 C++ 中使用，可以不注册全部类。需要被 ObjectDB、signal、script 或测试直接访问的类再注册。

## 测试策略

新增测试应覆盖适配层优先，而不是只测 UI 控件。

建议测试文件：

```text
tests/editor/test_agent_ui_adapter.cpp
tests/editor/test_agent_ui_config_adapter.cpp
tests/editor/test_agent_ui_view_models.cpp
```

第一阶段测试：

- adapter 能创建 session。
- adapter 能发送 prompt 并得到 projected user message。
- adapter 能切换 active session。
- adapter 能把 projector message 转换为 UI message view model。
- adapter 能返回 model list。
- config adapter 能保存/读取模型配置。
- permission pending request 能转换为 UI request。

UI 控件测试：

- Composer 在无模型时禁用发送。
- Composer 在运行中切换为停止状态。
- Message list 能渲染 user/assistant/error/tool view model。
- Settings dialog 能切换页面，并通过 adapter 保存。

## 分阶段实施

### Phase 0：设计和骨架

目标：

- 新增 `editor/agent_ui` 目录和 SCsub。
- 新增 adapter/types 基础类。
- 不接入编辑器入口，不影响旧 UI。

产出：

- `AIAgentUIAdapter`
- `AIAgentUIConfigAdapter`
- `AIAgentUITypes`
- 基础单元测试

验收：

- `agent_ui` 可单独编译。
- 无任何 `editor/ai_component/...` include。
- adapter 可实例化 `AISessionService` 默认依赖。

### Phase 1：Dock 和聊天主流程

目标：

- 复刻旧 Dock 主布局。
- 实现 session list、message list、composer。
- 接入 `AISessionService::create()`、`prompt()`、`interrupt()`。

产出：

- `AIAgentUIDock`
- `AIAgentUIComposer`
- `AIAgentUIMessageList`
- `AIAgentUIMessageBubble`
- editor 实验入口

验收：

- 可以打开新 Dock。
- 可以创建会话。
- 删除会话按钮在 `agent_v1` 支持前禁用或显示明确 unsupported。
- 可以发送 prompt。
- 可以显示 user/assistant/system/error 消息。
- 可以取消运行。

### Phase 2：配置和模型页

目标：

- 新设置窗口接入 `AIConfigService`。
- Composer 模型列表来自 config adapter。

产出：

- `AIAgentUISettingsDialog`
- `AIAgentUISettingsModelsPage`
- `AIAgentUIModelProfileDialog`

验收：

- 可以添加、编辑、删除模型配置。
- Composer 模型下拉能刷新。
- 保存配置不依赖旧 `AIModelSettings`。

### Phase 3：权限和工具状态

目标：

- 接入 `AIPermissionService` pending request。
- UI 展示 approve/reject。
- 消息气泡展示工具调用状态。

验收：

- 工具调用需要权限时，Dock 弹出审批。
- approve/reject 能回写 `agent_v1`。
- 消息列表能反映工具状态变化。

### Phase 4：MCP、Skill、Rule 设置页

目标：

- 设置页读写 `AIConfigService`。
- 不接入旧 `AIMCPSettings/AISkillSettings/AIRuleSettings`。

验收：

- MCP/Skill/Rule 设置可保存到 `agent_v1` config。
- Dock 状态按钮来自 adapter。
- 缺失的后端能力以明确空状态显示。

### Phase 5：变更审查和附件

目标：

- 新附件草稿和 resolver 接入 `agent_v1` attachment flow。
- 新 change review 接入 `agent_v1` event/projection。
- Plan 面板接入 `agent_v1` plan/tool result view model，或保持明确空状态。

验收：

- 附件进入 durable event 前被 resolver/blob store 处理。
- 文件/资源变更可在 Review 面板查看。
- Plan 面板不依赖旧 `AIPlanManager`。
- 不依赖旧 `AIChangeSetStore`。

### Phase 6：切换默认入口

目标：

- 新 UI 功能达到旧 UI 主路径能力。
- 默认入口切换到 `AIAgentUIDock`。
- 旧 UI 进入维护或废弃路径。

验收：

- 新 UI 默认可用。
- 旧 UI 可通过开关回退，或明确标记 deprecated。
- 文档和测试更新完成。

## 依赖边界检查

`editor/agent_ui` 中禁止：

```cpp
#include "editor/ai_component/..."
```

`editor/agent_ui/widgets` 中禁止直接 include：

```cpp
#include "editor/agent_v1/session/..."
#include "editor/agent_v1/domain/..."
#include "editor/agent_v1/runtime/..."
#include "editor/agent_v1/permission/..."
```

widgets 只允许依赖：

- Godot UI/control 类。
- `editor/agent_ui/adapter/agent_ui_types.h`。
- 必要的 `core/variant`、`core/string`、`core/templates`。

adapter 可以依赖 `agent_v1` 服务。

## 风险和应对

### UI 复刻时复制旧耦合

风险：复制旧文件会把旧后端 include 一并带入。

应对：

- 可以参考旧 UI 布局，但新类重新命名、重新接 adapter。
- 先写 adapter 接口，再迁控件。
- 每个 phase 用 `rg "editor/ai_component" editor/agent_ui` 检查。

### `agent_v1` 能力尚未覆盖旧后端

风险：MCP、Skill、Change Review 等旧能力在 `agent_v1` 中未完全实现。

应对：

- UI 可以先保留入口和空状态。
- config 先落地，runtime 消费后续补齐。
- 不为了短期完整性回连旧后端。

### 实时流式刷新复杂

风险：直接做完整 live delta 容易拉大范围。

应对：

- Phase 1 先用粗粒度刷新。
- 确认数据路径后，再做 event/live stream 增量。

### 新旧 UI 共存入口混乱

风险：用户不知道当前打开的是旧 UI 还是新 UI。

应对：

- 新 Dock 名称可暂定为 `AI Agent V1` 或 `Agent UI`。
- 设置项明确标记 experimental。
- 切换默认入口前统一命名。

## 验收标准

整体迁移完成时应满足：

- `editor/agent_ui` 不依赖 `editor/ai_component`。
- 新 UI 能完成主要聊天流程：创建会话、选择模型、发送、显示回复、取消。
- 新 UI 设置页通过 `AIConfigService` 管理配置。
- 权限审批通过 `AIPermissionService` 或 `AISessionService::reply_permission()` 完成。
- 消息展示来自 `AISessionProjector` 或 adapter view model。
- 附件进入后端前经过 `agent_v1` resolver/blob store。
- 旧 UI 可保留但不再是新能力开发入口。
- 文档、测试和注册入口同步更新。

## 推荐下一步

建议先实现 Phase 0：

1. 创建 `editor/agent_ui/adapter` 骨架。
2. 定义 `AIAgentUIAdapter` 的最小命令和 view model。
3. 写 `test_agent_ui_adapter.cpp`，验证不接旧 `ai_component` 也能创建 session/service。
4. 再开始复刻 Dock UI。

这样可以先把边界钉牢，再做视觉和交互迁移。
