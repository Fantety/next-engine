# NEXT Engine AI Agent 架构说明

本文档描述当前 NEXT Engine 中 AI Agent 的实现边界、核心模块、消息流程、工具调用流程和后续扩展方向。范围主要是 `editor/ai_component` 以及少量 Godot 编辑器入口修改。

## 目标

当前实现的目标是把 AI Coding Agent 作为编辑器内置能力接入 Godot 编辑器，而不是做一个外挂脚本工具。核心原则是：

- AI 交互在编辑器 Dock 中完成。
- 模型、MCP、会话、工具、上下文、Review/Diff 各自独立成模块。
- 编辑类工具尽量调用 Godot 编辑器内部 API，不直接绕过编辑器状态修改场景。
- 工具权限由 Agent Profile 控制，Ask / Review / Write 模式有不同边界。
- 流式响应和工具执行进度需要实时更新到消息列表。
- 文件改动通过 Change Set 记录，支持查看 diff、保留、撤销。

## 编辑器入口

AI 功能在编辑器启动时注册和挂载。

- `editor/register_editor_types.cpp`
  - 注册 `AIAgentSession`、`AIContextManager`、`AIChangeSetStore`、`AIToolExecutionContext`、`AIAgentSettingsDialog`、`AIMarkdownLabel`、`AITextDiffViewer` 等类型。

- `editor/editor_node.cpp`
  - 创建 `AIAgentSettingsDialog`。
  - 创建并挂载 `AIAgentDock` 到编辑器 Dock 区域。

- `editor/ai_component/SCsub`
  - 汇总构建子模块：`agent`、`providers`、`context`、`review`、`storage`、`prompts`、`tools`、`ui`。

## UI 层

UI 层只负责用户交互、展示状态和转发命令，不直接执行 Agent 逻辑。

- `editor/ai_component/ui/ai_agent_dock.*`
  - AI Dock 主界面。
  - 管理 session 下拉列表、新建/删除 session、消息列表、变更 Review 面板、token 用量、请求进度条、输入区。
  - 持有一个 `AIAgentSession` 节点。
  - 监听 session 的消息新增、消息更新、状态变化、token 用量变化、工具审批请求。

- `editor/ai_component/ui/ai_composer.*`
  - 输入框、模式选择、模型选择、发送/取消按钮。
  - 发送按钮和取消按钮复用同一个按钮：
    - 空输入时禁用。
    - 请求中显示 Stop 图标并触发 cancel。
  - 输入框使用自动换行，避免长文本产生横向滚动。

- `editor/ai_component/ui/ai_message_list.*`
  - 消息列表和滚动控制。
  - 当滚动条本来在底部时，新消息自动滚到底部；用户主动上滚时不强行打断。

- `editor/ai_component/ui/ai_message_bubble.*`
  - 单条消息气泡。
  - 支持 user、assistant、tool、error 等角色展示。
  - tool 气泡默认折叠长内容，可展开查看详情。

- `editor/ai_component/ui/ai_markdown_label.*`
  - 消息 Markdown 展示控件。
  - 使用 `AIMarkdownRenderer` 渲染 Markdown 到 `RichTextLabel`。

- `editor/ai_component/ui/ai_markdown_renderer.*`
  - Markdown AST 到富文本的转换逻辑。
  - 处理标题、列表、代码块、表格等基础展示。

- `editor/ai_component/ui/ai_change_review_panel.*`
  - AI 改动列表。
  - 每个 pending change set 提供查看 diff、保留、撤销按钮。

- `editor/ai_component/ui/ai_text_diff_viewer.*`
  - Diff 弹窗。
  - 左右对比旧内容和新内容。
  - 对 GDScript / GDShader 使用语法高亮。
  - 增加内容用绿色标识，删除内容用红色标识。

- `editor/ai_component/ui/ai_agent_settings_dialog.*`
  - AI 设置对话框。
  - 一级侧边栏包含模型、MCP、技能、规则。

- `editor/ai_component/ui/ai_settings_models_page.*`
  - 模型配置页面。
  - 支持添加、编辑、删除模型配置。
  - 同一个供应商和模型可以创建多个不同配置，由用户设置名称。

- `editor/ai_component/ui/ai_settings_mcp_page.*`
  - MCP Server 配置页面。
  - 支持添加、编辑、删除、启用和关闭 MCP Server。

## 会话层

会话层是 UI 和 Runtime 之间的协调者。

- `editor/ai_component/agent/ai_agent_session.*`
  - 维护当前会话 ID、标题、消息列表、状态、token 统计、pending tool approval。
  - 配置 provider、agent profile、tool registry、context providers。
  - 启动 `AIAgentRuntimeRunner` 在线程中执行 Agent Runtime。
  - 把 Runtime 的实时消息事件映射回本地消息列表。
  - 将会话持久化到 `AIConversationStore`。

关键流程：

1. `AIAgentDock::_send_requested()` 收到 UI 发送事件。
2. Dock 根据模型 ID 获取 `AIProviderConfig`，调用 `AIAgentSession::configure_provider()`。
3. Dock 设置当前 Agent Profile，例如 `plan`、`review`、`write`。
4. Dock 调用 `AIAgentSession::send_user_message()`。
5. Session 写入 user 消息，创建 assistant placeholder，保存会话。
6. Session 收集上下文，启动 Runtime Runner。
7. Runtime Runner 在线程中执行 `AIAgentRuntime::run()`。
8. Runtime 通过回调实时通知消息新增和更新。
9. Session 将 Runtime 消息映射到本地消息列表并通知 Dock 刷新 UI。
10. Runtime 结束后，Session 应用结果、保存会话、更新 token 用量和状态。

## Runtime 层

Runtime 是 Agent 的核心循环，负责模型调用、工具调用、权限判断和多轮执行。

- `editor/ai_component/agent/ai_agent_runtime.*`
  - 输入：历史消息、上下文文档、工具注册表、Agent Profile。
  - 输出：运行结果、assistant 消息、tool 消息、tool calls、metadata、pending approval。
  - 默认支持较高的 provider turn 和 tool call 次数上限。
  - 使用 function calling，而不是纯 ReAct 文本解析。

- `editor/ai_component/agent/ai_agent_runtime_runner.*`
  - 在线程中运行 Runtime，避免阻塞编辑器 UI。
  - 转发 runtime message added / updated 事件。

Runtime 主循环：

1. 从 `AIContextManager` 构建 provider messages。
2. 从 `AIToolRegistry` 生成 OpenAI-compatible tool schemas。
3. 调用 `AIAgentRuntimeClient::complete_streaming()`。
4. 流式内容通过 `_on_provider_partial_response()` 更新 assistant 消息。
5. 如果模型没有 tool calls，结束本轮。
6. 如果模型返回 tool calls，创建 assistant tool-call 消息。
7. 对每个 tool call 调用 `AIToolPermissionPolicy::evaluate()`。
8. 权限为 deny 时，生成 tool denied 消息并继续。
9. 权限为 ask 时，返回 pending approval，Session 进入 `WAITING_TOOL_APPROVAL`。
10. 权限为 allow 时，设置 `AIToolExecutionContext`，执行工具。
11. 工具结果转成 tool 消息实时加入 message list。
12. 继续下一轮 provider turn，让 LLM 读取工具结果并决定下一步。

## Provider 层

Provider 层负责把内部消息和工具 schema 转成 OpenAI-compatible 请求，并解析模型响应。

- `editor/ai_component/providers/ai_provider_config.h`
  - provider 名称、base URL、API key、model、timeout。

- `editor/ai_component/providers/ai_model_settings.*`
  - 模型配置存取。
  - 支持 provider preset、自定义模型、多个 profile。
  - `get_provider_config()` 根据模型 profile 生成请求配置。

- `editor/ai_component/providers/ai_openai_runtime_client.*`
  - Runtime 使用的 OpenAI-compatible client。
  - 负责：
    - 内部 tool name 和 provider tool name 的映射。
    - 内部消息到 OpenAI chat messages 的转换。
    - reasoning_content 的回传。
    - 非流式请求和流式 SSE 请求。
    - tool_calls 和 usage metadata 的解析。

- `editor/ai_component/providers/ai_openai_compatible_codec.*`
  - OpenAI-compatible 编解码辅助逻辑，主要服务测试和结构化转换。

## 上下文管理

上下文由 provider 收集、`AIContextManager` 裁剪并组装。

- `editor/ai_component/context/ai_context_provider.*`
  - 上下文 provider 基类。

- `editor/ai_component/context/ai_context_document.h`
  - 单个上下文文档结构：`title`、`source`、`content`、`truncated`。

- `editor/ai_component/context/ai_editor_context_provider.*`
  - 当前编辑器和项目的基础信息。

- `editor/ai_component/context/ai_project_tree_context_provider.*`
  - 当前项目的目录树摘要。

- `editor/ai_component/context/ai_best_practices_context_provider.*`
  - 注入 Agent 最佳实践。
  - 内容来自 `editor/ai_component/agent/best_practices.md`，当前实现会作为默认上下文进入 prompt。

- `editor/ai_component/agent/ai_context_manager.*`
  - 组装 system prompt、只读上下文和历史消息。
  - 当前用字符数估算上下文预算：
    - `max_input_chars = 96000`
    - `max_context_chars = 24000`
    - `max_history_chars = 64000`
    - `max_tool_result_chars = 16000`
    - `min_recent_messages = 4`
  - tool result 会单独裁剪，避免巨大工具输出拖垮上下文。
  - 历史消息按 assistant tool-call + tool result 分组处理，避免保留了 tool result 却丢失对应 tool call。
  - metadata 会记录 estimated input chars、omitted history messages、truncated tool results、truncated context documents。

## Agent Profile 和权限

权限由 `AIAgentProfile` 和 `AIToolPermissionPolicy` 控制。

- `editor/ai_component/agent/ai_agent_profile.*`
  - `plan`：只读工具。
  - `write`：只读工具 + 场景编辑 + 脚本编辑 + shader + 创建文件夹。
  - `review`：基于 write，但写文件工具会通过 Review Change Set 记录可撤销改动。
  - `build`：当前也是只读工具边界。

- `editor/ai_component/tools/ai_tool_permission.*`
  - 判断工具调用是 allow、ask 还是 deny。
  - `script.delete` 默认 ask，需要用户审批。

- `editor/ai_component/tools/ai_tool_execution_context.*`
  - thread-local 工具执行上下文。
  - 包含 session id、profile id、tool call id。
  - 工具可通过 `is_review_mode()` 判断是否进入 review 行为。

## 工具系统

工具统一继承 `AITool`。

- `editor/ai_component/tools/ai_tool.*`
  - 工具基类。
  - 工具必须提供 name、description、parameters schema、execute。
  - `get_openai_schema()` 转换为 function calling schema。

- `editor/ai_component/tools/ai_tool_registry.*`
  - 注册和查询工具。
  - Runtime 每次执行前从 registry 获取 tool schemas。

- `editor/ai_component/tools/ai_tool_call.*`
  - tool call 结构和状态。
  - 状态包括 pending、running、completed、denied、failed。

当前内置工具分组：

- Project 工具
  - `project.list_tree`
  - `project.read_file`
  - `project.search_text`
  - `project.create_folder`

- Editor 工具
  - `editor.get_context`
  - `docs.search`

- Scene 工具
  - `scene.describe_tree`
  - `scene.inspect_node`
  - `scene.list_properties`
  - `scene.apply_patch`

- Script 工具
  - `script.inspect`
  - `script.create`
  - `script.write`
  - `script.patch_function`
  - `script.bind_to_node`
  - `script.unbind_from_node`
  - `script.delete`

- Shader 工具
  - `shader.apply_to_node`

## 场景编辑工具

场景工具通过 `AISceneEditingService` 统一封装。

- `editor/ai_component/tools/editor/ai_scene_editing_service.*`
  - 在主线程中操作 Godot 编辑器场景。
  - `scene.describe_tree` 返回当前编辑场景的紧凑树结构，包含相对路径、名称、类型、子节点数量和 `ai_node_id`。
  - `scene.inspect_node` 返回指定节点当前真实属性值、值类型、声明类型、hint、Resource 信息和常用子属性路径；省略属性列表时会返回布局、可见性、文本、位置等常用验收字段，用于 `scene.apply_patch` 后复查。
  - `scene.list_properties` 会列出节点可编辑属性、类型、hint、Resource 预期、当前值预览和可用于 patch 的属性路径。
  - `scene.apply_patch` 是唯一公开的场景写工具，支持创建场景、添加/实例化节点、删除节点、重命名、移动和批量设置属性。
  - `scene.apply_patch` 接收批量 patch IR，但执行时仍通过 Godot 编辑器内部 API 创建节点、设置属性、保存场景，不直接写 `.tscn` 文本。
  - Patch 执行前会校验节点类型、场景路径、patch id、节点引用、属性存在性、只读属性、属性类型转换和 Resource 类型约束；失败时返回 `ops[index]` 或属性路径级别的精确错误，并在属性不存在时给出相近的可写属性候选。
  - Resource-backed 属性支持 `null`、`{"resource_path":"res://..."}`、`{"resource_type":"Type","properties":{...}}`。例如 `CollisionShape2D.shape` 可直接设置为 `{"resource_type":"RectangleShape2D","properties":{"size":{"type":"Vector2","args":[64,32]}}}`；`shape:size` 这类嵌套路径只适用于 `shape` 已存在的情况。
  - 修改后保存场景。

设计注意：

- 编辑场景必须走 Godot 编辑器 API。
- 不直接把场景文件当纯文本改写。
- Agent 需要先用 `scene.describe_tree` 定位节点，用 `scene.inspect_node` 查看目标节点当前状态，再用 `scene.list_properties` 获取真实属性路径和类型，最后用 `scene.apply_patch` 提交批量修改；涉及布局、位置、文本、可见性、结构变化时，提交后必须再次用 `scene.inspect_node` 或 `scene.describe_tree` 核对结果再回复完成。
- 固定 Editor Context 和 `editor.get_context` 都会提供 `display.project_viewport_size`、当前编辑器 2D viewport、编辑器窗口尺寸和当前场景根节点信息；Agent 在调整 Control/Node2D 位置时应优先使用 `project_viewport_size` 作为运行时窗口坐标范围。
- `docs.search` 通过 Godot `EditorHelp` / `DocData` 文档系统返回结构化类、属性、方法、信号、常量、枚举和主题项信息，可用于确认 Godot API 名称，也被场景工具复用来生成错误候选提示。
- Resource 类型属性需要通过工具 schema 和 `scene.list_properties` 给 LLM 足够信息。

## 脚本和 Shader 工具

- `editor/ai_component/tools/editor/ai_script_editing_service.*`
  - 创建、写入、删除脚本。
  - 绑定和解绑节点脚本。
  - 基于 GDScript 解析能力做函数级定位和局部替换。
  - Review 模式下把文本改动注册到 `AIChangeSetStore`。

- `editor/ai_component/tools/editor/ai_shader_editing_service.*`
  - 创建或更新 `.gdshader`。
  - 创建 `ShaderMaterial` 并绑定到节点指定 material 属性。
  - 保存当前场景。
  - Review 模式下记录 shader 文件改动。

## Review / Diff 架构

Review 模式不阻塞工具执行，而是记录用户可检查、可撤销的改动。

- `editor/ai_component/review/ai_change_set_store.*`
  - 维护 pending / kept / reverted change set。
  - 对同一 project、session、path 的多次改动进行合并。
  - 同一文件多次修改时，最终展示原始 old_text 和最新 new_text 的 diff。
  - `keep_change_set()` 表示接受当前文件状态。
  - `revert_change_set()` 会校验文件当前内容仍等于 new_text，只有安全时才恢复 old_text。

- `editor/ai_component/review/ai_diff_service.*`
  - 构建文本改动对象。
  - 生成 unified diff。
  - 统计 added / removed 行数。

文件 change 的核心字段：

- `path`：项目内资源路径。
- `type`：`create`、`modify`、`delete`。
- `language`：例如 `gdscript`、`gdshader`。
- `old_text`：AI 修改前内容。
- `new_text`：AI 修改后内容。
- `diff`：unified diff 文本。
- `added_lines` / `removed_lines`：行数统计。
- `metadata`：工具、session、合并状态等扩展信息。

## MCP 当前实现

MCP 已经作为可选工具来源接入，但仍应视为基础实现。

- `editor/ai_component/providers/ai_mcp_settings.*`
  - MCP Server 配置存储。
  - 字段包括 display name、command、arguments、working directory、environment、enabled。

- `editor/ai_component/providers/ai_mcp_stdio_client.*`
  - 通过 stdio 启动 MCP server。
  - 支持 initialize、tools/list、tools/call。

- `editor/ai_component/providers/ai_mcp_protocol.*`
  - JSON-RPC 请求构造和响应解析。
  - 将 MCP tool name 映射为 Agent 可用 tool name。

- `editor/ai_component/tools/ai_mcp_tool.*`
  - 把 MCP tool 包装成 `AITool`，统一进入 tool registry。

Session 启动时会读取启用的 MCP Server，列出工具并注册到 registry。MCP 工具默认走动态权限策略，目前名称以 `mcp_` 开头的工具会纳入当前 profile 可调用集合。

## AgentSkill 当前实现

AgentSkill 作为独立模块接入，首版只支持 Prompt/Context Skill，不执行代码，也不自动授予工具权限。

- `editor/ai_component/skills/ai_skill_settings.*`
  - 使用 `EditorSettings` 的 `ai_agent/skills` 存储 Skill 配置。
  - 字段包括 id、display name、description、content、kind、enabled。
  - 当前可用 kind 为 `prompt_context`，保留 kind 字段用于后续扩展。

- `editor/ai_component/skills/ai_skill_context_provider.*`
  - 收集启用 Skill 的索引上下文。
  - 只注入 skill id、名称、描述和安全边界说明，不默认注入完整内容。

- `editor/ai_component/skills/ai_activate_skill_tool.*`
  - 提供只读工具 `agent.activate_skill`。
  - Agent 根据上下文中的 skill id 主动调用该工具后，才会读取完整 Skill 内容。
  - 工具会拒绝缺失、禁用或非 `prompt_context` 的 Skill。

- `editor/ai_component/ui/ai_settings_skills_page.*`
  - AI Settings 中的 Skill 配置页。
  - 支持添加、编辑、删除、启用和禁用 Prompt/Context Skill。

Skill 的安全边界是：当前只提供 prompt/context 指令；不执行脚本，不启动外部进程，不读取任意 bundled resources，不绕过 `AIAgentProfile`、`AIToolPermissionPolicy`、MCP 审批或 Review/Diff 流程。后续如果支持 tool bundle、资源引用或 executable skill，需要新增独立的权限模型和显式用户授权。

## 会话和存储

- `editor/ai_component/storage/ai_conversation_store.*`
  - 按项目 scope 隔离会话。
  - 保存会话标题、消息、时间等 metadata。
  - 支持列出、加载、删除、查找最近会话。

- `editor/ai_component/storage/ai_conversation_serializer.*`
  - 会话序列化和反序列化。

项目隔离逻辑：

- Session 会根据当前项目路径生成 project scope key。
- 存储路径使用 `user://ai_agent/projects/<scope>/conversations`。
- 不同项目的对话列表互不展示。
- 打开项目后默认加载最近一次会话。

## Token 和用量展示

当前 token 统计分两类：

- provider usage：模型 API 返回的真实 usage metadata。
- estimated context usage：`AIContextManager` 基于字符数估算的上下文用量。

`AIAgentSession::_recalculate_token_usage()` 汇总消息 metadata 中的 usage，并通知 Dock 刷新 `token_usage_label`。当前不是严格 tokenizer 级别统计，后续可接入模型 tokenizer 或 provider-specific tokenizer。

## 一次完整请求的工作流

1. 用户在 `AIComposer` 输入消息，选择模型和模式。
2. 用户点击 Send。
3. `AIComposer` 发出 `send_requested`。
4. `AIAgentDock` 配置 provider 和 agent profile。
5. `AIAgentSession` 接收用户消息，保存会话，进入 preparing context。
6. Session 收集 editor/project/best-practices 上下文。
7. Session 启动 `AIAgentRuntimeRunner`。
8. Runner 在线程中执行 `AIAgentRuntime::run()`。
9. Runtime 通过 `AIContextManager` 构建 provider messages。
10. Runtime 通过 `AIOpenAICompatibleRuntimeClient` 发起 streaming chat completion。
11. 流式 assistant 文本实时回传到 `AIMessageList`。
12. 如果模型返回 tool calls，Runtime 创建 tool-call 消息。
13. Runtime 根据当前 profile 判断工具权限。
14. allow 的工具直接执行，ask 的工具暂停等待用户确认，deny 的工具返回拒绝结果。
15. 工具结果作为 tool message 实时加入消息列表。
16. Runtime 把工具结果继续交给模型进行下一轮 provider turn。
17. 当模型给出最终回答且没有新的 tool calls，Runtime 结束。
18. Session 应用结果、保存会话、刷新 token 和状态。
19. Dock 隐藏 loading 条，按钮回到 Send 状态。

## 后续维护建议

- 新增工具时：
  - 新建 `AITool` 子类。
  - 提供明确 JSON schema。
  - 在 `AIAgentSession::_configure_tool_runtime()` 注册。
  - 在 `AIAgentProfile` 中明确权限边界。
  - 如果会写文件或改场景，优先封装到 service，不要在 tool 类里堆业务。

- 新增 UI 页面时：
  - 保持 `AIAgentSettingsDialog` 只负责导航。
  - 页面逻辑拆到独立 page 组件。

- 新增 Provider 时：
  - 优先实现新的 runtime client 或 transport。
  - 保持 Runtime 不感知具体 provider 协议。

- 强化上下文时：
  - 新增 `AIContextProvider`，让 Session 收集。
  - 控制文档大小，并在 metadata 中标记是否截断。

- 强化 Review 时：
  - 文件级改动继续进入 `AIChangeSetStore`。
  - 场景二进制或资源级变更需要设计 Resource-aware diff，而不是直接文本 diff。
