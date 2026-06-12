# Agent V1 Domain API

本文档描述 `editor/agent_v1/domain` 中已经建立的 Domain Model 与 Event Log 基础契约。该层对应 `.devdocs/opencode-agent-architecture/01-domain-model-and-event-log.zh.md`，用于承载可恢复会话状态的源头：durable event log、`session_input`、`session_message` 投影和 Session 级 Context Epoch。

后续 Runner、Session service、ToolRegistry、Permission、UI bridge、Provider stream adapter 都应围绕这里的事件和投影契约接入，而不是直接从 provider 增量或 `editor/agent_ui` 临时界面状态恢复会话。

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
  attachments/
    ai_attachment_blob_store.*
    ai_attachment_resolver.*
    ai_attachment_model_part_builder.*
  events/
    ai_event_types.*
    ai_event_store.*
  context/
    ai_system_context.*
    ai_context_source_registry.*
    ai_context_epoch_service.*
    ai_context_epoch_store.*
  projection/
    ai_session_projector.*
    ai_session_history.*
    ai_token_estimator.*
```

构建入口：

```python
# editor/SCsub
SConscript("agent_v1/domain/SCsub")
```

已注册到 ClassDB 的服务类：

```cpp
GDREGISTER_CLASS(AIAttachmentBlobStore);
GDREGISTER_CLASS(AIAttachmentResolver);
GDREGISTER_CLASS(AIFilePreprocessor);
GDREGISTER_CLASS(AIImageNormalizer);
GDREGISTER_CLASS(AIModelPartBuilder);
GDREGISTER_CLASS(AIContextEpochService);
GDREGISTER_CLASS(AIContextEpochStore);
GDREGISTER_CLASS(AIContextSourceRegistry);
GDREGISTER_CLASS(AIEventStore);
GDREGISTER_CLASS(AISessionHistory);
GDREGISTER_CLASS(AISessionProjector);
GDREGISTER_CLASS(AITokenEstimator);
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
- Prompt attachment 必须先解析成 blob-backed `AIFileAttachment`，再写入 admitted event；durable log 不保存 data URL、base64 内容或 provider 请求体。
- 本地路径附件只作为 resolver 输入。进入 `AIFileAttachment.path` 的值必须是 blob ref，Runner 和 provider adapter 不应接收原始本地路径。

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
- Prompt admission 后的 `path` 是 blob ref，通常形如 `blob_<sha256>`，不再表示本地文件路径。
- `metadata.blob_id` / `metadata.blob_hash` 是 Runner 读取内容和日志审计的稳定入口；`metadata.source_type` 记录 `data`、`path`、`blob`、`bytes` 等来源类型。
- 原始 data URL、base64 内容和本地路径不进入 `AIFileAttachment` 的 durable metadata；如需本地审计，可读取 `AIAttachmentBlobStore` 的独立 metadata 文件。

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
- `files` 必须已经由 `AIAttachmentResolver` 解析完成；不要把 UI 草稿附件、data URL 或本地 path 直接塞进 `AIPrompt.files`。

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
- 权限等待中的 local tool 仍属于 open tool call；新 run 不应无条件把它标记为 interrupted。
- 显式 interrupt 或恢复策略判定为不可继续时，应写入 durable `session.next.tool.failed`，再由 projector 投影失败状态。
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

## attachments

文件：

```cpp
#include "editor/agent_v1/domain/attachments/ai_attachment_blob_store.h"
#include "editor/agent_v1/domain/attachments/ai_attachment_resolver.h"
#include "editor/agent_v1/domain/attachments/ai_attachment_model_part_builder.h"
```

### AIAttachmentBlobStore

```cpp
class AIAttachmentBlobStore : public RefCounted;
```

默认路径：

```text
user://net.nextengine/agent_v1/attachments
```

主要 API：

```cpp
void set_base_dir(const String &p_base_dir);
String get_base_dir() const;

bool put_bytes_struct(const PackedByteArray &p_bytes, const String &p_mime_type, const Dictionary &p_source_metadata, AIAttachmentBlobRecord &r_record, AIError &r_error);
bool put_file_struct(const String &p_path, const String &p_mime_type, const Dictionary &p_source_metadata, AIAttachmentBlobRecord &r_record, AIError &r_error);
bool has_blob_struct(const String &p_blob_ref) const;
bool get_metadata_struct(const String &p_blob_ref, AIAttachmentBlobRecord &r_record, AIError &r_error) const;
bool read_bytes_struct(const String &p_blob_ref, PackedByteArray &r_bytes, AIError &r_error) const;
```

语义：

- Blob store 是附件二进制内容的本地持久化边界，metadata 与 bytes 分开读取。
- Blob id/hash 是 content-addressed 引用；Session event 只保存 blob ref 和安全 metadata。
- `base_dir` 默认必须保持在 `user://net.nextengine/` 下，测试会覆盖该约束。

### AIAttachmentResolver

```cpp
class AIAttachmentResolver : public RefCounted;
```

主要 API：

```cpp
void set_blob_store(const Ref<AIAttachmentBlobStore> &p_blob_store);
void set_max_attachment_bytes(int64_t p_max_attachment_bytes);

static String detect_mime_static(const String &p_name, const PackedByteArray &p_bytes = PackedByteArray(), const String &p_declared_mime = String());
static String mime_to_modality_static(const String &p_mime);

bool resolve_struct(const String &p_session_id, const AILocationRef &p_location, const Dictionary &p_attachment, AIFileAttachment &r_file, AIError &r_error);
bool resolve_many_struct(const String &p_session_id, const AILocationRef &p_location, const Array &p_attachments, Vector<AIFileAttachment> &r_files, AIError &r_error);
Dictionary resolve(const Dictionary &p_input);
```

输入类型：

```text
data / data_url / dataURL
path
blob / blob_id / blobID
bytes
```

语义：

- Resolver 是 `SubmittedAttachment -> AIFileAttachment` 的唯一入口。
- `data:` 输入必须 base64 编码，payload 不能为空；image/audio/pdf 等二进制 MIME 会先按内容签名校验，再通过大小检查后写入 blob store。
- `path` 输入按 `AILocationRef.directory` 约束在 workspace/root 内，读取内容后写入 blob store；输出不携带原始 path。
- `blob` 输入只水合已有 blob metadata，不重新读本地路径；默认要求 blob metadata 的 `session_id` 等于当前 Session，并且 blob size 不超过 resolver 限制。

### AIFilePreprocessor

```cpp
class AIFilePreprocessor : public RefCounted;
```

主要 API：

```cpp
void set_max_text_bytes(int64_t p_max_text_bytes);
bool bytes_to_text_context_struct(const AIFileAttachment &p_attachment, const PackedByteArray &p_bytes, String &r_text, bool &r_truncated, AIError &r_error) const;
```

语义：

- 文本类附件先转为派生 text part，metadata 标记 `derived_from_attachment`。
- 超过 `max_text_bytes` 的文本会截断并标记 `truncated`，不会把大文件直接塞进 prompt。

### AIImageNormalizer

```cpp
class AIImageNormalizer : public RefCounted;
```

主要 API：

```cpp
void set_blob_store(const Ref<AIAttachmentBlobStore> &p_blob_store);
void set_max_width(int p_max_width);
void set_max_height(int p_max_height);
void set_max_output_bytes(int64_t p_max_output_bytes);

bool normalize_struct(const AIFileAttachment &p_attachment, AIFileAttachment &r_attachment, AIError &r_error);
Dictionary normalize(const Dictionary &p_attachment);
```

语义：

- PNG/JPEG/WebP 会尝试解码；解码失败返回 validation error。
- 超过尺寸或输出大小限制的图片会生成新的 PNG blob，原始 blob 不被覆盖。
- GIF/SVG 等暂不解码规范化；仍可作为 image modality 进入后续能力检查和 provider 映射。

### AIModelPartBuilder

```cpp
class AIModelPartBuilder : public RefCounted;
```

主要 API：

```cpp
void set_blob_store(const Ref<AIAttachmentBlobStore> &p_blob_store);
void set_file_preprocessor(const Ref<AIFilePreprocessor> &p_file_preprocessor);
void set_image_normalizer(const Ref<AIImageNormalizer> &p_image_normalizer);
void set_max_inline_bytes(int64_t p_max_inline_bytes);

static bool provider_supports_modality_static(const Dictionary &p_provider_config, const String &p_modality);

bool build_attachment_part_struct(const AIFileAttachment &p_attachment, const Dictionary &p_provider_config, AIModelPart &r_part, AIError &r_error);
bool append_attachment_parts_struct(const Vector<AIFileAttachment> &p_attachments, const Dictionary &p_provider_config, Vector<AIModelPart> &r_parts, AIError &r_error);
Dictionary build_attachment_part(const Dictionary &p_attachment, const Dictionary &p_provider_config);
```

语义：

- `text` modality 不要求 provider multimodal 能力，会转成派生 text part。
- `image`、`audio`、`file` modality 必须由 provider config 显式声明支持；不支持时返回 `AI_ERROR_UNAVAILABLE`。
- 非文本 part 使用 provider-neutral `AIModelPart::data_part(...)`，data 当前是 data URL；Runner 写 `step_started.request` 时会 redaction。
- Builder 的安全 metadata 只包含 attachment id、blob id/hash、name、mime、size、modality 和 normalize 信息，不传播 `source_path`。

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
user://net.nextengine/agent_v1/events
```

每个 aggregate 一个 JSONL 文件：

```text
user://net.nextengine/agent_v1/events/<aggregate_id>.jsonl
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
#include "editor/agent_v1/domain/projection/ai_token_estimator.h"
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
- `session.next.context.updated` 的 projected epoch 以 `AIEventRow.seq` 作为 canonical `baseline_seq`，忽略旧 payload 中可能滞后的 `baseline_seq`。
- Tool settlement 必须先定位 assistant message，再定位 tool call id。
- `mark_pending_tools_interrupted()` 只修改当前投影读模型，不写 durable event；它只能用于临时读模型修正，不能作为恢复/中断的 source of truth。

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
static Vector<AISessionMessage> entries_for_runner(const Vector<AISessionMessage> &p_messages, int64_t p_baseline_seq, int64_t p_token_budget);

Array entries_for_runner_from_messages(const Array &p_messages, int64_t p_baseline_seq) const;
Array entries_for_runner_with_budget_from_messages(const Array &p_messages, int64_t p_baseline_seq, int64_t p_token_budget) const;
Array entries_for_runner_from_projector(const Ref<AISessionProjector> &p_projector, const String &p_session_id, int64_t p_baseline_seq) const;
Array entries_for_runner_with_budget_from_projector(const Ref<AISessionProjector> &p_projector, const String &p_session_id, int64_t p_baseline_seq, int64_t p_token_budget) const;
```

规则：

- 查找最新 `AI_SESSION_MESSAGE_COMPACTION` 的 `seq` 作为 compaction boundary。
- 丢弃 boundary 之前的消息。
- System message 只有 `seq > baseline_seq` 时进入 Runner。
- 设置 token budget 时，从尾部保留最近消息；system/compaction message 保留；单个 assistant tool content 不会被拆开。
- Runner 后续应基于这里的结果构造 `AIModelRequest.messages`，不要直接读 EventStore。

### AITokenEstimator

```cpp
class AITokenEstimator : public RefCounted;
```

主要 API：

```cpp
static int64_t estimate_text_tokens(const String &p_text);
static int64_t estimate_variant_tokens(const Variant &p_value);
static int64_t estimate_message_struct(const AISessionMessage &p_message);

int64_t estimate_text(const String &p_text) const;
int64_t estimate_variant(const Variant &p_value) const;
int64_t estimate_message(const Dictionary &p_message) const;
```

语义：

- 当前是 provider-neutral 的保守估算器，用于 Phase 6 history selection。
- 后续 provider tokenizer 可以替换该估算策略，但不应改变 `AISessionHistory` 的“按完整 message 裁剪”约束。

## context

文件：

```cpp
#include "editor/agent_v1/domain/context/ai_system_context.h"
#include "editor/agent_v1/domain/context/ai_context_source_registry.h"
#include "editor/agent_v1/domain/context/ai_context_epoch_service.h"
#include "editor/agent_v1/domain/context/ai_context_epoch_store.h"
```

### AISystemContextSource / AISystemContext

```cpp
struct AISystemContextSource {
	String domain;
	String text;
	String content_hash;
	bool required;
	bool available;
	int priority;
	Dictionary metadata;
};

struct AISystemContext {
	String baseline;
	Vector<AISystemContextSource> sources;
	Dictionary snapshot;
	bool available;
	String blocked_reason;
};
```

主要 API：

```cpp
bool AISystemContextSource::is_blocking() const;
Dictionary AISystemContextSource::to_dictionary() const;
static AISystemContextSource AISystemContextSource::from_dictionary(const Dictionary &p_dict);

bool AISystemContext::is_available() const;
Dictionary AISystemContext::to_dictionary() const;
static AISystemContext AISystemContext::from_dictionary(const Dictionary &p_dict);
static AISystemContext AISystemContext::combine(const Vector<AISystemContextSource> &p_sources);
```

语义：

- `domain + content_hash` 是 Context Epoch reconcile 的比较基础。
- `baseline` 是所有可用 source text 的有序组合。
- `snapshot` 是 model-hidden source observation，包含 source hash、baseline hash 和 snapshot hash。
- required source 不可用时，Runner 必须停止，不能 promote pending prompt。

### AIContextSourceRegistry

```cpp
class AIContextSourceRegistry : public RefCounted;
```

主要 API：

```cpp
void set_config_service(const Ref<AIConfigService> &p_config_service);
bool load_struct(const String &p_agent_id, const AILocationRef &p_location, const String &p_provider, const String &p_model, AISystemContext &r_context, AIError &r_error) const;
Dictionary load(const String &p_agent_id, const Dictionary &p_location, const String &p_provider, const String &p_model) const;
void add_source(const Dictionary &p_source);
void clear_sources();
void set_blocked(bool p_blocked, const String &p_reason = String());
```

当前内置 sources：

- environment facts，包括 NextEngine agent runtime、workspace directory、host-local date。
- model selection snapshot。
- config 中 selected agent 的 `system`。
- provider `system` / `instructions`。
- `skills.guidance` / `skills.sources` 作为后续 Skill 机制接入口。

### AIContextEpochService

```cpp
class AIContextEpochService : public RefCounted;
```

主要 API：

```cpp
bool initialize_struct(const String &p_session_id, const AILocationRef &p_location, const String &p_agent_id, const AISystemContext &p_context, AIContextEpoch &r_epoch, bool &r_initialized, AIError &r_error);
bool prepare_struct(const String &p_session_id, const AILocationRef &p_location, const String &p_agent_id, const AISystemContext &p_context, AIContextEpoch &r_epoch, AIError &r_error);
bool current_struct(const String &p_session_id, const String &p_agent_id, int p_revision, AIContextEpoch &r_epoch, AIError &r_error) const;
bool current_struct(const String &p_session_id, const String &p_agent_id, int p_revision, const AISystemContext &p_context, AIContextEpoch &r_epoch, AIError &r_error) const;
bool request_replacement_struct(const String &p_session_id, int64_t p_seq, AIError &r_error);
bool reset_struct(const String &p_session_id, AIError &r_error);
```

语义：

- `initialize/prepare` 是唯一可以写 `session.next.context.updated` 的入口。
- Context 未变化时复用旧 epoch，不写重复 system message。
- 内存 epoch 缺失时，服务会先从 `AISessionProjector` 或 `AIEventStore` 重建的 projected `context.updated` 水合，避免进程重启后重复写 system context。
- Context 变化、agent 变化、replacement request、compaction boundary 变化时写新的 `context.updated` 并推进 revision。
- `current_struct(..., context, ...)` 是 provider turn 前的严格 fence，必须同时确认 agent、revision、baseline 与 snapshot 未变化；冲突会返回带 `rebuild_request = true` 的 `AI_ERROR_CONFLICT`。
- `reset_struct()` 用于 Session move，下一次 turn 重新初始化 baseline。

### AIContextEpochStore

```cpp
class AIContextEpochStore : public RefCounted;
```

主要 API：

```cpp
bool set_epoch_struct(const AIContextEpoch &p_epoch);
bool get_epoch_struct(const String &p_session_id, AIContextEpoch &r_epoch) const;
AIContextEpoch reset_epoch_struct(const String &p_session_id, const String &p_baseline, const Dictionary &p_snapshot, const String &p_agent_id, int64_t p_baseline_seq, int64_t p_replacement_seq = 0);
bool clear_epoch_struct(const String &p_session_id);

bool set_epoch(const Dictionary &p_epoch);
Dictionary get_epoch(const String &p_session_id) const;
Dictionary reset_epoch(const String &p_session_id, const String &p_baseline, const Dictionary &p_snapshot, const String &p_agent_id, int64_t p_baseline_seq, int64_t p_replacement_seq = 0);
bool clear_epoch(const String &p_session_id);
bool has_epoch(const String &p_session_id) const;
void clear();
```

语义：

- Store 当前是内存表； durable source of truth 仍是 `session.next.context.updated`，`AIContextEpochService` 会从投影/事件日志水合缺失的内存 epoch。
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
  -> Runner projects durable history
  -> ContextSourceRegistry.load(...)
  -> ContextEpochService.prepare(...)
  -> append session.next.prompt.promoted
  -> Projector marks promoted_seq
  -> Projector inserts user session_message
  -> SessionHistory exposes it to Runner
```

关键点：

- 首次或 replacement context 不可用时，不执行 prompt promotion。
- `session.next.context.updated` 必须早于首个 model-visible prompt。

### Multimodal Attachment

```text
UI/API submits attachments
  -> SessionService.prompt
  -> AttachmentResolver resolves data/path/blob/bytes into blob-backed AIFileAttachment
  -> append session.next.prompt.admitted without data URL/base64/local path
  -> Runner promotes prompt and reads projected AISessionMessage.files
  -> ModelPartBuilder checks provider modalities
  -> ImageNormalizer/FilePreprocessor prepares safe model parts
  -> Provider adapter maps AIModelPart to provider-specific request content
```

关键点：

- Session durable log 保存的是 typed attachment metadata 和 blob ref，不保存附件内容。
- Runner 构建 `step_started.request` 事件时必须 redaction 非文本 data、派生附件文本和 provider secret。
- Provider adapter 只做协议映射，不读取本地路径、不解析 UI 草稿附件。

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
  -> optional token-budget trimming without splitting tool content
  -> Runner builds AIModelRequest
```

## 当前实现边界

- `AIEventStore` 已提供 durable JSONL 存储，但不是完整数据库层。
- `AISessionProjector` 的 read model 当前在内存中，进程重启后应通过 `rebuild_from_store()` 重建。
- `AIContextEpochStore` 当前在内存中；恢复时由 `AIContextEpochService` 从 durable `context.updated` 投影水合，不单独作为 source of truth。
- Phase 7 的 server/core 附件链路已经建立：resolver/blob store/model part builder/provider mapper。UI chat draft attachment state 尚未在本层实现。
- Tool result media 复用为下一轮 multimodal input 尚未在本层实现；后续应复用同一 blob/part/capability pipeline。
- Session 创建、复用、删除、移动、title/model/cost 更新尚未在本层实现。
- Prompt exact retry conflict 尚未在本层强制执行，应由 Session service append 前检查。
- Permission events 在 domain 层只定义事件名和 durable 分类；具体权限状态机由 `editor/agent_v1/permission` 实现。

## 变更规则

修改 Domain API 前请先判断：

- 是否仍然是会话恢复所需的源头语义。
- 是否会破坏 durable cursor 单调性。
- 是否会把 live-only delta 误变成 replay source。
- 是否会让 Runner 绕过 projected history。
- 是否会让 tool settlement 退化为只按 callID 定位。
- 是否会把 Context Epoch 误建成 provider turn snapshot。
- 是否会把 `editor/agent_ui` 重新变成会话核心或 Agent 后端。

如果答案不明确，优先把新能力放到后续 `session`、`runtime`、`tools`、`permission` 或 `mcp` 子目录，而不是扩张 Domain 基础契约。
