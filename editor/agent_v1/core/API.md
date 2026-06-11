# Agent V1 Core API

本文档描述 `editor/agent_v1/core` 中已经建立的基础设施和运行时契约。它的目标是给后续复刻 opencode 架构时提供稳定参考：Provider、Runner、ToolRegistry、MCP、Skill、EventStore 等模块应优先依赖这里的中性类型，而不是直接依赖旧 `editor/ai_component` 的业务实现或某个具体 provider 的数据格式。

## 设计定位

`agent_v1/core` 分两层：

1. **基础设施层**
   - 取消令牌
   - HTTP/SSE 传输
   - 后台任务
   - 主线程派发

2. **Runtime Contract 层**
   - 统一错误/结果
   - 操作上下文
   - provider-neutral 模型请求
   - provider-neutral 流式事件
   - 流事件消费者
   - scoped registration 身份

它不负责：

- Session 入队/提升。
- EventStore 持久化。
- History projection。
- ToolRegistry 具体工具解析和执行。
- Permission 规则判断。
- MCP/Skill 的业务协议。
- 旧多 Agent 规划工作流。

## 目录结构

```text
editor/agent_v1/core
  base/
    ai_cancel_token.*
    ai_error.*
    ai_id.*
    ai_operation_context.*
  transport/
    ai_http_client.*
    ai_sse_parser.*
  threading/
    ai_task_runner.*
    ai_main_thread_dispatcher.*
  runtime/
    ai_model_request.*
    ai_stream_event.*
    ai_stream_sink.*
  registry/
    ai_scoped_registration.*
```

构建入口：

```python
# editor/SCsub
SConscript("agent_v1/core/SCsub")
```

## 全局约束

- Core 类型应保持 provider-neutral。
- Core 不应包含 Session/Tool/MCP/Skill 的业务分支。
- Provider adapter 只负责格式转换：把 provider 原生请求/响应转换到 Core contract。
- Runner 后续只消费 `AIModelRequest` 和 `AIStreamEvent`。
- 长任务必须支持 `AICancelToken`。
- 需要访问 Godot Editor 主线程对象时，必须通过主线程派发。
- scoped registration 必须用 identity/generation 防止 stale call。
- 错误返回应优先使用 `AIError` 或可转换到 `AIError` 的结构，而不是散落的裸 `String error`。

## base

### AICancelToken

文件：

```cpp
#include "editor/agent_v1/core/base/ai_cancel_token.h"
```

用途：

- 表示跨线程、跨层级共享的取消请求。
- 用于 HTTP 请求、后台任务、工具执行、MCP 调用、Provider stream。

主要 API：

```cpp
void request_cancel(const String &p_reason = String());
void clear_cancel();
bool is_cancel_requested() const;
String get_cancel_reason() const;
String get_cancel_message(const String &p_fallback = "Operation cancelled.") const;
```

使用约定：

- 长循环应周期性检查 `is_cancel_requested()`。
- 出错信息应使用 `get_cancel_message()`，保留上层取消原因。
- 取消不是普通失败；后续 Runner/Event 层应投射为 interrupted/cancelled 语义。

### AIError

文件：

```cpp
#include "editor/agent_v1/core/base/ai_error.h"
```

错误分类：

```cpp
AI_ERROR_NONE
AI_ERROR_CANCELLED
AI_ERROR_TIMEOUT
AI_ERROR_NETWORK
AI_ERROR_PROVIDER
AI_ERROR_PROTOCOL
AI_ERROR_VALIDATION
AI_ERROR_PERMISSION
AI_ERROR_INTERRUPTED
AI_ERROR_CONFLICT
AI_ERROR_UNAVAILABLE
AI_ERROR_INTERNAL
```

结构：

```cpp
struct AIError {
	AIErrorKind kind;
	String message;
	Dictionary details;
};
```

主要 API：

```cpp
bool is_error() const;
Dictionary to_dictionary() const;

static String kind_to_string(AIErrorKind p_kind);
static AIErrorKind string_to_kind(const String &p_kind);
static AIError none();
static AIError make(AIErrorKind p_kind, const String &p_message, const Dictionary &p_details = Dictionary());
```

使用约定：

- `details` 用于结构化诊断，例如 provider name、HTTP code、tool name、resource 等。
- 面向模型的错误投射应在更高层完成；Core 只保留事实。
- `AI_ERROR_INTERNAL` 应作为兜底，而不是常态。

### AIResult

结构：

```cpp
struct AIResult {
	bool success;
	Variant value;
	AIError error;
};
```

主要 API：

```cpp
bool is_ok() const;
Dictionary to_dictionary() const;

static AIResult ok(const Variant &p_value = Variant());
static AIResult fail(const AIError &p_error);
static AIResult fail(AIErrorKind p_kind, const String &p_message, const Dictionary &p_details = Dictionary());
```

使用约定：

- 对外暴露时可转为 `Dictionary`。
- 新的基础服务建议返回 `AIResult`，或至少保证能无损转换为 `AIResult`。
- 大型业务层可以定义更强类型结果，但错误侧应能映射到 `AIError`。

### AIId

文件：

```cpp
#include "editor/agent_v1/core/base/ai_id.h"
```

主要 API：

```cpp
static String make(const String &p_prefix = String());
static bool is_valid_name(const String &p_name, int p_max_length = 64);
```

用途：

- 生成 operation、request、registration 等基础 ID。
- 校验模型可见名称，例如工具名。

使用约定：

- `make("req")` 生成类似 `req_<hex>` 的 ID。
- `is_valid_name()` 采用工具名友好的规则：首字符字母，后续为字母、数字、`_`、`-`。
- 持久化层未来可以替换为更严格的 ID 服务，但调用方不应手写随机 ID。

### AIOperationContext

文件：

```cpp
#include "editor/agent_v1/core/base/ai_operation_context.h"
```

结构：

```cpp
struct AIOperationContext {
	String operation_id;
	String session_id;
	String agent_id;
	String location_key;
	int timeout_msec;
	Dictionary metadata;
	Ref<AICancelToken> cancel_token;
};
```

主要 API：

```cpp
bool is_cancel_requested() const;
String get_cancel_message(const String &p_fallback = "Operation cancelled.") const;
Ref<AICancelToken> ensure_cancel_token();
AIOperationContext make_child(const String &p_operation_prefix) const;
Dictionary to_dictionary() const;
```

用途：

- 统一一次可追踪操作的上下文。
- 传递 session、agent、location、timeout、cancel token、metadata。

使用约定：

- Provider turn、tool execution、MCP call、compaction call 都应能关联一个 `AIOperationContext`。
- 子操作使用 `make_child()`，继承 session/location/cancel token，但拥有新的 operation ID。
- 不要在 Core 内解释 `session_id` 的业务含义。

## transport

### AISSEParser

文件：

```cpp
#include "editor/agent_v1/core/transport/ai_sse_parser.h"
```

结构：

```cpp
struct AISSEEvent {
	String event;
	String data;
	String id;
	int retry_msec;
};
```

主要 API：

```cpp
void clear();
bool push_chunk(const PackedByteArray &p_chunk, Vector<AISSEEvent> &r_events);
bool finish(Vector<AISSEEvent> &r_events);

static bool extract_events_from_text(const String &p_text, bool p_require_complete_events, Vector<AISSEEvent> &r_events);
```

用途：

- 解析标准 SSE 数据块。
- 支持增量 chunk 消费。

使用约定：

- `push_chunk()` 只返回已经完整分隔的事件。
- 流结束后调用 `finish()` 处理 trailing buffer。
- Provider adapter 应把 `AISSEEvent::data` 继续解析成 provider 原生 JSON，再归一化为 `AIStreamEvent`。

### AIHTTPClient

文件：

```cpp
#include "editor/agent_v1/core/transport/ai_http_client.h"
```

请求结构：

```cpp
struct AIHTTPRequest {
	HTTPClient::Method method;
	String url;
	Vector<String> headers;
	PackedByteArray body;
	int timeout_msec;
	int poll_interval_usec;
	int max_response_body_bytes;
	bool fail_on_http_error;
	bool store_stream_body;
	String label;
	Ref<AICancelToken> cancel_token;
};
```

响应结构：

```cpp
struct AIHTTPResponse {
	bool headers_received;
	int response_code;
	Vector<String> headers;
	PackedByteArray body;
};
```

主要 API：

```cpp
bool request(const AIHTTPRequest &p_request, AIHTTPResponse &r_response, String &r_error) const;
bool request_sse(const AIHTTPRequest &p_request, const Callable &p_event_callback, AIHTTPResponse &r_response, String &r_error) const;

static String normalize_connection_host(const String &p_host);
static bool parse_endpoint(const String &p_url, AIHTTPEndpoint &r_endpoint, String &r_error);
static String join_paths(const String &p_base_path, const String &p_endpoint_path);
```

SSE callback 约定：

```cpp
// 参数：Dictionary event
// 返回 true 表示请求调用方主动停止继续读取。
Callable callback;
```

`event` 字段：

```text
event
data
id
retry_msec
```

使用约定：

- Provider、MCP HTTP 都应复用该类，不再各自手写 `HTTPClient` poll/read loop。
- `fail_on_http_error = true` 时，非 2xx 直接失败。
- `store_stream_body = false` 时，流式 body 只保留预览，不限制 SSE 解析完整 chunk。
- 取消时返回 `false`，错误消息来自 cancel token。

示例：

```cpp
Ref<AIHTTPClient> http;
http.instantiate();

AIHTTPRequest request;
request.method = HTTPClient::METHOD_POST;
request.url = "https://example.com/v1/chat/completions";
request.headers.push_back("Content-Type: application/json");
request.headers.push_back("Accept: text/event-stream");
request.body = body;
request.timeout_msec = context.timeout_msec;
request.cancel_token = context.cancel_token;

AIHTTPResponse response;
String error;
const bool ok = http->request_sse(request, event_callback, response, error);
```

## threading

### AITaskRunner

文件：

```cpp
#include "editor/agent_v1/core/threading/ai_task_runner.h"
```

用途：

- 运行一个后台任务。
- 统一传入 cancel token。
- 完成后发出 `task_finished` 信号。

主要 API：

```cpp
bool start(const Callable &p_task, const Variant &p_argument = Variant());
void cancel(const String &p_reason = String());
void wait_to_finish();
bool is_running() const;
Ref<AICancelToken> get_cancel_token() const;

bool was_last_successful() const;
Variant get_last_result() const;
String get_last_error() const;
```

任务 callable 签名：

```cpp
Variant task(Ref<AICancelToken> cancel_token, Variant argument);
```

使用约定：

- 任务内部必须检查 cancel token。
- `AITaskRunner` 适合较粗粒度后台工作，例如 discovery、model turn、compaction。
- 不适合大量短任务池；如果后续需要任务池，应另建基于该取消语义的 pool。

### AIMainThreadDispatcher

文件：

```cpp
#include "editor/agent_v1/core/threading/ai_main_thread_dispatcher.h"
```

用途：

- 从后台线程安全调度到 Godot 主线程。
- 访问 `EditorNode`、SceneTree、ResourceSaver 等主线程对象。

主要 API：

```cpp
static Error queue_call(const Callable &p_callable, const Array &p_arguments, uint64_t &r_item_id);
static void flush_pending_calls();
static bool dispatch_sync(
	const Callable &p_callable,
	const Array &p_arguments,
	Variant &r_result,
	String &r_error,
	const Ref<AICancelToken> &p_cancel_token = Ref<AICancelToken>(),
	int p_wait_usec = 1000
);
```

使用约定：

- 后台工具执行需要触碰编辑器对象时，使用 `dispatch_sync()`。
- 如果等待过程中 cancel token 被取消，并且 queued call 尚未执行，会尝试移除队列项并返回取消错误。
- 已经进入主线程执行的 call 不会被强制中断；主线程函数自己也应检查上层上下文。

## runtime

### AIModelPart

文件：

```cpp
#include "editor/agent_v1/core/runtime/ai_model_request.h"
```

类型：

```cpp
AI_MODEL_PART_TEXT
AI_MODEL_PART_IMAGE
AI_MODEL_PART_AUDIO
AI_MODEL_PART_FILE
```

结构：

```cpp
struct AIModelPart {
	AIModelPartType type;
	String text;
	String mime;
	String name;
	String data;
	Dictionary metadata;
};
```

用途：

- 表达 provider-neutral 消息片段。
- 文本、图片、音频、文件都通过同一结构进入 model request。

使用约定：

- 本地路径不应直接写进 `text` 发给云模型。
- 图片/音频/文件应由更高层 resolver 转为安全的 data/blob/provider file reference。
- Provider adapter 再把 `AIModelPart` 降级为 OpenAI/Anthropic/Gemini 等格式。

### AIModelMessage

结构：

```cpp
struct AIModelMessage {
	String id;
	String role;
	Vector<AIModelPart> parts;
	Dictionary metadata;
};
```

使用约定：

- `role` 使用 provider-neutral 字符串，如 `system`、`user`、`assistant`、`tool`。
- `metadata` 可携带 provider continuation 必需信息，但 Runner 不能依赖某个 provider 专有字段。

### AIModelToolDefinition

结构：

```cpp
struct AIModelToolDefinition {
	String name;
	String description;
	Dictionary input_schema;
	Dictionary metadata;
};
```

主要 API：

```cpp
bool is_valid() const;
Dictionary to_dictionary() const;
```

使用约定：

- `name` 必须满足 `AIId::is_valid_name()`。
- ToolRegistry materialize 时生成该结构。
- 工具执行器本体不在这里；这里仅是模型可见定义。

### AIModelRequest

结构：

```cpp
struct AIModelRequest {
	String request_id;
	String provider;
	String model;
	Vector<AIModelPart> system;
	Vector<AIModelMessage> messages;
	Vector<AIModelToolDefinition> tools;
	Dictionary provider_options;
	Dictionary metadata;
	int max_output_tokens;
	bool stream;
};
```

使用约定：

- 一次 provider turn 对应一个 `AIModelRequest`。
- Runner 后续应保证一个 provider turn 只调用一次 `llm.stream(request)`。
- `provider_options` 可放 provider 专有参数，但不能泄漏到 ToolRegistry/EventStore 的核心语义里。

### AIStreamEvent

文件：

```cpp
#include "editor/agent_v1/core/runtime/ai_stream_event.h"
```

事件类型：

```cpp
AI_STREAM_EVENT_STEP_START
AI_STREAM_EVENT_STEP_END
AI_STREAM_EVENT_TEXT_START
AI_STREAM_EVENT_TEXT_DELTA
AI_STREAM_EVENT_TEXT_END
AI_STREAM_EVENT_REASONING_START
AI_STREAM_EVENT_REASONING_DELTA
AI_STREAM_EVENT_REASONING_END
AI_STREAM_EVENT_TOOL_INPUT_START
AI_STREAM_EVENT_TOOL_INPUT_DELTA
AI_STREAM_EVENT_TOOL_INPUT_END
AI_STREAM_EVENT_TOOL_CALL
AI_STREAM_EVENT_TOOL_RESULT
AI_STREAM_EVENT_TOOL_ERROR
AI_STREAM_EVENT_PROVIDER_ERROR
```

结构：

```cpp
struct AIStreamEvent {
	AIStreamEventType type;
	String id;
	String name;
	String text;
	Variant input;
	Variant result;
	AIError error;
	bool provider_executed;
	Dictionary provider_metadata;
	Dictionary metadata;
};
```

使用约定：

- Provider adapter 把原生增量转换为 `AIStreamEvent`。
- Runner 只处理 `AIStreamEvent`，不直接解析 provider 原生 JSON。
- `*_DELTA` 后续应被视为 live-only；可持久化的是 `*_END`、tool success/failed 等边界事件。
- `provider_executed = true` 表示工具由 provider 执行，不进入本地 ToolRegistry settlement。

### AIStreamSink

文件：

```cpp
#include "editor/agent_v1/core/runtime/ai_stream_sink.h"
```

用途：

- 表示流事件消费者。
- Provider runtime 可写入 sink。
- Runner/Event publisher/UI bridge 可实现自己的 sink。

主要 API：

```cpp
virtual bool push_event(const AIStreamEvent &p_event, bool &r_stop_requested, String &r_error);
```

### AICallableStreamSink

用途：

- 把 Godot `Callable` 适配成 `AIStreamSink`。

主要 API：

```cpp
void set_callback(const Callable &p_callback);
Callable get_callback() const;
```

callback 约定：

```cpp
// 参数：Dictionary event
// 返回 true 表示请求停止继续消费。
Variant callback(Dictionary event);
```

## registry

### AIRegistrationIdentity

文件：

```cpp
#include "editor/agent_v1/core/registry/ai_scoped_registration.h"
```

结构：

```cpp
struct AIRegistrationIdentity {
	String id;
	String name;
	String owner;
	uint64_t generation;
	Dictionary metadata;
};
```

主要 API：

```cpp
bool is_valid() const;
bool matches(const AIRegistrationIdentity &p_other) const;
Dictionary to_dictionary() const;
static AIRegistrationIdentity from_dictionary(const Dictionary &p_dict);
```

用途：

- ToolRegistry、ContextSourceRegistry、MCP/Skill dynamic registration 都应使用类似 identity。
- materialize 时捕获 identity。
- settle/resolve 时比较 identity，防止模型调用已经被替换的旧工具。

### AIScopedRegistration

用途：

- RAII 风格的注册生命周期。
- 析构或显式 `close()` 时调用关闭回调。

主要 API：

```cpp
void setup(const AIRegistrationIdentity &p_identity, const Callable &p_close_callback);
void close();
bool is_closed() const;
AIRegistrationIdentity get_identity() const;
Dictionary get_identity_dict() const;
```

close callback 约定：

```cpp
void close(Dictionary identity);
```

使用约定：

- Registry 注册时返回 `Ref<AIScopedRegistration>`。
- 调用方释放 scope 后，registry 应移除对应 identity。
- 如果同名注册有栈式覆盖关系，关闭当前 scope 后应恢复前一个注册。

## 推荐调用链

### Provider Streaming

```text
Runner
  -> build AIModelRequest
  -> provider adapter lowers request to provider-native body
  -> AIHTTPClient.request_sse(...)
  -> AISSEParser emits AISSEEvent
  -> provider adapter converts AISSEEvent.data to AIStreamEvent
  -> AIStreamSink.push_event(...)
  -> Runner publishes durable/live events
```

关键约束：

- Provider adapter 不执行工具。
- Provider adapter 不决定权限。
- Provider adapter 不直接写 Session event log。
- 一个 provider turn 只发出一个模型请求。

### Tool Materialization

```text
ToolRegistry
  -> active registrations
  -> filter wholly denied tools by permission rules
  -> materialize AIModelToolDefinition[]
  -> capture AIRegistrationIdentity + Location root snapshot
  -> Runner sends AIModelRequest
  -> AI_STREAM_EVENT_TOOL_CALL
  -> ToolRegistry compares captured identity
  -> decode / permission / execute / output bounding
```

关键约束：

- 工具定义和工具执行分离。
- 模型可见 name 来自注册名，不来自工具对象自身。
- Location root 必须在 materialize 时固定，不能在 settlement 时读取可变全局状态。
- `resource="*"` 且 `effect="deny"` 的工具不应暴露给模型。
- stale call 必须返回模型可见错误，不能执行新 handler。

### Main Thread Tool Work

```text
Tool execute background thread
  -> needs EditorNode/SceneTree
  -> AIMainThreadDispatcher.dispatch_sync(...)
  -> main thread mutates/reads editor state
  -> result returns to tool execution
```

关键约束：

- 后台线程不要直接触碰编辑器主线程对象。
- 可取消等待，不等于可强杀已经进入主线程的操作。

## 后续模块接入建议

### LLM Runtime

应新增在 `editor/agent_v1/runtime` 或类似目录，而不是塞回 `core`。

依赖：

- `AIModelRequest`
- `AIStreamEvent`
- `AIStreamSink`
- `AIOperationContext`
- `AIHTTPClient`

产出：

- `stream(request, sink, context)`。

### EventStore / Session

应新增在 `editor/agent_v1/session` 或类似目录。

可以引用：

- `AIStreamEvent` 的类型枚举作为 provider turn 输入。
- `AIError` 分类。
- `AIId` 生成 ID。

不要把 EventStore 写进 `core`。

### ToolRegistry

应新增在 `editor/agent_v1/tools`。

可以引用：

- `AIModelToolDefinition`
- `AIRegistrationIdentity`
- `AIScopedRegistration`
- `AIError`
- `AIOperationContext`

ToolRegistry 当前基础能力：

- name validation
- scoped overlay
- materialization snapshot
- stale rejection
- input/output codec boundary
- permission assert

ToolRegistry 后续仍需完善：

- output bounding

### MCP

应新增在 `editor/agent_v1/mcp`。

可以引用：

- `AIHTTPClient`
- `AITaskRunner`
- `AIModelToolDefinition`
- `AIScopedRegistration`
- `AIOperationContext`

MCP tool 最终应变成普通 ToolRegistry 注册，不应有绕开工具/权限/事件的特殊通道。

## 变更规则

修改 Core API 前请先判断：

- 是否被多个后续模块共享？
- 是否仍然 provider-neutral？
- 是否不包含 Session/Tool/MCP/Skill 业务细节？
- 是否能在不中断旧 `ai_component` 的前提下演进？
- 是否能通过 `scons platform=windows target=editor dev_build=yes -j8`？

如果答案不明确，优先把实现放到 `agent_v1` 的业务子目录，而不是扩大 `core`。
