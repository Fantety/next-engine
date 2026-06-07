# AI 模块 Normal 模式与 NEXT 模式共性分析与优化建议

## 概述

本文档分析 NextEngine 中 AI 功能的两种工作模式——**普通模式 (Normal)** 和 **NEXT 模式**——之间的架构共性、差异及代码重复点，并提出可优化结构点以提高代码可维护性和统一性。

---

## 1. 架构总览

```
                        ┌──────────────────────┐
                        │     AIAgentBase       │ (RefCounted) 公共基类
                        │  - runtime/runner     │
                        │  - tool_registry      │
                        │  - prompt/provider    │
                        └──────┬───────────────┘
                               │
              ┌────────────────┼──────────────────┐
              │                                 │
    ┌─────────▼──────────┐        ┌──────────────▼─────────────┐
    │    AIMainAgent     │        │   AINextPlanningAgent      │
    │  (Normal 单Agent)   │        │   AINextScriptAgent        │
    │  - 注册全部工具      │        │   AINextSceneAgent         │
    └────────┬───────────┘        │   AINextShaderAgent        │
             │                    │   AINextReviewAgent        │
             │                    │   (NEXT 多Agent协作)        │
             │                    └──────────────┬─────────────┘
             │                                   │
    ┌────────▼───────────┐         ┌─────────────▼──────────────┐
    │   AIAgentSession   │         │   AIAgentNextSession       │
    │     (Node)         │         │       (Node)               │
    │  - 存储: ConvStore │         │  - 存储: WorkflowStore     │
    │  - 计划: PlanMgr   │         │  - 状态: ProjectState      │
    └────────────────────┘         │  - 检查点: Checkpoint       │
                                   │  - 事件日志: EventLog      │
                                   └────────────────────────────┘
```

### 共享层

| 层次 | 共享状态 | 说明 |
|------|---------|------|
| Agent 基类 | ✅ `AIAgentBase` | 所有 Normal/NEXT Agent 均继承此基类 |
| 运行时 | ✅ `AIAgentRuntime` / `AIAgentRuntimeRunner` | 完全共享 |
| 工具系统 | ✅ `AITool` / `AIToolRegistry` / 所有具体工具类 | 完全共享 |
| Provider | ✅ OpenAI Runtime Client / MCP Client / Model Settings | 完全共享 |
| Context Provider | ✅ Editor / ProjectTree / BestPractices / Rules / Skill | 完全共享 |
| UI 基础组件 | ✅ MessageList / MessageBubble / Composer / MarkdownRenderer | 完全共享 |

### 差异化层

| 层次 | Normal 模式 | NEXT 模式 |
|------|------------|----------|
| 会话管理 | `AIAgentSession` (agent/) | `AIAgentNextSession` (next/) |
| Agent 数量 | 1 (`AIMainAgent`) | 5 (Planning/Script/Scene/Shader/Review) |
| 计划模型 | `AIPlanManager` 简单任务列表 | `AINextProjectState` 里程碑/任务/依赖/资产 |
| 存储 | `AIConversationStore` (storage/) | `AINextWorkflowStore` + `AINextProjectStore` (next/) |
| Prompt | `agent_system_prompt.h` 单提示词 | `ai_next_prompts.h/cpp` 各Agent专用提示词 |
| UI 面板 | `AIAgentDock` (EditorDock) | `AIAgentNextDock` (VBoxContainer) |
| 状态机 | `AI_AGENT_STATE_*` 简单线性 | `AI_NEXT_SESSION_*` 里程碑式工作流 |

---

## 2. 发现的代码重复与结构问题

### 2.1 会话层 (Session) 缺失公共基类 [高优先级]

**位置**: 
- `agent/ai_agent_session.h:24` — `AIAgentSession : public Node`
- `next/ai_agent_next_session.h:16` — `AIAgentNextSession : public Node`

**问题**: 两个 Session 类独立实现，有大量重复代码：

```cpp
// 两处完全相同的实现
String _get_project_scope_key() const {
    ProjectSettings *ps = ProjectSettings::get_singleton();
    if (!ps) return "global";
    String resource_path = ps->get_resource_path();
    if (resource_path.is_empty()) return "global";
    return resource_path.md5_text();
}
```

| 重复模式 | AIAgentSession | AIAgentNextSession |
|---------|---------------|-------------------|
| 项目作用域键计算 | `_get_project_scope_key()` | `_get_project_scope_key()` |
| 运行时消息索引映射 | `runtime_to_local_message_indices` | `runtime_to_progress_indices` |
| 初始加载逻辑 | `_load_initial_session()` | `_load_initial_workflow()` |
| 信号连接 (runtime_finished/message_added/message_updated) | 构造中连接 | `_connect_agent_runtime()` |
| 忙碌状态检查 | `_is_busy()` | `_is_workflow_active()` |
| 测试辅助方法 | `save_for_test()`, `replace_messages_for_test()` 等 | `save_workflow_for_test()`, `get_workflow_checkpoint_for_test()` 等 |

**建议**: 提取 `AISessionBase : public Node` 公共基类，包含:
```cpp
class AISessionBase : public Node {
protected:
    String _get_project_scope_key() const;          // 统一实现
    String _make_id(const String &p_prefix) const;  // ID 生成
    void _connect_runtime_signals(...);             // 运行时信号连接
    virtual void _load_initial_state() = 0;         // 子类实现初始加载
};
```

**预估收益**: 消除 ~80 行重复代码，统一运行时消息管理逻辑。

---

### 2.2 工具注册代码高度重复 [高优先级]

**位置**:
- `agent/ai_main_agent.cpp:48-75` — `MainAgentToolFactory` + `_create_main_agent_tool<T>()`
- `next/agents/ai_next_agents.cpp:39-44` — `_make_tool<T>()`

**问题**: 同一模式的工具工厂函数以不同名字实现：

```cpp
// ai_main_agent.cpp:52-57
template <typename T>
Ref<AITool> _create_main_agent_tool() {
    Ref<T> tool;
    tool.instantiate();
    return tool;
}

// ai_next_agents.cpp:39-44 (完全相同)
template <typename T>
Ref<AITool> _make_tool() {
    Ref<T> tool;
    tool.instantiate();
    return tool;
}
```

此外，两个文件都导入几乎完全相同的工具头文件列表（场景工具、脚本工具、着色器工具、项目工具），重复约 30 行 #include。

**建议**: 
1. 在 `tools/` 下创建 `ai_tool_factory.h`:
```cpp
template <typename T>
Ref<AITool> ai_make_tool() { ... }

void ai_register_shared_project_tools(AIAgentBase *p_agent);
void ai_register_scene_write_tools(AIAgentBase *p_agent, AIToolPermission p_perm);
void ai_register_script_write_tools(AIAgentBase *p_agent, AIToolPermission p_perm);
void ai_register_shader_tools(AIAgentBase *p_agent, AIToolPermission p_perm);
void ai_register_destructive_tools(AIAgentBase *p_agent);
```
2. Normal 和 NEXT 均调用这些工厂函数

**预估收益**: 消除 ~60 行重复代码 + ~30 行重复引入，工具分组逻辑集中管理。

---

### 2.3 计划管理系统分裂 [中优先级]

**位置**:
- `planning/ai_plan_manager.h:13-17` — `AIPlanManager` 单例
- `next/ai_next_project_state.h:14-87` — `AINextProjectState`

**问题**: 两套完全独立的计划管理模型：

| 特性 | AIPlanManager (Normal) | AINextProjectState (NEXT) |
|------|----------------------|--------------------------|
| 模式 | 单例 | RefCounted 实例 |
| 计划层级 | 扁平任务列表 | 里程碑 → 任务 → 依赖 → 资产 |
| 任务状态 | pending / in_progress / completed / failed | pending / blocked / ready / in_progress / completed / failed / skipped |
| 依赖管理 | 无 | 有 (depends_on) |
| 资产追踪 | 无 | 有 (AINextAssetRecord) |
| 持久化 | 用户配置 | 文件存储 (ProjectStore) |
| 对应 Tool | `AIManagePlanTool` | `AINextManageProjectTool` |

两类 Tool 也完全独立实现：
- `AIManagePlanTool` (`planning/ai_manage_plan_tool.h`) → 操作 `AIPlanManager::get_singleton()`
- `AINextManageProjectTool` (`next/ai_next_manage_project_tool.h`) → 操作 `AINextProjectState`

**建议**: 
- 短期：在 `AIPlanManager` 文档中明确标记其为轻量级单任务计划，与 `AINextProjectState` 的职责边界
- 长期：将 `AIPlanManager` 重构为 `AINextProjectState` 的「单里程碑适配器」，复用其内部状态管理

---

### 2.4 存储层三套独立实现 [中优先级]

**位置**:
- `storage/ai_conversation_store.h` — Normal 对话存储
- `next/ai_next_workflow_store.h` — NEXT 工作流存储
- `next/ai_next_project_store.h` — NEXT 项目状态存储

**问题**: 三者各自实现相同的文件操作模式：

```cpp
// 每个 Store 各自实现的重复模式：
String base_dir = "user://ai_agent/...";
Error _ensure_base_dir() const;                    // 目录创建
String _sanitize_*(const String &p_key) const;     // 路径安全化
String _get_*_path(const String &p_id) const;      // 路径拼接
```

**建议**: 提取 `ai_storage_base.h`:
```cpp
class AIStorageBase : public RefCounted {
protected:
    String base_dir;
    Error ensure_dir() const;
    static String sanitize_path_segment(const String &p_segment);
    String get_file_path(const String &p_id, const String &p_ext = ".json") const;
    Error write_json(const String &p_path, const Dictionary &p_data) const;
    Error read_json(const String &p_path, Dictionary &r_data) const;
};
```

---

### 2.5 NEXT UI 组件与 Normal UI 混放 [低优先级]

**当前结构**:
```
ui/
├── ai_agent_dock.cpp/h         ← Normal 主面板
├── ai_agent_next_dock.cpp/h    ← NEXT 主面板 (混在此处)
├── ai_next_feedback_panel.*    ← NEXT 组件
├── ai_next_milestone_list.*    ← NEXT 组件
├── ai_next_panel.*             ← NEXT 组件
├── ai_next_task_inspector.*    ← NEXT 组件
├── ai_next_task_tree.*         ← NEXT 组件
├── ai_settings_next_page.*     ← NEXT 设置页
├── ai_composer.*               ← Normal 组件
├── ai_message_list.*           ← Normal 组件
└── ... (其他 30+ Normal 组件)
```

对比 `next/agents/` 有独立子目录，NEXT UI 应同等待遇。

**建议**: 将 7 组 NEXT UI 文件移入 `ui/next/`:
```
ui/
├── (Normal 模式 UI 组件)
└── next/
    ├── ai_agent_next_dock.cpp/h
    ├── ai_next_panel.cpp/h
    ├── ai_next_milestone_list.cpp/h
    ├── ai_next_task_tree.cpp/h
    ├── ai_next_task_inspector.cpp/h
    ├── ai_next_feedback_panel.cpp/h
    └── ai_settings_next_page.cpp/h
```

**注意**: 需要同步更新 `ui/SCsub` 中头文件路径引用。

---

### 2.6 Dock 层紧耦合 [低优先级]

**位置**: `ui/ai_agent_dock.h:47` — `AIAgentNextDock *next_dock = nullptr`

```cpp
class AIAgentDock : public EditorDock {
    AIAgentNextDock *next_dock = nullptr;   // 直接持有 NEXT Dock 指针
    bool next_mode_enabled = false;         // if/else 模式切换
    void set_next_mode_enabled(bool p_enabled);
};
```

**问题**: 
- `AIAgentDock` 直接依赖 `AIAgentNextDock` 的具体类型
- 模式切换通过 `if (next_mode_enabled)` 分支实现
- 未来添加第三种模式需修改 Dock 代码

**建议**: 抽象模式面板接口：
```cpp
class AIModePanel : public VBoxContainer {
    GDCLASS(AIModePanel, VBoxContainer);
public:
    virtual void apply_settings() = 0;
    virtual void on_activated() = 0;
    virtual void on_deactivated() = 0;
};

// Normal 模式
class AINormalModePanel : public AIModePanel { ... };

// NEXT 模式  
class AINextModePanel : public AIModePanel { ... };
```

`AIAgentDock` 改为持有 `AIModePanel *active_panel` 并通过接口操作。

---

### 2.7 Prompt 管理风格不统一 [低优先级]

| 模式 | 文件 | 风格 |
|------|------|------|
| Normal | `prompts/agent_system_prompt.h` | `constexpr const char*` 单字符串常量 |
| NEXT | `next/ai_next_prompts.h/cpp` | `namespace` + 函数返回 `const char*` |

**建议**: 统一为 Prompt 注册表：
```cpp
// prompts/ai_prompt_registry.h
namespace AIPrompts {
    const char *get_system_prompt();        // Normal 通用
    const char *get_planning_prompt();      // NEXT Planning Agent
    const char *get_script_prompt();        // NEXT Script Agent
    const char *get_scene_prompt();         // NEXT Scene Agent
    const char *get_shader_prompt();        // NEXT Shader Agent  
    const char *get_review_prompt();        // NEXT Review Agent
}
```

将 `agent_system_prompt.h` 和 `ai_next_prompts.h/cpp` 合并到一处。

---

### 2.8 Context Provider 注入不统一 [低优先级]

**问题**: 
- Normal 模式：`AIAgentSession._collect_context()` 显式收集 editor/project/best_practices/rules/skill context，作为 `context_documents` 参数传入 `agent->start(messages, context_documents)`
- NEXT 模式：`AIAgentNextSession._begin_agent_run()` 调用 `agent->start(run_messages)` 时**未传入 context documents**，依赖 Agent system prompt 内指令
  
这意味着 NEXT Agent 在执行时缺少编辑器状态、项目树结构等上下文信息。

**建议**: NEXT Session 的 `_begin_agent_run()` 中应同样调用 context provider 收集上下文并注入。

---

## 3. 优化优先级汇总

| 优先级 | 编号 | 优化项 | 消除重复行数 | 实施难度 |
|--------|------|--------|-------------|---------|
| 🔴 高 | 2.2 | 工具注册公共工厂 | ~90 行 | 低 |
| 🔴 高 | 2.1 | Session 公共基类 | ~80 行 | 中 |
| 🟡 中 | 2.3 | 计划管理模型统一 | 架构收敛 | 高 |
| 🟡 中 | 2.4 | 存储层基类 | ~60 行 | 中 |
| 🟢 低 | 2.5 | NEXT UI 移入子目录 | 0（纯移动） | 低 |
| 🟢 低 | 2.6 | Dock 层解耦 | 架构改进 | 中 |
| 🟢 低 | 2.7 | Prompt 管理统一 | 0（合并） | 低 |
| 🟢 低 | 2.8 | Context Provider 注入统一 | ~5 行 | 低 |

---

## 4. 实施路线建议

### 第一阶段 (低风险、高收益)
1. **2.2 工具注册公共工厂** — 创建 `tools/ai_tool_factory.h`，两边统一调用
2. **2.7 Prompt 管理统一** — 合并到一个文件
3. **2.8 Context 注入补齐** — NEXT Session 注入 context documents

### 第二阶段 (需要一定重构)
4. **2.1 Session 公共基类** — 需仔细设计接口，保证两个 Session 的现有行为不变
5. **2.4 存储层基类** — 提取公共文件操作

### 第三阶段 (架构级改进)
6. **2.5 NEXT UI 移入子目录** — 主要涉及路径更新
7. **2.6 Dock 层解耦** — 引入模式面板接口
8. **2.3 计划管理统一** — 需评估兼容性影响

---

## 5. 关键文件索引

### Normal 模式核心文件
| 文件 | 行数 | 说明 |
|------|------|------|
| `agent/ai_agent_base.h/cpp` | 71 / - | Agent 基类 |
| `agent/ai_main_agent.h/cpp` | 24 / 153 | 主 Agent |
| `agent/ai_agent_session.h/cpp` | 109 / 735 | 会话管理 |
| `planning/ai_plan_manager.h/cpp` | 52 / - | 计划管理 |
| `planning/ai_manage_plan_tool.h/cpp` | 20 / - | 计划工具 |
| `storage/ai_conversation_store.h/cpp` | - / - | 对话存储 |
| `prompts/agent_system_prompt.h` | 16 | 系统提示词 |
| `ui/ai_agent_dock.h/cpp` | 97 / - | 主面板 |

### NEXT 模式核心文件
| 文件 | 行数 | 说明 |
|------|------|------|
| `next/agents/ai_next_agents.h/cpp` | 65 / 176 | 5个NEXT Agent |
| `next/ai_agent_next_session.h/cpp` | 149 / 1566 | NEXT 会话管理 |
| `next/ai_next_project_state.h/cpp` | 87 / - | 项目状态 |
| `next/ai_next_workflow_store.h/cpp` | 39 / - | 工作流存储 |
| `next/ai_next_project_store.h/cpp` | 31 / - | 项目存储 |
| `next/ai_next_workflow_snapshot.h` | 82 | 快照/检查点 |
| `next/ai_next_prompts.h/cpp` | 13 / - | NEXT 提示词 |
| `next/ai_next_manage_project_tool.h/cpp` | 29 / - | 项目管理工具 |
| `next/ai_next_event_log.h/cpp` | - / - | 事件日志 |
| `ui/ai_agent_next_dock.h/cpp` | 29 / - | NEXT 面板 |
