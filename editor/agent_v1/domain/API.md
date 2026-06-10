# Agent V1 Domain API

本文档描述 `editor/agent_v1/domain` 中已经建立的 Domain Model 与 Event Log 基础契约。该层对应 `.devdocs/opencode-agent-architecture/01-domain-model-and-event-log.zh.md`，用于承载可恢复会话状态的源头：durable event log、`session_input`、`session_message` 投影和 Session 级 Context Epoch。

后续 Runner、Session service、ToolRegistry、Permission、UI bridge、Provider stream adapter 都应围绕这里的事件和投影契约接入，而不是直接从 provider 增量、UI 临时状态或旧 `editor/ai_component` 消息结构恢复会话。

## 设计定位

`agent_v1/domain` 负责：

- 定义 Session、Location、Prompt、Attachment、Message、ToolState、ContextEpoch 等领域值对象。
- 定义 `session.next.*` 与 `permission.v2.*` 事件名。
- 区分 durable event 与 live-only delta。
- 提供 append-only Session aggregate event store。
- 将 durable events 投影到 `session_input` 与 `session_message`。
- 提供 Runner history 入口，避免 Runner 直接读 raw events。
- 管理 Session 级 Context Epoch baseline/snapshot/revision。

`agent_v1/domain` 不负责：

- 调用 provider。
- 执行工具。
- 决定权限。
- 创建 UI 草稿附件状态。
- 调度单 MainAgent 工作流。
- 恢复旧多 Agent planning 工作流。

## 目录结构

```text
editor/agent_v1/domain
  API.md
  SCsub
  model/
    ai_domain_types.*
  events/
    ai_event_types.*
    ai_event_store.*
  context/
    ai_context_epoch_store.*
  projection/
    ai_session_projector.*
    ai_session_history.*
```

构建入口：

```python
# editor/SCsub
SConscript("agent_v1/domain/SCsub")
```

已注册到 ClassDB 的服务类：

```cpp
GDREGISTER_CLASS(AIContextEpochStore);
GDREGISTER_CLASS(AIEventStore);
GDREGISTER_CLASS(AISessionHistory);
GDREGISTER_CLASS(AISessionProjector);
```

值对象保持为 C++ struct，不注册为 Godot Object。对外脚本边界使用 `Dictionary` / `Array`。

## 全局约束

- Session recoverable state 必须来自 durable events 和投影读模型。
- Durable replay cursor 基于同一个 Session aggregate 内单调递增的 `seq`。
- Live-only delta 不写入 durable log，不推进 durable cursor。
- Prompt admitted 只进入 durable inbox，不进入模型历史。
- Prompt promoted 才投影为 model-visible user message。
- Tool settlement 必须使用 `assistantMessageID + callID` 定位，不能只依赖 provider call id。
- Context Epoch 是 Session 级 baseline，不是 provider turn 级请求快照。
- Compaction 不删除旧事件；Runner history 从最新 compaction checkpoint 之后继续。
- Provider adapter 不应直接写 `session_message`，应发出 normalized stream events，由 Runner 决定 durable/live event 边界。

## model

文件：

```cpp
#include "editor/agent_v1/domain/model/ai_domain_types.h"
```

### 枚举

```cpp
enum AISessionInputDelivery {
	AI_SESSION_INPUT_DELIVERY_STEER,
	AI_SESSION_INPUT_DELIVERY_QUEUE,
};

enum AISessionMessageType {
	AI_SESSION_MESSAGE_USER,
	AI_SESSION_MESSAGE_ASSISTANT,
	AI_SESSION_MESSAGE_SYSTEM,
	AI_SESSION_MESSAGE_COMPACTION,
};

enum AIToolStatus {
	AI_TOOL_STATUS_PENDING,
	AI_TOOL_STATUS_RUNNING,
	AI_TOOL_STATUS_SUCCESS,
	AI_TOOL_STATUS_FAILED,
};
```

转换函数：

```cpp
String ai_session_input_delivery_to_string(AISessionInputDelivery p_delivery);
AISessionInputDelivery ai_session_input_delivery_from_string(const String &p_delivery);

String ai_session_message_type_to_string(AISessionMessageType p_type);
AISessionMessageType ai_session_message_type_from_string(const String &p_type);

String ai_tool_status_to_string(AIToolStatus p_status);
AIToolStatus ai_tool_status_from_string(const String &p_status);
```

### AILocationRef

```cpp
struct AILocationRef {
	String directory;
	String workspace_id;
};
```

语义：

- Runner、ToolRegistry、Permission、filesystem、System Context 都应按 Location scope 隔离。
- Session move 后必须重置 Context Epoch，避免复用旧目录的 privileged context。

### AIModelRef

```cpp
struct AIModelRef {
	String provider;
	String model;
	Dictionary metadata;
};
```

用于记录 Session 当前偏好的 provider/model。业务层可以在 `metadata` 中保留 provider-neutral 的模型信息，但不要把 provider 专属请求体泄漏为 Domain 语义。

### AITokenUsage

```cpp
struct AITokenUsage {
	int64_t input_tokens;
	int64_t output_tokens;
	int64_t cache_read_tokens;
	int64_t cache_write_tokens;
};
```

主要 API：

```cpp
int64_t get_total_tokens() const;
Dictionary to_dictionary() const;
static AITokenUsage from_dictionary(const Dictionary &p_dict);
```

### AIFileAttachment

```cpp
struct AIFileAttachment {
	String id;
	String path;
	String name;
	String mime;
	int64_t size_bytes;
	Dictionary metadata;
};
```

语义：

- 这里表示 durable typed attachment。
- UI 草稿附件不属于 Domain，只有被 admitted prompt 接收后才进入该结构。
- `path` 是工作区/文件引用语义，后续 resolver 应负责转换为模型可消费的安全内容。

### AIAgentReference

```cpp
struct AIAgentReference {
	String id;
	String name;
	Dictionary metadata;
};
```

用于 prompt 中的结构化 agent 引用。当前项目保持单 MainAgent 工作流，不应借此恢复旧多 Agent planning 类型。

### AIPromptReference

```cpp
struct AIPromptReference {
	String id;
	String kind;
	String label;
	Variant value;
	Dictionary metadata;
};
```

用于 prompt/reference expansion 的结构化入口。具体 expansion 由后续 Session/Runner 层处理。

### AIPrompt

```cpp
struct AIPrompt {
	String text;
	Vector<AIFileAttachment> files;
	Vector<AIAgentReference> agents;
	Vector<AIPromptReference> references;
};
```

语义：

- Prompt 是结构化输入，不是单纯字符串。
- Prompt admitted 后仍不进入模型历史。
- Prompt promoted 后才投影为 `AISessionMessage` user message。

### AISessionInfo

```cpp
struct AISessionInfo {
	String id;
	String project_id;
	AILocationRef location;
	String path;
	String title;
	String agent_id;
	AIModelRef model;
	double cost;
	AITokenUsage tokens;
	uint64_t time_created;
	uint64_t time_updated;
};
```

当前 Domain 层只定义结构，不实现 `sessions.create()`。Session service 后续应负责创建、复用、冲突检测、路径和持久化。

### AISessionInput

```cpp
struct AISessionInput {
	int64_t admitted_seq;
	String id;
	String session_id;
	AIPrompt prompt;
	AISessionInputDelivery delivery;
	uint64_t time_created;
	int64_t promoted_seq;
};
```

主要 API：

```cpp
bool is_promoted() const;
Dictionary to_dictionary() const;
static AISessionInput from_dictionary(const Dictionary &p_dict);
```

语义：

- `admitted_seq` 来自 `session.next.prompt.admitted` 的 durable aggregate seq。
- `promoted_seq > 0` 表示该 input 已被提升为模型可见 user message。
- 精确重试冲突策略应由 Session service 在 append 前处理。

### AIToolState

```cpp
struct AIToolState {
	AIToolStatus status;
	Variant input;
	Variant progress;
	Variant output;
	PackedStringArray output_paths;
	AIError error;
	Variant result;
	Dictionary provider;
	Dictionary metadata;
};
```

构造辅助：

```cpp
static AIToolState pending(const Variant &p_input = Variant(), const Dictionary &p_provider = Dictionary());
static AIToolState running(const Variant &p_input, const Dictionary &p_provider = Dictionary());
static AIToolState success(const Variant &p_input, const Variant &p_output, const PackedStringArray &p_output_paths = PackedStringArray(), const Dictionary &p_provider = Dictionary());
static AIToolState failed(const AIError &p_error, const Variant &p_input = Variant(), const Dictionary &p_provider = Dictionary());
```

语义：

- `pending/running` 表示仍未稳定完成的工具状态。
- 新 run 开始前，后续执行层应调用 projector 标记遗留 pending/running local tools 为 interrupted。
- Provider-executed tools 应保留 `provider.executed = true` 及 provider metadata。

### AIAssistantContent

```cpp
struct AIAssistantContent {
	String type; // "text" | "reasoning" | "tool"
	String id;
	String name;
	String text;
	AIToolState tool_state;
	Dictionary provider_metadata;
	Dictionary metadata;
};
```

构造辅助：

```cpp
static AIAssistantContent text_content(const String &p_text, const String &p_id = String());
static AIAssistantContent reasoning_content(const String &p_text, const Dictionary &p_provider_metadata = Dictionary(), const String &p_id = String());
static AIAssistantContent tool_content(const String &p_call_id, const String &p_name, const AIToolState &p_state, const Dictionary &p_provider_metadata = Dictionary());
```

语义：

- Assistant message 是 tool settlement 的 durable owner。
- Tool content 的 `id` 是 provider call id，但定位时必须先找到 assistant message。

### AISessionMessage

```cpp
struct AISessionMessage {
	int64_t seq;
	String id;
	String session_id;
	AISessionMessageType type;
	String text;
	Vector<AIFileAttachment> files;
	Vector<AIAgentReference> agents;
	Vector<AIPromptReference> references;
	Vector<AIAssistantContent> content;
	Dictionary metadata;
	uint64_t time_created;
};
```

构造辅助：

```cpp
static AISessionMessage user_message(...);
static AISessionMessage assistant_shell(...);
static AISessionMessage system_message(...);
static AISessionMessage compaction_message(...);
```

Runner 可见性：

```cpp
bool is_runner_visible_system_message(int64_t p_baseline_seq) const;
```

语义：

- UI 和 Runner 都应读 projected `AISessionMessage`。
- Runner 不直接读 raw events。
- System message 只有在 `seq > baseline_seq` 时才进入 Runner history。

### AIContextEpoch

```cpp
struct AIContextEpoch {
	String session_id;
	String baseline;
	Dictionary snapshot;
	String agent_id;
	int64_t baseline_seq;
	int64_t replacement_seq;
	int revision;
};
```

主要 API：

```cpp
bool has_replacement() const;
Dictionary to_dictionary() const;
static AIContextEpoch from_dictionary(const Dictionary &p_dict);
```

语义：

- `baseline` 是进入模型的 privileged system context。
- `snapshot` 是隐藏观察数据，不直接进入模型。
- `baseline_seq` 用于 Runner history 过滤旧 system updates。
- `replacement_seq` 标记 agent/model/compaction/location move 等 replacement boundary。
- `revision` 用于上层 fencing 和变更检测。

## events

文件：

```cpp
#include "editor/agent_v1/domain/events/ai_event_types.h"
#include "editor/agent_v1/domain/events/ai_event_store.h"
```

### AIDomainEventTypes

事件名获取：

```cpp
static String prompt_admitted();
static String prompt_promoted();
static String step_started();
static String step_ended();
static String step_failed();
static String text_started();
static String text_delta();
static String text_ended();
static String reasoning_started();
static String reasoning_delta();
static String reasoning_ended();
static String tool_input_started();
static String tool_input_delta();
static String tool_input_ended();
static String tool_called();
static String tool_progress();
static String tool_success();
static String tool_failed();
static String context_updated();
static String compaction_started();
static String compaction_delta();
static String compaction_ended();
static String interrupt_requested();
static String permission_asked();
static String permission_replied();
```

分类辅助：

```cpp
static bool is_live_only_event(const String &p_type);
static bool is_durable_event(const String &p_type);
static bool is_session_event(const String &p_type);
static bool is_permission_event(const String &p_type);
```

当前 live-only 事件：

```text
session.next.text.delta
session.next.reasoning.delta
session.next.tool.input.delta
session.next.compaction.delta
```

这些事件只能用于实时 UI，不可用于 reconnect/replay/recovery。

### AIEventRow

```cpp
struct AIEventRow {
	String id;
	String aggregate_id;
	int64_t seq;
	String type;
	Dictionary data;
	uint64_t timestamp;
	bool live_only;
};
```

主要 API：

```cpp
Dictionary to_dictionary() const;
static AIEventRow from_dictionary(const Dictionary &p_dict);
```

语义：

- `aggregate_id` 当前等同于 Session ID。
- Durable event 的 `seq` 在同一 aggregate 内单调递增。
- Live-only event 使用当前 durable seq，不推进 durable cursor。

### AIEventStore

```cpp
class AIEventStore : public RefCounted;
```

默认路径：

```text
user://agent_v1/events
```

每个 aggregate 一个 JSONL 文件：

```text
user://agent_v1/events/<aggregate_id>.jsonl
```

主要 API：

```cpp
void set_base_dir(const String &p_base_dir);
String get_base_dir() const;

bool append(const String &p_aggregate_id, const String &p_type, const Dictionary &p_data, bool p_live_only, AIEventRow &r_row, String &r_error);
Dictionary append_event(const String &p_aggregate_id, const String &p_type, const Dictionary &p_data, bool p_live_only = false);
Dictionary append_durable_event(const String &p_aggregate_id, const String &p_type, const Dictionary &p_data);
Dictionary append_live_event(const String &p_aggregate_id, const String &p_type, const Dictionary &p_data);

Vector<AIEventRow> list(const String &p_aggregate_id, int64_t p_after_seq = 0, bool p_include_live = false);
Array list_events(const String &p_aggregate_id, int64_t p_after_seq = 0, bool p_include_live = false);
Array replay_events(const String &p_aggregate_id, int64_t p_after_seq = 0);

int64_t get_latest_seq(const String &p_aggregate_id);
bool load_aggregate(const String &p_aggregate_id);
void clear_memory();
```

信号：

```text
event_appended(Dictionary event)
durable_event_appended(Dictionary event)
live_event_appended(Dictionary event)
```

调用约定：

- `append_durable_event()` 写 JSONL，并推进 `get_latest_seq()`。
- `append_live_event()` 只进内存 live queue，不写 JSONL。
- `replay_events()` 永远返回 durable-only rows。
- `list_events(..., include_live = true)` 仅适合 UI/live tail 调试，不适合作为恢复来源。
- Tail wakeup 应被视为 advisory；消费者收到信号后仍应按 durable cursor 重新查询。

示例：

```cpp
Ref<AIEventStore> store;
store.instantiate();

Dictionary data;
data["id"] = "msg_1";
data["session_id"] = session_id;
data["prompt"] = prompt.to_dictionary();
data["delivery"] = "queue";

Dictionary row = store->append_durable_event(session_id, AIDomainEventTypes::prompt_admitted(), data);
```

## projection

文件：

```cpp
#include "editor/agent_v1/domain/projection/ai_session_projector.h"
#include "editor/agent_v1/domain/projection/ai_session_history.h"
```

### AISessionProjector

```cpp
class AISessionProjector : public RefCounted;
```

主要 API：

```cpp
bool project(const AIEventRow &p_row);
int project(const Vector<AIEventRow> &p_rows);
bool project_event(const Dictionary &p_event);
int project_events(const Array &p_events);
int project_from_store(const Ref<AIEventStore> &p_store, const String &p_session_id, int64_t p_after_seq = 0);
int rebuild_from_store(const Ref<AIEventStore> &p_store, const String &p_session_id);

Vector<AISessionInput> get_inputs_struct(const String &p_session_id) const;
Vector<AISessionMessage> get_messages_struct(const String &p_session_id) const;
bool get_context_epoch_struct(const String &p_session_id, AIContextEpoch &r_epoch) const;

Array get_inputs(const String &p_session_id) const;
Array get_messages(const String &p_session_id) const;
Dictionary get_context_epoch(const String &p_session_id) const;
int64_t get_projected_seq(const String &p_session_id) const;
int mark_pending_tools_interrupted(const String &p_session_id, const String &p_reason = "Tool execution interrupted");
void clear_session(const String &p_session_id);
void clear();
```

当前投影规则：

```text
session.next.prompt.admitted
  -> insert session_input row

session.next.prompt.promoted
  -> mark session_input.promoted_seq
  -> insert user session_message

session.next.step.started
  -> create/update assistant message shell

session.next.step.ended / failed
  -> update assistant message metadata status

session.next.text.ended
  -> append assistant text content

session.next.reasoning.ended
  -> append assistant reasoning content with provider metadata

session.next.tool.input.ended
  -> append/update assistant tool content as pending

session.next.tool.called
  -> append/update assistant tool content as running

session.next.tool.progress
  -> update assistant tool progress

session.next.tool.success / failed
  -> settle assistant tool content by assistantMessageID + callID

session.next.context.updated
  -> insert system session_message
  -> update projected context epoch if epoch/baseline data exists

session.next.compaction.ended
  -> insert compaction session_message
```

关键约束：

- Projector 忽略 live-only events。
- `get_projected_seq()` 只反映 durable projection cursor。
- Tool settlement 必须先定位 assistant message，再定位 tool call id。
- `mark_pending_tools_interrupted()` 只修改当前投影读模型，不写 durable event；后续 SessionExecution 可以在新 run 开始时调用它恢复 UI 状态。

示例：

```cpp
Ref<AISessionProjector> projector;
projector.instantiate();

projector->project_from_store(store, session_id);
Array messages = projector->get_messages(session_id);
```

### AISessionHistory

```cpp
class AISessionHistory : public RefCounted;
```

主要 API：

```cpp
static Vector<AISessionMessage> entries_for_runner(const Vector<AISessionMessage> &p_messages, int64_t p_baseline_seq);

Array entries_for_runner_from_messages(const Array &p_messages, int64_t p_baseline_seq) const;
Array entries_for_runner_from_projector(const Ref<AISessionProjector> &p_projector, const String &p_session_id, int64_t p_baseline_seq) const;
```

规则：

- 查找最新 `AI_SESSION_MESSAGE_COMPACTION` 的 `seq` 作为 compaction boundary。
- 丢弃 boundary 之前的消息。
- System message 只有 `seq > baseline_seq` 时进入 Runner。
- Runner 后续应基于这里的结果构造 `AIModelRequest.messages`，不要直接读 EventStore。

## context

文件：

```cpp
#include "editor/agent_v1/domain/context/ai_context_epoch_store.h"
```

### AIContextEpochStore

```cpp
class AIContextEpochStore : public RefCounted;
```

主要 API：

```cpp
bool set_epoch_struct(const AIContextEpoch &p_epoch);
bool get_epoch_struct(const String &p_session_id, AIContextEpoch &r_epoch) const;
AIContextEpoch reset_epoch_struct(const String &p_session_id, const String &p_baseline, const Dictionary &p_snapshot, const String &p_agent_id, int64_t p_baseline_seq, int64_t p_replacement_seq = 0);

bool set_epoch(const Dictionary &p_epoch);
Dictionary get_epoch(const String &p_session_id) const;
Dictionary reset_epoch(const String &p_session_id, const String &p_baseline, const Dictionary &p_snapshot, const String &p_agent_id, int64_t p_baseline_seq, int64_t p_replacement_seq = 0);
bool has_epoch(const String &p_session_id) const;
void clear();
```

语义：

- Store 当前是内存表，后续可由 Session service 替换为磁盘持久化表。
- `reset_epoch()` 会递增 revision。
- `baseline_seq` 与 Runner history 过滤配合使用。
- Location move、agent/model switch、compaction replacement 都应调用 reset。

## 推荐调用链

### Prompt Admission

```text
UI Composer
  -> Session service validates existing session
  -> append session.next.prompt.admitted
  -> Projector inserts session_input
  -> Runner drain decides when to promote
```

关键点：

- admitted 不是模型历史。
- exact retry conflict 检测应在 Session service 中完成。

### Prompt Promotion

```text
SessionExecution
  -> append session.next.prompt.promoted
  -> Projector marks promoted_seq
  -> Projector inserts user session_message
  -> SessionHistory exposes it to Runner
```

### Provider Streaming

```text
Provider adapter
  -> AIStreamEvent
Runner
  -> maps boundary events to durable session.next.*
  -> maps deltas to live-only session.next.*.delta
EventStore
  -> durable JSONL / live in-memory
Projector
  -> durable session_input/session_message
```

关键点：

- `text.delta`、`reasoning.delta`、`tool.input.delta`、`compaction.delta` 只能给实时 UI。
- replay/recovery 必须依赖 `*.ended`、`tool.success/failed`、`prompt.promoted`、`context.updated`。

### Tool Settlement

```text
Runner receives tool call
  -> append session.next.tool.called
Tool execution / provider result
  -> append session.next.tool.success or failed
Projector
  -> find assistantMessageID
  -> find callID inside assistant content
  -> update ToolState
```

关键点：

- 不要只用 `callID` 全局查找。
- Provider-executed tool metadata 必须保留在 `provider` / `provider_metadata`。

### Reconnect / Replay

```text
client has cursor seq
  -> AIEventStore.replay_events(session_id, cursor)
  -> durable rows only
  -> client advances cursor with row.seq
```

关键点：

- live-only rows 不参与 cursor。
- tail signal 只是唤醒，不是可靠队列。

### Runner History

```text
SessionHistory.entries_for_runner(...)
  -> latest compaction boundary
  -> messages after boundary
  -> system messages after baseline_seq
  -> Runner builds AIModelRequest
```

## 当前实现边界

- `AIEventStore` 已提供 durable JSONL 存储，但不是完整数据库层。
- `AISessionProjector` 的 read model 当前在内存中，进程重启后应通过 `rebuild_from_store()` 重建。
- `AIContextEpochStore` 当前在内存中，后续应接入 Session persistence。
- Session 创建、复用、删除、移动、title/model/cost 更新尚未在本层实现。
- Prompt exact retry conflict 尚未在本层强制执行，应由 Session service append 前检查。
- Permission events 目前只定义事件名和 durable 分类，具体权限状态机由后续 Permission 模块实现。

## 变更规则

修改 Domain API 前请先判断：

- 是否仍然是会话恢复所需的源头语义。
- 是否会破坏 durable cursor 单调性。
- 是否会把 live-only delta 误变成 replay source。
- 是否会让 Runner 绕过 projected history。
- 是否会让 tool settlement 退化为只按 callID 定位。
- 是否会把 Context Epoch 误建成 provider turn snapshot。
- 是否能在不影响旧 `editor/ai_component` 单 MainAgent 工作流的前提下演进。

如果答案不明确，优先把新能力放到后续 `session`、`runtime`、`tools`、`permission` 或 `mcp` 子目录，而不是扩张 Domain 基础契约。
