# NEXT 面板集成 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在顶部运行栏加入 NEXT 开关；开关打开后，现有 `AIAgentDock` 从聊天面板切换为 NEXT 面板，同时保留 `AIAgentNextDock` 作为 NEXT 专用容器，并保持 NEXT 的会话、状态机和 Agent 编排独立于现有 AI 对话流程。

**Architecture:** `EditorRunBar` 只负责显示 NEXT 开关并发出切换信号；`AIAgentDock` 成为两套视图的宿主：关闭 NEXT 时显示现有聊天 UI，开启 NEXT 时显示 `AIAgentNextDock`。`AIAgentNextDock` 是 NEXT 专用 UI 容器，内部持有 `AIAgentNextSession` 和 `AINextPanel`，但不作为第二个独立 Dock tab 注册。NEXT 逻辑由新增 `AIAgentNextSession` 驱动，它不继承、不复用 `AIAgentSession`，也不改 `AIMainAgent`；只复用底层 `AIAgentBase`、`AIAgentRuntime`、工具循环、权限系统、模型配置、已有编辑器工具和 provider 链路。

**Tech Stack:** NEXT Engine / Godot-derived C++ editor module, `EditorRunBar`, existing `AIAgentDock`, Godot GUI controls, `RefCounted`/`Node` domain model, existing AI runtime/tool/permission/model infrastructure, SCons editor tests.

---

## 1. 最新边界

这版按最新澄清修正：不是新增第二个 Dock，而是现有 `AIAgentDock` 在 NEXT 开关打开后变成 NEXT 面板。

必须遵守：

- 顶部 `NEXT` 开关放在 `EditorRunBar`，位置在主运行按钮 `play_button` 左侧。
- `AIAgentDock` 可以做外壳级改造：增加 `chat_root` 和 `next_dock` 两个视图容器，并根据开关切换显示。
- `AIAgentDock` 的现有聊天逻辑不重写：原 `AIAgentSession`、消息列表、composer、change review、MCP/Skill 状态仍服务聊天模式。
- NEXT 不使用 `AIAgentSession`。
- NEXT 不改 `AIMainAgent`，当前主 Agent 继续只服务 AI 对话框。
- NEXT 新增 `AIAgentNextSession`，自己管理 Brief、里程碑、任务、执行、反馈、锁定。
- NEXT 使用 `AIAgentBase` 创建 planning/script/scene/shader/review agent 实例。
- NEXT 保留 `AIAgentNextDock`，它是 `AIAgentDock` 内部显示的 NEXT 专用容器。

允许修改：

- `editor/run/editor_run_bar.*`: 加顶部 NEXT 开关和信号。
- `editor/ai_component/ui/ai_agent_dock.*`: 外壳级视图切换和 NEXT panel 挂载。
- `editor/ai_component/ui/SCsub`: 编译新增 NEXT UI 控件。
- `editor/ai_component/SCsub`: 编译新增 NEXT 领域层。
- `editor/register_editor_types.cpp`: 注册新增 NEXT 类型。

禁止修改：

- `editor/ai_component/agent/ai_agent_session.*`
- `editor/ai_component/agent/ai_main_agent.*`
- `editor/ai_component/planning/ai_plan_manager.*`
- `editor/ai_component/planning/ai_manage_plan_tool.*`

## 2. 用户体验

### 2.1 顶部入口

`NEXT` 开关在引擎顶部右侧运行按钮组内，位于主运行按钮左边：

```text
┌──────────────────────────────────────────────────────────────┐
│ ...                                      [NEXT] [▶] [Ⅱ] [■] │
└──────────────────────────────────────────────────────────────┘
```

交互：

- 默认关闭，`AIAgentDock` 显示现有聊天 UI。
- 打开后，`AIAgentDock` 保持同一个 Dock tab，但内容切换为 NEXT 面板。
- 关闭后，`AIAgentDock` 恢复聊天 UI，聊天会话不丢失。
- NEXT 内部状态保留，重新打开后恢复里程碑和任务。
- 编辑器重启后恢复项目级 NEXT 开关状态。

### 2.2 AIAgentDock 内部视图

`AIAgentDock` 作为宿主后，结构从单一路径变成两个 sibling view：

```text
AIAgentDock
├── chat_root       // 现有 session bar + change review + message list + composer
└── next_dock       // 新增 AIAgentNextDock，默认 hidden
```

聊天模式：

```text
┌──────────────────────────────────────────────┐
│ Session selector / New / Delete / MCP / Skill│
├──────────────────────────────────────────────┤
│ Change Review                                │
│ Message List                                 │
│ Token Usage                                  │
│ Composer                                     │
└──────────────────────────────────────────────┘
```

NEXT 模式：

```text
┌──────────────────────────────────────────────┐
│ NEXT                                         │
├──────────────────────────────────────────────┤
│ Brief → Plan → Execute → Test → Lock         │
├──────────────────────────────────────────────┤
│ Brief                                        │
│ 做一个 2D 角色移动原型...                   │
│ [Generate Milestones]                        │
├──────────────────────────────────────────────┤
│ Active Milestone                             │
│ 01 Core Movement                  3 / 7      │
│ █████████░░░░░░░░░░░░                       │
├──────────────────────────────────────────────┤
│ Milestones                                   │
│ 01 Core Movement        In Review            │
│ 02 Combat Prototype     Pending              │
│ 03 Enemy Loop           Pending              │
├──────────────────────────────────────────────┤
│ Tasks                                        │
│ Ready    Create player script    Script      │
│ Running  Assemble player scene   Scene       │
│ Blocked  Add dash shader         Shader      │
│ Done     Define input actions    Script      │
├──────────────────────────────────────────────┤
│ Task Inspector                               │
│ Agent: Scene Agent                           │
│ Depends: player_controller.gd                │
│ Outputs: player.tscn                         │
│ [Edit] [Reassign] [Split]                    │
├──────────────────────────────────────────────┤
│ [Run Milestone] [Pause] [Retry Failed]       │
├──────────────────────────────────────────────┤
│ Playtest Feedback                            │
│ 跳跃太飘，落地反馈不明显...                 │
│ [Generate Fix Tasks] [Accept & Lock]         │
└──────────────────────────────────────────────┘
```

### 2.3 视觉原则

- 跟随 Godot 编辑器原生风格：紧凑、深色、细分隔线。
- 不使用聊天气泡承载 NEXT 任务。
- 不做大卡片和营销式布局。
- Agent 标记用小标签和状态点。
- 路径、任务标题、模型名全部支持 ellipsis 和 tooltip。

## 3. 文件结构

### 3.1 NEXT 领域层

新增：

```text
editor/ai_component/next/
├── SCsub
├── ai_next_types.h
├── ai_next_types.cpp
├── ai_next_project_state.h
├── ai_next_project_state.cpp
├── ai_next_project_store.h
├── ai_next_project_store.cpp
├── ai_next_event_log.h
├── ai_next_event_log.cpp
├── ai_next_prompts.h
├── ai_next_prompts.cpp
├── ai_next_manage_project_tool.h
├── ai_next_manage_project_tool.cpp
├── ai_agent_next_session.h
└── ai_agent_next_session.cpp
```

职责：

- `ai_next_types`: 枚举、数据结构、状态字符串转换、序列化辅助。
- `ai_next_project_state`: 里程碑、任务、资产记录、状态推进和依赖判断。
- `ai_next_project_store`: NEXT 状态持久化，建议路径 `user://ai_agent/next/<project_key>.json`。
- `ai_next_event_log`: 规划、执行、失败、反馈、锁定事件。
- `ai_next_prompts`: NEXT 专用 planning/execution/review prompts。
- `ai_next_manage_project_tool`: NEXT 专用结构化写入工具。
- `ai_agent_next_session`: NEXT 产品会话，不继承 `AIAgentSession`，内部组合多个 `AIAgentBase`。

### 3.2 NEXT UI 层

新增：

```text
editor/ai_component/ui/ai_agent_next_dock.h
editor/ai_component/ui/ai_agent_next_dock.cpp
editor/ai_component/ui/ai_next_panel.h
editor/ai_component/ui/ai_next_panel.cpp
editor/ai_component/ui/ai_next_milestone_list.h
editor/ai_component/ui/ai_next_milestone_list.cpp
editor/ai_component/ui/ai_next_task_tree.h
editor/ai_component/ui/ai_next_task_tree.cpp
editor/ai_component/ui/ai_next_task_inspector.h
editor/ai_component/ui/ai_next_task_inspector.cpp
editor/ai_component/ui/ai_next_feedback_panel.h
editor/ai_component/ui/ai_next_feedback_panel.cpp
```

职责：

- `AIAgentNextDock`: NEXT 专用容器，由 `AIAgentDock` 在 NEXT 模式中显示；它持有 `AIAgentNextSession` 和 `AINextPanel`。
- `AINextPanel`: NEXT 主面板，绑定 `AIAgentNextSession`。
- `AINextMilestoneList`: 里程碑列表。
- `AINextTaskTree`: 当前里程碑任务树。
- `AINextTaskInspector`: 选中任务详情与编辑入口。
- `AINextFeedbackPanel`: 游玩反馈、修复任务生成、锁定按钮。

### 3.3 集成层

修改：

```text
editor/run/editor_run_bar.h
editor/run/editor_run_bar.cpp
editor/ai_component/ui/ai_agent_dock.h
editor/ai_component/ui/ai_agent_dock.cpp
editor/register_editor_types.cpp
editor/ai_component/SCsub
editor/ai_component/ui/SCsub
```

职责：

- `EditorRunBar`: 加 `NEXT` toggle button、持久化开关、发 `next_mode_toggled(bool)` 信号。不 include NEXT UI。
- `AIAgentDock`: 加 `set_next_mode_enabled(bool)`，切换 `chat_root` 和 `next_dock`。
- `register_editor_types.cpp`: 注册新增 NEXT state/session/UI 类。

## 4. 数据模型

### 4.1 状态枚举

```cpp
enum AINextSessionState {
	AI_NEXT_SESSION_IDLE,
	AI_NEXT_SESSION_BRIEFING,
	AI_NEXT_SESSION_PLANNING,
	AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL,
	AI_NEXT_SESSION_EXECUTING,
	AI_NEXT_SESSION_WAITING_PLAYTEST,
	AI_NEXT_SESSION_FEEDBACK_PLANNING,
	AI_NEXT_SESSION_READY_TO_LOCK,
	AI_NEXT_SESSION_FAILED,
};

enum AINextTaskStatus {
	AI_NEXT_TASK_PENDING,
	AI_NEXT_TASK_BLOCKED,
	AI_NEXT_TASK_READY,
	AI_NEXT_TASK_IN_PROGRESS,
	AI_NEXT_TASK_COMPLETED,
	AI_NEXT_TASK_FAILED,
	AI_NEXT_TASK_SKIPPED,
};

enum AINextMilestoneStatus {
	AI_NEXT_MILESTONE_DRAFT,
	AI_NEXT_MILESTONE_READY,
	AI_NEXT_MILESTONE_EXECUTING,
	AI_NEXT_MILESTONE_WAITING_PLAYTEST,
	AI_NEXT_MILESTONE_READY_TO_LOCK,
	AI_NEXT_MILESTONE_LOCKED,
	AI_NEXT_MILESTONE_FAILED,
};
```

### 4.2 核心结构

```cpp
struct AINextTask {
	String id;
	String title;
	String description;
	String assigned_agent_id;
	Vector<String> depends_on;
	Vector<String> asset_refs;
	Vector<String> output_paths;
	AINextTaskStatus status = AI_NEXT_TASK_PENDING;
	String run_id;
	String result_summary;
	String error;
	uint64_t created_at = 0;
	uint64_t updated_at = 0;
};

struct AINextMilestone {
	String id;
	String title;
	String description;
	Vector<AINextTask> tasks;
	AINextMilestoneStatus status = AI_NEXT_MILESTONE_DRAFT;
	int feedback_iteration = 0;
	uint64_t created_at = 0;
	uint64_t updated_at = 0;
};

struct AINextAssetRecord {
	String id;
	String path;
	String source; // user_imported, agent_generated, derived
	bool protected_from_agent_edits = false;
	String parent_asset_id;
	String baseline_milestone_id;
};
```

### 4.3 依赖规则

- `READY`: 当前任务是 `PENDING`，且所有 `depends_on` 任务已完成或跳过。
- `BLOCKED`: 当前任务是 `PENDING`，但至少一个依赖未完成。
- `FAILED`: 执行失败并记录 `error`，用户可重试、改派、拆分或跳过。
- 里程碑可执行：状态为 `READY` 或 `WAITING_PLAYTEST`，且存在至少一个 `READY` 任务。
- 里程碑可锁定：所有非跳过任务完成，没有失败任务，没有执行中任务，没有未处理 change set。

## 5. NEXT Session 设计

`AIAgentNextSession` 是新的产品会话对象，不继承 `AIAgentSession`。

建议继承 `Node`，便于 UI 信号连接和生命周期托管：

```cpp
class AIAgentNextSession : public Node {
	GDCLASS(AIAgentNextSession, Node);

	Ref<AINextProjectState> project_state;
	Ref<AINextProjectStore> project_store;
	Ref<AINextEventLog> event_log;

	Ref<AIAgentBase> planning_agent;
	Ref<AIAgentBase> script_agent;
	Ref<AIAgentBase> scene_agent;
	Ref<AIAgentBase> shader_agent;
	Ref<AIAgentBase> review_agent;

	void _configure_agent(const Ref<AIAgentBase> &p_agent, const String &p_prompt, const Vector<Ref<AITool>> &p_tools);
	void _register_next_tools(const Ref<AIAgentBase> &p_agent);
	void _register_shared_read_tools(const Ref<AIAgentBase> &p_agent);
	void _register_specialist_write_tools(const Ref<AIAgentBase> &p_agent, const String &p_agent_id);

public:
	void set_model_profile_id(const String &p_model_profile_id);
	void submit_brief(const String &p_brief);
	void generate_plan();
	void approve_plan();
	void run_active_milestone();
	void generate_feedback_tasks(const String &p_feedback);
	void accept_and_lock_active_milestone();
	void cancel_current_operation();
};
```

Agent 组成：

- `planning_agent`: 基于 `AIAgentBase`，只能调用 `ai_next.manage_project` 和只读上下文工具。
- `script_agent`: 基于 `AIAgentBase`，使用脚本工具和只读上下文工具。
- `scene_agent`: 基于 `AIAgentBase`，使用场景工具和只读上下文工具。
- `shader_agent`: 基于 `AIAgentBase`，使用 shader 工具和只读上下文工具。
- `review_agent`: 基于 `AIAgentBase`，只读项目和 change set，输出 findings，不直接改文件。

## 6. 实施计划

### Task 1: 建立 NEXT 状态模型

**Files:**
- Create: `editor/ai_component/next/SCsub`
- Create: `editor/ai_component/next/ai_next_types.h`
- Create: `editor/ai_component/next/ai_next_types.cpp`
- Create: `editor/ai_component/next/ai_next_project_state.h`
- Create: `editor/ai_component/next/ai_next_project_state.cpp`
- Modify: `editor/ai_component/SCsub`
- Test: `tests/editor/test_ai_next_project_state.cpp`

- [ ] **Step 1: 写失败测试**

```cpp
TEST_CASE("[AI][NEXT] project state marks tasks ready only after dependencies complete") {
	Ref<AINextProjectState> state;
	state.instantiate();
	String milestone_id = state->create_milestone("Core Movement", "Player movement baseline.");
	String script_task = state->add_task(milestone_id, "Create player script", "script_agent", Array());
	String scene_task = state->add_task(milestone_id, "Assemble player scene", "scene_agent", Array::make(script_task));

	Array ready = state->get_ready_tasks(milestone_id);
	CHECK(ready.size() == 1);

	state->mark_task_completed(script_task, "Created player_controller.gd", Array::make("res://player_controller.gd"));
	ready = state->get_ready_tasks(milestone_id);
	CHECK(ready.size() == 1);
	CHECK(String(Dictionary(ready[0]).get("id", "")) == scene_task);
}
```

- [ ] **Step 2: 运行测试并确认失败**

Run: `scons platform=windows target=editor tests=yes`

Expected: 编译失败，提示 `AINextProjectState` 未定义。

- [ ] **Step 3: 实现最小状态模型**

实现枚举、结构序列化、里程碑创建、任务创建、依赖 ready 判断、任务完成状态更新。

- [ ] **Step 4: 运行测试**

Run: `scons platform=windows target=editor tests=yes`

Expected: `test_ai_next_project_state` 通过。

- [ ] **Step 5: Commit**

```bash
git add editor/ai_component/next editor/ai_component/SCsub tests/editor/test_ai_next_project_state.cpp
git commit -m "feat(ai): add NEXT project state"
```

### Task 2: 增加 NEXT 持久化与事件日志

**Files:**
- Create: `editor/ai_component/next/ai_next_project_store.h`
- Create: `editor/ai_component/next/ai_next_project_store.cpp`
- Create: `editor/ai_component/next/ai_next_event_log.h`
- Create: `editor/ai_component/next/ai_next_event_log.cpp`
- Test: `tests/editor/test_ai_next_project_state.cpp`

- [ ] **Step 1: 写失败测试**

```cpp
TEST_CASE("[AI][NEXT] project store round trips milestones") {
	Ref<AINextProjectStore> store;
	store.instantiate();
	Ref<AINextProjectState> state;
	state.instantiate();
	state->create_milestone("Inventory", "Basic inventory loop.");

	CHECK(store->save("test_project", state).is_empty());
	Ref<AINextProjectState> loaded = store->load("test_project");
	CHECK(loaded.is_valid());
	CHECK(loaded->get_milestone_count() == 1);
}
```

- [ ] **Step 2: 实现 store**

保存到 `user://ai_agent/next/<project_key>.json`。写入使用临时文件再替换，读取失败返回空 state 并记录错误事件。

- [ ] **Step 3: 实现 event log**

事件字段：`timestamp`、`event_type`、`milestone_id`、`task_id`、`agent_id`、`message`、`metadata`。

- [ ] **Step 4: 运行测试**

Run: `scons platform=windows target=editor tests=yes`

Expected: store 和 event log 测试通过。

- [ ] **Step 5: Commit**

```bash
git add editor/ai_component/next/ai_next_project_store.* editor/ai_component/next/ai_next_event_log.* tests/editor/test_ai_next_project_state.cpp
git commit -m "feat(ai): persist NEXT project state"
```

### Task 3: 增加 NEXT 专用管理工具

**Files:**
- Create: `editor/ai_component/next/ai_next_manage_project_tool.h`
- Create: `editor/ai_component/next/ai_next_manage_project_tool.cpp`
- Test: `tests/editor/test_ai_next_tools.cpp`

禁止修改：

- `editor/ai_component/agent/ai_main_agent.*`
- `editor/ai_component/planning/ai_manage_plan_tool.*`

- [ ] **Step 1: 写失败测试**

```cpp
TEST_CASE("[AI][NEXT] manage project tool creates structured milestones") {
	Ref<AINextProjectState> state;
	state.instantiate();
	Ref<AINextManageProjectTool> tool;
	tool.instantiate();
	tool->set_project_state(state);

	Dictionary milestone;
	milestone["title"] = "Core Movement";
	milestone["description"] = "Build player movement.";
	milestone["tasks"] = Array();

	Dictionary args;
	args["action"] = "replace_plan";
	args["milestones"] = Array::make(milestone);

	AIToolResult result = tool->execute(args);
	CHECK(result.error.is_empty());
	CHECK(state->get_milestone_count() == 1);
}
```

- [ ] **Step 2: 实现 tool schema**

动作：

- `replace_plan`
- `update_milestone`
- `update_task`
- `append_tasks`
- `register_asset`
- `mark_feedback_iteration`

- [ ] **Step 3: 参数校验**

缺少 action、未知 agent、循环依赖、重复 task id 都必须返回错误，不得部分写入。

- [ ] **Step 4: 运行测试**

Run: `scons platform=windows target=editor tests=yes`

Expected: NEXT tool 测试通过。

- [ ] **Step 5: Commit**

```bash
git add editor/ai_component/next/ai_next_manage_project_tool.* tests/editor/test_ai_next_tools.cpp
git commit -m "feat(ai): add NEXT project management tool"
```

### Task 4: 实现 `AIAgentNextSession`

**Files:**
- Create: `editor/ai_component/next/ai_next_prompts.h`
- Create: `editor/ai_component/next/ai_next_prompts.cpp`
- Create: `editor/ai_component/next/ai_agent_next_session.h`
- Create: `editor/ai_component/next/ai_agent_next_session.cpp`
- Test: `tests/editor/test_ai_agent_next_session.cpp`

禁止修改：

- `editor/ai_component/agent/ai_agent_session.*`
- `editor/ai_component/agent/ai_main_agent.*`

- [ ] **Step 1: 写 session 初始化测试**

```cpp
TEST_CASE("[AI][NEXT] session initializes independent agents") {
	AIAgentNextSession *session = memnew(AIAgentNextSession);
	CHECK(session->get_project_state().is_valid());
	CHECK(session->has_agent("planning_agent"));
	CHECK(session->has_agent("script_agent"));
	CHECK(session->has_agent("scene_agent"));
	CHECK(session->has_agent("shader_agent"));
	memdelete(session);
}
```

- [ ] **Step 2: 实现 session skeleton**

`AIAgentNextSession` 继承 `Node`，持有 state/store/event log 和多个 `AIAgentBase`。

- [ ] **Step 3: 接入模型配置**

通过 `AIModelSettings::get_provider_config(model_id)` 配置每个 `AIAgentBase`，不复用聊天 composer 的状态。

- [ ] **Step 4: 注册 NEXT 专用工具**

Planning Agent 注册 `AINextManageProjectTool`；实施 Agent 注册对应 editor write tools；所有 Agent 注册必要 read tools。

- [ ] **Step 5: 运行测试**

Run: `scons platform=windows target=editor tests=yes`

Expected: session 初始化测试通过。

- [ ] **Step 6: Commit**

```bash
git add editor/ai_component/next/ai_next_prompts.* editor/ai_component/next/ai_agent_next_session.* tests/editor/test_ai_agent_next_session.cpp
git commit -m "feat(ai): add NEXT session"
```

### Task 5: 创建 `AIAgentNextDock` 和 NEXT 面板 UI 组件

**Files:**
- Create: `editor/ai_component/ui/ai_agent_next_dock.h`
- Create: `editor/ai_component/ui/ai_agent_next_dock.cpp`
- Create: `editor/ai_component/ui/ai_next_panel.h`
- Create: `editor/ai_component/ui/ai_next_panel.cpp`
- Create: `editor/ai_component/ui/ai_next_milestone_list.h`
- Create: `editor/ai_component/ui/ai_next_milestone_list.cpp`
- Create: `editor/ai_component/ui/ai_next_task_tree.h`
- Create: `editor/ai_component/ui/ai_next_task_tree.cpp`
- Create: `editor/ai_component/ui/ai_next_task_inspector.h`
- Create: `editor/ai_component/ui/ai_next_task_inspector.cpp`
- Create: `editor/ai_component/ui/ai_next_feedback_panel.h`
- Create: `editor/ai_component/ui/ai_next_feedback_panel.cpp`
- Modify: `editor/ai_component/ui/SCsub`

- [ ] **Step 1: 创建可编译空控件**

每个控件只负责一个区域：

- `AIAgentNextDock`: NEXT 容器，创建并持有 `AIAgentNextSession`，承载 `AINextPanel`。
- `AINextPanel`: 顶层布局、状态绑定、按钮事件。
- `AINextMilestoneList`: 里程碑列表。
- `AINextTaskTree`: 当前里程碑任务树。
- `AINextTaskInspector`: 选中任务详情。
- `AINextFeedbackPanel`: 反馈和锁定。

- [ ] **Step 2: 创建静态布局**

先用假数据渲染完整结构，确认右侧 Dock 宽度下文本不挤压。

- [ ] **Step 3: 在 `AIAgentNextDock` 内创建 `AIAgentNextSession`**

`AIAgentNextDock` 初始化时创建 session，并交给 `AINextPanel`：

```cpp
next_session = memnew(AIAgentNextSession);
add_child(next_session);

next_panel = memnew(AINextPanel);
next_panel->set_next_session(next_session);
add_child(next_panel);
```

- [ ] **Step 4: 接入 `AIAgentNextSession`**

`AINextPanel::set_next_session(AIAgentNextSession *p_session)` 绑定状态刷新。UI 只调用 session 方法，不直接改 state。

- [ ] **Step 5: 运行编译**

Run: `scons platform=windows target=editor tests=yes`

Expected: 编译通过。

- [ ] **Step 6: Commit**

```bash
git add editor/ai_component/ui/ai_agent_next_dock.* editor/ai_component/ui/ai_next_* editor/ai_component/ui/SCsub
git commit -m "feat(ai): add NEXT panel UI"
```

### Task 6: 改造 `AIAgentDock` 外壳以承载 NEXT 面板

**Files:**
- Modify: `editor/ai_component/ui/ai_agent_dock.h`
- Modify: `editor/ai_component/ui/ai_agent_dock.cpp`
- Modify: `editor/register_editor_types.cpp`

禁止修改：

- `editor/ai_component/agent/ai_agent_session.*`
- `editor/ai_component/agent/ai_main_agent.*`

- [ ] **Step 1: 增加成员**

在 `AIAgentDock` 增加：

```cpp
VBoxContainer *chat_root = nullptr;
AIAgentNextDock *next_dock = nullptr;
bool next_mode_enabled = false;
```

- [ ] **Step 2: 保留现有聊天控件**

把当前构造函数里的原 `root` 改名为 `chat_root`，原 session bar、message list、composer 都继续挂在 `chat_root` 下。

- [ ] **Step 3: 添加 NEXT dock view**

在同一个 `AIAgentDock` 下创建 `AIAgentNextDock`，但不要把它注册到 `EditorDockManager`：

```cpp
next_dock = memnew(AIAgentNextDock);
next_dock->hide();
add_child(next_dock);
```

- [ ] **Step 4: 添加切换 API**

```cpp
void AIAgentDock::set_next_mode_enabled(bool p_enabled) {
	next_mode_enabled = p_enabled;
	if (chat_root) {
		chat_root->set_visible(!p_enabled);
	}
	if (next_dock) {
		next_dock->set_visible(p_enabled);
	}
	if (p_enabled) {
		make_visible();
	}
}
```

- [ ] **Step 5: 注册新增类型**

在 `register_editor_types.cpp` 注册 `AIAgentNextSession`、`AIAgentNextDock` 和 NEXT UI 控件。

- [ ] **Step 6: 手动验证**

Manual:

- `AIAgentDock` 原聊天模式能正常显示。
- 调用 `set_next_mode_enabled(true)` 后同一个 Dock tab 显示 NEXT 面板。
- 调用 `set_next_mode_enabled(false)` 后恢复聊天 UI。
- `AIAgentNextDock` 没有作为第二个 Dock tab 出现在 Dock 管理器里。
- 聊天会话消息不丢失。

- [ ] **Step 7: Commit**

```bash
git add editor/ai_component/ui/ai_agent_dock.* editor/register_editor_types.cpp
git commit -m "feat(ai): host NEXT panel in AI agent dock"
```

### Task 7: 在顶部运行栏增加 NEXT 开关并连接 AIAgentDock

**Files:**
- Modify: `editor/run/editor_run_bar.h`
- Modify: `editor/run/editor_run_bar.cpp`
- Modify: `editor/editor_node.cpp`

- [ ] **Step 1: 添加 RunBar 字段和信号**

在 `EditorRunBar` 增加：

```cpp
Button *next_mode_button = nullptr;
bool next_mode_enabled = false;
void _next_mode_pressed();
bool is_next_mode_enabled() const;
void set_next_mode_enabled(bool p_enabled);
```

`_bind_methods()` 增加：

```cpp
ADD_SIGNAL(MethodInfo("next_mode_toggled", PropertyInfo(Variant::BOOL, "enabled")));
```

- [ ] **Step 2: 在 play_button 左侧创建按钮**

在 `EditorRunBar::EditorRunBar()` 中，`play_button = memnew(Button);` 之前插入：

```cpp
next_mode_button = memnew(Button);
main_hbox->add_child(next_mode_button);
next_mode_button->set_text(TTRC("NEXT"));
next_mode_button->set_toggle_mode(true);
next_mode_button->set_theme_type_variation("RunBarButton");
next_mode_button->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
next_mode_button->set_tooltip_text(TTRC("Toggle NEXT mode."));
next_mode_button->connect(SceneStringName(pressed), callable_mp(this, &EditorRunBar::_next_mode_pressed));
```

- [ ] **Step 3: 持久化开关**

使用 `EditorSettings::get_singleton()->get_project_metadata("ai_next", "enabled", false)` 保存项目级状态。

- [ ] **Step 4: 连接现有 AIAgentDock**

在 `EditorNode` 完成 `ai_dock = memnew(AIAgentDock);` 后连接：

```cpp
EditorRunBar::get_singleton()->connect("next_mode_toggled", callable_mp(ai_dock, &AIAgentDock::set_next_mode_enabled));
ai_dock->set_next_mode_enabled(EditorRunBar::get_singleton()->is_next_mode_enabled());
```

- [ ] **Step 5: 手动验证**

Manual:

- NEXT 按钮在运行按钮左侧。
- 点击 NEXT 后现有 AI Agent Dock 显示 NEXT 面板。
- 再点一次后现有 AI Agent Dock 恢复聊天面板。
- 现有聊天请求仍走 `AIAgentSession` 和 `AIMainAgent`。

- [ ] **Step 6: Commit**

```bash
git add editor/run/editor_run_bar.* editor/editor_node.cpp
git commit -m "feat(editor): toggle NEXT panel from run bar"
```

### Task 8: 接入规划流程

**Files:**
- Modify: `editor/ai_component/next/ai_agent_next_session.h`
- Modify: `editor/ai_component/next/ai_agent_next_session.cpp`
- Modify: `editor/ai_component/ui/ai_next_panel.cpp`
- Modify: `editor/ai_component/ui/ai_next_milestone_list.cpp`
- Modify: `editor/ai_component/ui/ai_next_task_tree.cpp`

- [ ] **Step 1: `submit_brief()`**

Brief 文本写入 NEXT state，不进入聊天 conversation store。

- [ ] **Step 2: `generate_plan()`**

调用 `planning_agent->start()` 或 `run()`，prompt 中要求必须调用 `ai_next.manage_project` 写入结构化计划。

- [ ] **Step 3: UI 状态**

规划中禁用 Brief 输入和生成按钮；规划成功进入人工审核；规划失败显示错误并允许重试。

- [ ] **Step 4: 运行测试**

Run: `scons platform=windows target=editor tests=yes`

Expected: 编译通过，session 状态测试通过。

- [ ] **Step 5: Commit**

```bash
git add editor/ai_component/next/ai_agent_next_session.* editor/ai_component/ui/ai_next_*
git commit -m "feat(ai): connect NEXT planning flow"
```

### Task 9: 接入里程碑执行流程

**Files:**
- Modify: `editor/ai_component/next/ai_agent_next_session.h`
- Modify: `editor/ai_component/next/ai_agent_next_session.cpp`
- Modify: `editor/ai_component/ui/ai_next_panel.cpp`
- Modify: `editor/ai_component/ui/ai_next_task_tree.cpp`
- Modify: `editor/ai_component/ui/ai_next_task_inspector.cpp`

- [ ] **Step 1: 串行执行 READY 任务**

MVP 先串行执行，避免并发 agent 抢写同一资源。

- [ ] **Step 2: 按 `assigned_agent_id` 选择 agent**

- `script_agent` -> 脚本工具。
- `scene_agent` -> 场景工具。
- `shader_agent` -> shader 工具。

- [ ] **Step 3: 状态回写**

执行开始 `IN_PROGRESS`，成功 `COMPLETED`，失败 `FAILED`。

- [ ] **Step 4: 产出路径回写**

从工具结果或 agent summary 提取 `output_paths`，写入任务。

- [ ] **Step 5: Commit**

```bash
git add editor/ai_component/next/ai_agent_next_session.* editor/ai_component/ui/ai_next_*
git commit -m "feat(ai): run NEXT milestone tasks"
```

### Task 10: 接入反馈与锁定

**Files:**
- Modify: `editor/ai_component/next/ai_agent_next_session.h`
- Modify: `editor/ai_component/next/ai_agent_next_session.cpp`
- Modify: `editor/ai_component/next/ai_next_project_state.*`
- Modify: `editor/ai_component/ui/ai_next_feedback_panel.cpp`

- [ ] **Step 1: 反馈生成任务**

`generate_feedback_tasks(feedback)` 调用 planning agent，把反馈转换为追加任务。

- [ ] **Step 2: 人工确认**

追加任务先显示预览，用户确认后才写入 `AINextProjectState`。

- [ ] **Step 3: 锁定前检查**

阻止锁定条件：

- 有未完成任务。
- 有失败任务。
- 有执行中任务。
- 有未处理 change set。
- 当前主场景未保存。

- [ ] **Step 4: `Accept & Lock`**

锁定当前里程碑，记录 baseline，不让 agent 自动执行下一里程碑。

- [ ] **Step 5: Commit**

```bash
git add editor/ai_component/next/ai_agent_next_session.* editor/ai_component/next/ai_next_project_state.* editor/ai_component/ui/ai_next_feedback_panel.cpp
git commit -m "feat(ai): add NEXT feedback and locking"
```

### Task 11: 增强调度和 Review Agent

**Files:**
- Modify: `editor/ai_component/next/ai_agent_next_session.cpp`
- Modify: `editor/ai_component/next/ai_next_project_state.cpp`
- Modify: `editor/ai_component/ui/ai_next_panel.cpp`

- [ ] **Step 1: 检测输出冲突**

并行前先检查同一轮 READY tasks 的 `output_paths`，冲突时串行执行。

- [ ] **Step 2: 并行同层任务**

并行上限默认 2。

- [ ] **Step 3: Review Agent**

Review Agent 只读项目和 change set，输出 findings；findings 经用户确认后转换为修复任务。

- [ ] **Step 4: 失败恢复**

失败任务支持 retry、reassign、split、skip with reason。

- [ ] **Step 5: Commit**

```bash
git add editor/ai_component/next/ai_agent_next_session.cpp editor/ai_component/next/ai_next_project_state.cpp editor/ai_component/ui/ai_next_panel.cpp
git commit -m "feat(ai): improve NEXT scheduling and review"
```

## 7. 验证策略

### 自动测试

每个任务至少运行：

```bash
scons platform=windows target=editor tests=yes
```

重点覆盖：

- NEXT state 序列化。
- 依赖 ready 判断。
- 结构化 tool 写入校验。
- `AIAgentNextSession` 不依赖 `AIAgentSession`。
- NEXT 工具不注册到 `AIMainAgent`。
- `AIAgentDock` 在 NEXT 开关切换时保留聊天 state。
- `AIAgentNextDock` 被保留并由 `AIAgentDock` 承载。
- Store 读写和损坏 JSON 回退。

### 手动测试

1. 启动编辑器。
2. 确认顶部运行按钮左侧出现 `NEXT`。
3. 确认默认 `AIAgentDock` 是聊天面板。
4. 点击 `NEXT`，同一个 `AIAgentDock` 显示 NEXT 面板。
5. 再点击 `NEXT`，同一个 `AIAgentDock` 恢复聊天面板。
6. 在聊天面板发送消息，确认仍走原聊天流程。
7. 在 NEXT 面板输入需求并生成计划。
8. 关闭 NEXT 再打开，确认 NEXT 状态保留。
9. 运行里程碑，确认任务状态更新。
10. 输入反馈，确认生成追加任务。
11. 完成后锁定里程碑。

## 8. 风险与处理

- 风险：`AIAgentDock` 变复杂。
  - 处理：只让它做宿主和视图切换；NEXT 逻辑放进 `AIAgentNextSession`，NEXT UI 放进独立控件。
- 风险：NEXT 和聊天共享 session 导致状态污染。
  - 处理：NEXT 完全使用 `AIAgentNextSession`，不引用 `AIAgentSession`。
- 风险：NEXT 工具污染 `AIMainAgent`。
  - 处理：NEXT 工具只注册到 `AIAgentNextSession` 创建的 `AIAgentBase` 实例。
- 风险：`EditorRunBar` 引入 AI 组件依赖。
  - 处理：RunBar 只发 `next_mode_toggled` 信号，不 include AI headers。
- 风险：并行任务覆盖文件。
  - 处理：MVP 串行，增强阶段检测 `output_paths` 再并行。

## 9. 明确不做

- 不把 `AIAgentNextDock` 注册成第二个独立 Dock tab。
- 不让 `AIAgentSession` 管 NEXT 状态。
- 不扩展 `AIMainAgent` 来做 NEXT coordinator。
- 不让现有聊天 composer 承担 NEXT Brief 输入。
- 不在第一版做复杂可视化依赖图。
- 不允许 Agent 自动锁定里程碑。

## 10. 推荐里程碑

### Milestone A: 宿主切换

交付：

- 顶部 NEXT 开关。
- `AIAgentDock` 内聊天/NEXT 视图切换。
- 保留并实现 `AIAgentNextDock`，由 `AIAgentDock` 承载。
- 独立 `AIAgentNextSession` skeleton。
- NEXT state/store/event log。

验收：

- 现有 AI 对话流程不变。
- NEXT 开关能让同一个 `AIAgentDock` 在聊天面板和 NEXT 面板之间切换。

### Milestone B: 规划可用

交付：

- NEXT planning agent。
- `ai_next.manage_project` 工具。
- 里程碑和任务可编辑。

验收：

- 输入需求后生成结构化计划，不依赖自由文本解析。

### Milestone C: 单里程碑执行

交付：

- 串行执行任务。
- 任务状态、错误、产出回写。

验收：

- 一个简单游戏原型里程碑可从计划执行到完成。

### Milestone D: 反馈闭环

交付：

- 游玩反馈生成修复任务。
- 锁定前检查。
- 里程碑锁定。

验收：

- 完成“规划 -> 执行 -> 游玩 -> 反馈 -> 修复 -> 锁定”闭环。

### Milestone E: 调度增强

交付：

- 并行同层任务。
- Review Agent。
- findings 转修复任务。

验收：

- 多 Agent 不依赖固定顺序，而是依赖图调度。
