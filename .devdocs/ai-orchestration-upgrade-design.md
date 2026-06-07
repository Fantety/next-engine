# 多 Agent 协作架构升级设计文档

> **状态**: 设计中  
> **日期**: 2026-05-29  
> **关联审查**: `docs/ai-multi-agent-architecture-review.md`  
> **目标**: 在现有架构上以最小改动实现增量式里程碑开发、层级任务调度、共享上下文和自动化审查闭环

---

## 目录

1. [设计目标](#1-设计目标)
2. [现有架构复用分析](#2-现有架构复用分析)
3. [改造总览：改什么、不改什么](#3-改造总览改什么不改什么)
4. [新组件设计](#4-新组件设计)
   - [4.1 TaskGraph：层级任务数据结构](#41-taskgraph层级任务数据结构)
   - [4.2 Planning Agent](#42-planning-agent)
   - [4.3 Review Agent](#43-review-agent)
   - [4.4 SharedContext：跨 Agent 公共上下文](#44-sharedcontext跨-agent-公共上下文)
   - [4.5 升级 SubAgentService：依赖感知调度](#45-升级-subagentservice依赖感知调度)
   - [4.6 超级面板 UI](#46-超级面板-ui)
5. [完整工作流](#5-完整工作流)
6. [数据流与接口](#6-数据流与接口)
7. [实现路线图](#7-实现路线图)

---

## 1. 设计目标

### 1.1 核心目标

| 目标 | 说明 |
|------|------|
| **增量式里程碑开发** | 不再"全部规划完再全部执行"，改为逐里程碑：规划 → 执行 → 审查 → 确认 |
| **层级任务调度** | 任务支持父子关系、Agent 分配、依赖约束，主 Agent 按拓扑顺序调度 |
| **跨 Agent 共享上下文** | 所有 Agent 能看到已完成任务的结果摘要、关键决策、产生的文件列表 |
| **自动化审查闭环** | 每个里程碑完成后自动触发 Review Agent，产出修复清单，自动二次调度 |
| **全程可视化** | 超级面板展示任务树、Agent 运行状态、实时进度、审查结果 |

### 1.2 约束

- **最小改动原则**: 尽量复用现有 `AIAgentRuntime`、`AIAgentSession`、`AISubAgentService`、`AIToolRegistry`
- **不破坏现有功能**: 当前单轮对话模式继续可用，新能力作为增强叠加
- **Agent 注册可扩展**: Planning/Review Agent 通过 SubAgentRegistry 注册，不硬编码特殊逻辑

---

## 2. 现有架构复用分析

### 2.1 可直接复用的组件

| 组件 | 复用方式 | 说明 |
|------|---------|------|
| `AIAgentRuntime` | 零改动 | 核心 LLM 循环不变，Planning/Review Agent 直接复用 |
| `AIAgentRuntimeRunner` | 零改动 | 线程模型不变 |
| `AIAgentSession` | 小幅扩展 | 增加里程碑状态、TaskGraph 引用 |
| `AISubAgentService` | 升级 | 增加依赖感知调度（见 4.5） |
| `AISubAgentRegistry` | 扩展 | 新增 Planning/Review Agent 定义 |
| `AIToolRegistry` | 零改动 | 工具注册机制不变 |
| `AIToolPermissionPolicy` | 零改动 | 权限模型不变 |
| `AIContextManager` | 零改动 | 上下文预算控制不变，SharedContext 作为新的 ContextProvider 接入 |
| `AISubAgentRunStore` | 零改动 | 持久化机制不变 |
| `AIChangeSetStore` | 零改动 | 变更审查不变 |
| `AIPlanPanel` | 升级 | 扩展为超级面板 |

### 2.2 需要新增的组件

| 组件 | 类型 | 说明 |
|------|------|------|
| `AITaskGraph` | 数据结构 | 层级任务树，替代现有扁平 `AIPlanTask` |
| `AISharedContext` | 服务 (RefCounted) | 跨 Agent 公共上下文，提供读写接口 |
| `AIPlanningAgent` | Agent 定义 | 注册到 SubAgentRegistry，负责需求分析→任务分解 |
| `AIReviewAgent` | Agent 定义 | 注册到 SubAgentRegistry，负责代码审查→修复清单 |
| `AIMilestonePanel` | UI | 超级面板，替换/升级现有 PlanPanel |

---

## 3. 改造总览：改什么、不改什么

```
现有架构                              改造后
══════════════════════              ══════════════════════

AIAgentSession                      AIAgentSession (小幅扩展)
  ├── AIAgentRuntime                  ├── AIAgentRuntime (不变)
  ├── AISubAgentService               ├── AISubAgentService (升级)
  │     ├── Shader Agent              │     ├── Shader Agent (不变)
  │     ├── Scene Agent               │     ├── Scene Agent (不变)
  │     └── Script Agent              │     ├── Script Agent (不变)
  ├── PlanManager (单层任务)          │     ├── Planning Agent (新增)
  ├── ContextProviders[]              │     └── Review Agent (新增)
  └── PlanPanel (简单列表)            ├── AITaskGraph (新增，替代 PlanManager)
                                      ├── AISharedContext (新增)
                                      ├── ContextProviders[] + SharedContextProvider (新增)
                                      └── AIMilestonePanel (新增，替代 PlanPanel)
```

**改动量估算**:
- 修改文件: ~6 个 (SubAgentRegistry, SubAgentService, Session, ContextProvider 注册, PlanPanel, SCsub)
- 新增文件: ~8 个 (TaskGraph, SharedContext, PlanningAgent 定义, ReviewAgent 定义, PlanningAgent 工具, ReviewAgent 工具, SharedContextProvider, MilestonePanel)
- 总代码量: ~2500 行新增 + ~300 行修改

---

## 4. 新组件设计

### 4.1 TaskGraph：层级任务数据结构

**动机**: 现有 `AIPlanManager` 只支持扁平的 `pending/in_progress/completed` 任务列表，无法表达父子关系和 Agent 分配。

**设计**:

```cpp
// editor/ai_component/planning/ai_task_graph.h

enum AITaskStatus {
    AI_TASK_PENDING,
    AI_TASK_BLOCKED,       // 新增：等待依赖完成
    AI_TASK_READY,         // 新增：依赖已满足，等待调度
    AI_TASK_IN_PROGRESS,
    AI_TASK_COMPLETED,
    AI_TASK_FAILED,
    AI_TASK_SKIPPED,
};

struct AITaskNode {
    String task_id;              // 唯一标识，如 "milestone_1.task_3"
    String title;                // 人类可读标题
    String description;          // 详细描述（供 Agent 理解）
    String assigned_agent;       // "scene_agent" / "script_agent" / "shader_agent"
    String parent_task_id;       // 父任务 ID，为空则是顶层任务
    Vector<String> depends_on;   // 依赖的任务 ID 列表
    AITaskStatus status;
    
    // 运行时信息
    String sub_agent_run_id;     // 对应的 SubAgent run_id（执行后填充）
    String result_summary;       // 执行结果摘要（供共享上下文）
    Vector<String> produced_files; // 产生的文件列表
    
    uint64_t created_at;
    uint64_t started_at;
    uint64_t completed_at;
    
    Dictionary to_dict() const;
    static AITaskNode from_dict(const Dictionary &p_dict);
};

struct AIMilestone {
    String milestone_id;
    String title;
    String description;
    Vector<AITaskNode> tasks;
    String status;  // "planning" | "executing" | "reviewing" | "completed"
    
    // 审查相关
    String review_run_id;
    Vector<String> review_findings;  // Review Agent 的发现列表
    int fix_iteration;               // 修复轮次计数
    
    Dictionary to_dict() const;
    static AIMilestone from_dict(const Dictionary &p_dict);
};

class AITaskGraph : public RefCounted {
    GDCLASS(AITaskGraph, RefCounted);
    
    String active_milestone_id;
    HashMap<String, AIMilestone> milestones;
    
    // 拓扑排序
    Vector<String> _topological_order(const Vector<AITaskNode> &p_tasks) const;
    bool _all_dependencies_satisfied(const AITaskNode &p_task, const Vector<AITaskNode> &p_all_tasks) const;
    
public:
    // 里程碑管理
    String create_milestone(const String &p_title, const String &p_description);
    void add_task(const String &p_milestone_id, const AITaskNode &p_task);
    
    // 调度查询（主 Agent 用来决定下一步激活哪个子 Agent）
    Vector<AITaskNode> get_ready_tasks(const String &p_milestone_id) const;
    AITaskNode get_next_task(const String &p_milestone_id) const;
    
    // 状态更新
    void mark_task_status(const String &p_task_id, AITaskStatus p_status);
    void set_task_result(const String &p_task_id, const String &p_run_id, const String &p_summary, const Vector<String> &p_files);
    
    // 审查
    void add_review_findings(const String &p_milestone_id, const Vector<String> &p_findings);
    Vector<AITaskNode> generate_fix_tasks(const String &p_milestone_id, const Vector<String> &p_findings);
    
    // 序列化
    Dictionary to_dict() const;
    void from_dict(const Dictionary &p_dict);
};
```

**关键行为**:
1. `get_ready_tasks()`: 返回所有依赖已满足且状态为 PENDING 的任务 → 主 Agent 据此决定并行启动哪些子 Agent
2. `generate_fix_tasks()`: Review Agent 发现的问题自动转为修复任务，追加到当前里程碑
3. 循环检测: `_topological_order()` 检测循环依赖并拒绝

---

### 4.2 Planning Agent

**定位**: 一个标准的子 Agent，专门负责需求分析 → 任务分解 → 产出结构化 TaskGraph。

**注册**（在 `ai_sub_agent_registry.cpp` 中新增）:

```cpp
AISubAgentDefinition make_planning_agent() {
    AISubAgentDefinition def;
    def.agent_id = "planning_agent";
    def.display_name = "Planning Agent";
    def.description = "Analyzes requirements and produces structured, dependency-aware task plans for game development milestones.";
    def.instruction = R"(
You are a game development planning specialist. Your role is to:
1. Analyze the user's feature request in the context of the existing project
2. Break it down into concrete, dependency-ordered tasks
3. Assign each task to the most appropriate specialist agent
4. Output a structured plan using the planning tools

When planning:
- Read project context first (use project.* tools to understand existing code)
- Tasks should be small enough that one agent can complete them in one session
- Identify dependencies: data models before UI, base classes before derived
- Prefer parallel execution where possible
- Each task must have a clear acceptance criterion
- Use the agent.manage_plan tool to publish the plan
)";
    def.owned_tools.insert("agent.manage_plan");  // 规划专用工具（升级版）
    def.max_provider_turns = 30;
    def.max_tool_calls = 20;
    return def;
}
```

**可用工具**:
- 共享只读工具（project.list_tree, project.read_file, project.search_text, editor.get_context, scene.list_properties, script.inspect）
- `agent.manage_plan`（升级版，支持层级任务创建）

**输入/输出**:
- 输入: 用户里程碑描述 + 项目上下文
- 输出: 填充好的 TaskGraph（写入 AITaskGraph，通过 SharedContext 传递回主 Agent）

---

### 4.3 Review Agent

**定位**: 一个标准的子 Agent，专门负责代码审查 → 产出修复清单。

**注册**:

```cpp
AISubAgentDefinition make_review_agent() {
    AISubAgentDefinition def;
    def.agent_id = "review_agent";
    def.display_name = "Review Agent";
    def.description = "Reviews code changes from a milestone and produces actionable fix reports.";
    def.instruction = R"(
You are a code review specialist for a Godot-based game project. Your role:
1. Read all files changed during the current milestone (from shared context)
2. Check for: compilation errors, GDScript best practices violations, 
   scene/node naming consistency, signal connection correctness, 
   resource path validity
3. Produce a structured fix report with:
   - Severity (error/warning/suggestion)
   - File path and line reference
   - Problem description
   - Recommended fix
   - Which agent should fix it (script_agent / scene_agent / shader_agent)

Use the agent.manage_plan tool to publish findings as fix tasks.
)";
    def.owned_tools.insert("agent.manage_plan");
    def.max_provider_turns = 20;
    def.max_tool_calls = 20;
    return def;
}
```

**可用工具**:
- 共享只读工具（同 Planning Agent）
- `agent.manage_plan`（用于输出修复任务）

**输入/输出**:
- 输入: SharedContext 中的 `produced_files` 列表 + 当前代码状态
- 输出: 修复任务列表（写入 TaskGraph 的 `review_findings`，主 Agent 据此生成修复子任务）

---

### 4.4 SharedContext：跨 Agent 公共上下文

**动机**: 现有 `AISubAgentContextProvider` 只在子 Agent 启动时单向传递少量上下文。需要一个所有 Agent 都能读写的共享信息空间，让 Agent 之间能感知彼此的工作成果。

**设计**:

```cpp
// editor/ai_component/context/ai_shared_context.h

class AISharedContext : public RefCounted {
    GDCLASS(AISharedContext, RefCounted);
    
    struct MilestoneContext {
        String milestone_id;
        String summary;                          // 里程碑总体描述
        Vector<String> completed_task_summaries;  // 已完成任务的摘要
        Vector<String> produced_file_paths;       // 所有产生的文件
        Dictionary key_decisions;                 // 关键决策记录
    };
    
    MilestoneContext current_milestone;
    Vector<MilestoneContext> completed_milestones; // 历史里程碑摘要
    
    static inline Ref<AISharedContext> singleton;
    
public:
    static Ref<AISharedContext> get_singleton();
    
    // 写接口（子 Agent 完成后调用）
    void record_task_completion(const String &p_task_title, const String &p_summary, const Vector<String> &p_files);
    void record_decision(const String &p_key, const String &p_value);
    void record_milestone_summary(const String &p_summary);
    
    // 读接口（作为 ContextProvider 供所有 Agent 使用）
    String build_context_text() const;           // 生成紧凑的上下文文本
    Vector<String> get_all_produced_files() const; // 里程碑产出的所有文件
    
    // 里程碑生命周期
    void start_milestone(const String &p_id, const String &p_title);
    void complete_milestone();
    
    // 序列化（崩溃恢复）
    Dictionary to_dict() const;
    void from_dict(const Dictionary &p_dict);
};

// 作为 ContextProvider 接入现有系统
class AISharedContextProvider : public AIContextProvider {
    GDCLASS(AISharedContextProvider, AIContextProvider);
    
public:
    virtual Array collect_context() override {
        Array docs;
        Ref<AISharedContext> ctx = AISharedContext::get_singleton();
        if (ctx.is_valid()) {
            Dictionary doc;
            doc["source"] = "shared_context";
            doc["content"] = ctx->build_context_text();
            docs.push_back(doc);
        }
        return docs;
    }
};
```

**上下文文本格式**（Agent 看到的内容）:

```
[Shared Context - Current Milestone: "背包系统"]
Completed tasks (3/5):
✅ [scene_agent] 创建背包UI场景 → 产出: res://scenes/inventory.tscn
✅ [script_agent] 物品数据模型 → 产出: res://scripts/item_data.gd, res://scripts/item_database.gd
✅ [script_agent] 背包逻辑脚本 → 产出: res://scripts/inventory.gd
⏳ [script_agent] 物品拖拽交互 (in progress)
⏳ [shader_agent] 物品稀有度边框着色器 (pending)

Key decisions:
- 物品数据使用 Resource 类型存储
- 背包容量上限: 30 格
- 物品稀有度: Common/Uncommon/Rare/Epic/Legendary
```

**接入点**: 在所有 Agent 的 `build_tool_registry()` 和 `build_profile()` 中，将 `AISharedContextProvider` 加入 ContextProviders 列表。无论是主 Agent、Planning Agent、Review Agent 还是专业 Agent，都能看到这份公共上下文。

---

### 4.5 升级 SubAgentService：依赖感知调度

**改动范围**: `ai_sub_agent_service.h/.cpp`（新增方法，不改现有签名）

**新增能力**:

```cpp
class AISubAgentService : public RefCounted {
    // ... 现有接口不变 ...
    
    // === 新增：里程碑调度 ===
    
    // 按依赖顺序激活准备就绪的任务
    // 返回激活的 run_id 列表
    Vector<String> dispatch_ready_tasks(const String &p_milestone_id);
    
    // 当子 Agent 完成时，检查是否有新的就绪任务
    // 如果有，自动激活（或在回调中返回给主 Agent 决定）
    Vector<AITaskNode> on_sub_agent_completed(const String &p_run_id);
    
    // 查询里程碑进度
    Dictionary get_milestone_progress(const String &p_milestone_id) const;
    
    // 激活审查流程
    String start_milestone_review(const String &p_milestone_id);
    
private:
    Ref<AITaskGraph> task_graph;          // 新增引用
    Ref<AISharedContext> shared_context;  // 新增引用
    
    void _on_sub_agent_run_completed(const String &p_run_id);
    void _try_dispatch_next(const String &p_milestone_id);
};
```

**调度流程**:

```
_sub_on_agent_run_completed(run_id):
    1. 从 task_graph 找到 run_id 对应的 task_node
    2. 将 task_node 状态标记为 COMPLETED
    3. 将结果写入 shared_context (summary + produced_files)
    4. 调用 task_graph.get_ready_tasks(milestone_id) 
    5. 如果有就绪任务 → dispatch_ready_tasks(milestone_id)
    6. 如果全部完成 → 通知主 Agent 进入审查阶段
```

**主 Agent 的编排工具（升级 `agent.manage_plan`）**:

主 Agent 通过工具调用来驱动整个流程，而不是在代码中硬编码流程：

```
工具: agent.manage_plan (升级版)

新增操作:
- create_milestone(title, description) → milestone_id
- add_task(milestone_id, title, description, agent, depends_on[], parent_id)
- dispatch_milestone(milestone_id) → 开始执行
- get_milestone_status(milestone_id) → 进度报告
- start_review(milestone_id) → 触发 Review Agent
- get_review_findings(milestone_id) → 获取审查结果
- apply_fix_tasks(milestone_id) → 将审查发现转为修复任务并重新调度
```

---

### 4.6 超级面板 UI

**定位**: 升级现有 `AIPlanPanel`，展示里程碑级别的任务管理和 Agent 状态。

**布局设计**:

```
┌─────────────────────────────────────────────────────┐
│  超级面板 - 背包系统                          [×]    │
├─────────────────────────────────────────────────────┤
│  里程碑: 背包系统                   状态: 执行中     │
│  进度: ████████░░░░░░░░ 3/5 任务完成               │
│─────────────────────────────────────────────────────│
│  📋 任务树                                          │
│                                                     │
│  ✅ 物品数据模型              [script_agent] 完成   │
│     └─ 产出: item_data.gd, item_database.gd         │
│  ✅ 创建背包UI场景            [scene_agent]  完成   │
│     └─ 产出: inventory.tscn                         │
│  ⏳ 物品拖拽交互              [script_agent] 进行中 │
│  ⏸️ 物品稀有度边框着色器     [shader_agent] 等待中  │
│     └─ 依赖: 物品数据模型 ✅                         │
│  ⏸️ 背包容量扩展逻辑         [script_agent] 等待中  │
│─────────────────────────────────────────────────────│
│  🤖 Agent 运行状态                                  │
│                                                     │
│  script_agent  ████████░░  运行中 (拖拽交互)        │
│  scene_agent   ✅         空闲                      │
│  shader_agent  ⏸️         等待调度                  │
│─────────────────────────────────────────────────────│
│  🔍 审查 (上一轮)                                   │
│  ⚠️  item_data.gd:23 - 未使用的变量 'temp'         │
│  ℹ️  inventory.gd:45 - 建议添加类型注解            │
│─────────────────────────────────────────────────────│
│  [📋 查看完整 TaskGraph] [🔄 重新调度] [▶️ 启动审查] │
└─────────────────────────────────────────────────────┘
```

**实现**:

```cpp
// editor/ai_component/ui/ai_milestone_panel.h

class AIMilestonePanel : public Control {
    GDCLASS(AIMilestonePanel, Control);
    
    Ref<AITaskGraph> task_graph;
    Ref<AISharedContext> shared_context;
    
    // UI 组件
    Tree *task_tree;              // 任务树（替代列表）
    Label *milestone_title;
    ProgressBar *progress_bar;
    Container *agent_status_area; // Agent 状态条
    Container *review_findings;   // 审查发现列表
    
    void _refresh_task_tree();
    void _refresh_agent_status();
    void _refresh_review_findings();
    void _on_task_selected();
    
public:
    void set_task_graph(const Ref<AITaskGraph> &p_graph);
    void set_shared_context(const Ref<AISharedContext> &p_ctx);
    void refresh(); // 定时轮询刷新（1s 间隔）
};
```

**刷新机制**: 使用 `SceneTreeTimer` 每秒轮询 `task_graph->get_milestone_progress()` 和 `SubAgentService` 的运行状态，增量更新 UI。

---

## 5. 完整工作流

### 5.1 里程碑生命周期

```
┌──────────────────────────────────────────────────────────────┐
│                      MILESTONE LIFECYCLE                      │
├──────────────────────────────────────────────────────────────┤
│                                                               │
│  用户输入                                                     │
│  "帮我实现背包系统，包含物品栏UI、拖拽、稀有度显示"           │
│     │                                                         │
│     ▼                                                         │
│  ┌──────────┐                                                │
│  │ PLANNING │  主Agent → 激活 planning_agent                  │
│  │          │  planning_agent 读取项目上下文                  │
│  │          │  → 产出 TaskGraph (写入 shared_context)        │
│  │          │  → 返回摘要给主Agent                            │
│  └────┬─────┘                                                │
│       │                                                       │
│       ▼                                                       │
│  ┌──────────────┐                                            │
│  │ USER CONFIRM │  主Agent展示 TaskGraph → 等待用户确认      │
│  │ (human gate) │  用户可修改/调整/确认                      │
│  └──────┬───────┘                                            │
│         │                                                     │
│         ▼                                                     │
│  ┌───────────┐                                               │
│  │ EXECUTING │  主Agent: dispatch_milestone()                │
│  │           │  SubAgentService 按拓扑顺序调度:               │
│  │           │  Round 1: 并行 [scene_agent, script_agent]    │
│  │           │  Round 2: script_agent (依赖round1完成)       │
│  │           │  Round 3: shader_agent (依赖round2完成)       │
│  │           │  每完成一个: shared_context 更新 + UI 刷新    │
│  └─────┬─────┘                                              │
│        │                                                      │
│        ▼ (全部任务完成)                                       │
│  ┌───────────┐                                               │
│  │ REVIEWING │  主Agent: start_review()                      │
│  │           │  → 激活 review_agent                          │
│  │           │  → review_agent 读取 shared_context           │
│  │           │  → 产出 fix_tasks (写入 TaskGraph)            │
│  └─────┬─────┘                                              │
│        │                                                      │
│        ▼                                                      │
│  ┌──────────┐                                                │
│  │ FIXING   │  有修复项?                                     │
│  │ (loop)   │  → 主Agent: apply_fix_tasks()                  │
│  │          │  → 重新调度修复任务                            │
│  │          │  → 完成后再次 start_review()                   │
│  │          │  → 最多3轮，超出则请求用户介入                 │
│  └─────┬────┘                                                │
│        │ (无修复项 或 达到最大轮次)                           │
│        ▼                                                      │
│  ┌───────────┐                                               │
│  │ COMPLETED │  主Agent 报告结果                             │
│  │           │  用户确认 → 里程碑归档                        │
│  │           │  shared_context 转到 completed_milestones    │
│  └───────────┘                                               │
│                                                               │
└──────────────────────────────────────────────────────────────┘
```

### 5.2 主 Agent 的编排 Prompt（关键）

主 Agent 的系统提示词需要更新，明确其**纯编排角色**：

```
You are the Main Orchestrator Agent. You do NOT write code or modify scenes directly.
Your role is to manage milestones and coordinate specialist agents.

Available orchestration tools:
- agent.activate_sub_agent("planning_agent", task="...")  - Start planning
- agent.activate_sub_agent("script_agent", task="...")    - Delegate script work
- agent.activate_sub_agent("scene_agent", task="...")     - Delegate scene work
- agent.manage_plan - Create/manage task graphs
- agent.get_sub_agent_status - Check progress
- agent.resume_sub_agent - Resume interrupted work

Workflow for each milestone:
1. Activate planning_agent with the user's request
2. Present the plan to the user, wait for confirmation
3. Dispatch tasks using manage_plan
4. Monitor progress, handle failures
5. When all tasks complete, start review
6. If review finds issues: create fix tasks, re-dispatch, re-review (max 3 iterations)
7. Report completion

You have access to shared context showing all completed work and key decisions.
Use it to avoid redundant work and maintain consistency across the project.
```

---

## 6. 数据流与接口

### 6.1 Agent 间通信

```
                    ┌──────────────────┐
                    │   SharedContext   │ (Singleton, 所有Agent可读写)
                    │  ───────────────  │
                    │  completed_tasks  │
                    │  produced_files   │
                    │  key_decisions    │
                    └───┬──────┬───────┘
            ▲ 写入      │      │  读取
            │           │      │
    ┌───────┴──┐  ┌────┴──────┴───┐  ┌──────────┐
    │ 子Agent  │  │  主 Agent      │  │ 审查Agent│
    │ (执行)   │  │  (编排)        │  │ (审查)   │
    └──────────┘  └───────┬───────┘  └──────────┘
                          │
                   ┌──────┴───────┐
                   │  TaskGraph    │ (主Agent管理，所有Agent通过Context看到摘要)
                   │  ───────────  │
                   │  milestones   │
                   │  task_nodes   │
                   │  dependencies │
                   └──────────────┘
```

### 6.2 与现有 Session 的集成

```cpp
// AIAgentSession 的扩展（最小改动）

class AIAgentSession : public Node {
    // ... 现有成员 ...
    
    // 新增
    Ref<AITaskGraph> task_graph;
    Ref<AISharedContext> shared_context;
    
    // 在 configure_provider() 或初始化时设置
    void _init_orchestration_layer();
    
    // milestone 模式判断
    bool is_milestone_mode() const;  // 当 task_graph 有 active milestone 时
    
    // 子 Agent 完成回调增强
    void _on_sub_agent_completed(const String &p_run_id) {
        // 原有逻辑...
        // 新增: 更新 task_graph + shared_context
        if (task_graph.is_valid()) {
            sub_agent_service->on_sub_agent_completed(p_run_id);
            _emit_milestone_progress_update();
        }
    }
};
```

### 6.3 工具注册

Planning Agent 和 Review Agent 的工具注册类似现有专业 Agent：

```cpp
// AISubAgentService::build_tool_registry() 扩展

Ref<AIToolRegistry> AISubAgentService::build_tool_registry(const String &p_agent_id) const {
    Ref<AIToolRegistry> registry;
    registry.instantiate();
    
    if (p_agent_id == "planning_agent" || p_agent_id == "review_agent") {
        // 共享只读工具
        HashSet<String> shared_tools = AISubAgentRegistry::get_shared_read_tools();
        for (auto &tool : shared_tools) {
            register_builtin_tool_by_name(registry, tool);
        }
        // 规划/审查专用工具
        register_tool<AIManagePlanTool>(registry);
        
        // 共享上下文工具（读取 SharedContext）
        register_tool<AISharedContextReadTool>(registry);
    }
    
    // ... 现有专业 Agent 的逻辑不变 ...
    
    return registry;
}
```

---

## 7. 实现路线图

### Phase 1: 数据层（1-2 周）

| 步骤 | 内容 | 文件 |
|------|------|------|
| 1.1 | 实现 `AITaskGraph` 数据结构 | `planning/ai_task_graph.h/.cpp` 新增 |
| 1.2 | 实现 `AISharedContext` 服务 | `context/ai_shared_context.h/.cpp` 新增 |
| 1.3 | 实现 `AISharedContextProvider` | `context/ai_shared_context.h` 新增 |
| 1.4 | 单元测试：TaskGraph 拓扑排序、依赖检测 | `tests/editor/test_ai_task_graph.cpp` 新增 |
| 1.5 | 单元测试：SharedContext 读写、序列化 | `tests/editor/test_ai_shared_context.cpp` 新增 |

### Phase 2: Agent 注册（1 周）

| 步骤 | 内容 | 文件 |
|------|------|------|
| 2.1 | 注册 Planning Agent 定义 | `ai_sub_agent_registry.cpp` 修改 |
| 2.2 | 注册 Review Agent 定义 | `ai_sub_agent_registry.cpp` 修改 |
| 2.3 | 升级 `agent.manage_plan` 工具支持层级任务 | `planning/ai_manage_plan_tool.cpp` 修改 |
| 2.4 | 集成 SharedContextProvider 到上下文链 | `ai_sub_agent_service.cpp` 修改 |
| 2.5 | 测试：Planning Agent 任务分解流程 | 扩展现有 `test_ai_sub_agents.cpp` |

### Phase 3: 调度引擎（1-2 周）

| 步骤 | 内容 | 文件 |
|------|------|------|
| 3.1 | `dispatch_ready_tasks()` 依赖感知调度 | `ai_sub_agent_service.cpp` 修改 |
| 3.2 | 子 Agent 完成 → 自动触发下一个 | `ai_sub_agent_service.cpp` 修改 |
| 3.3 | 审查触发 → 修复任务生成 → 二次调度 | `ai_sub_agent_service.cpp` 修改 |
| 3.4 | 主 Agent 编排 Prompt 更新 | `prompts/agent_system_prompt.h` 修改 |
| 3.5 | 集成测试：完整里程碑工作流 | 新增测试 |

### Phase 4: UI（1-2 周）

| 步骤 | 内容 | 文件 |
|------|------|------|
| 4.1 | `AIMilestonePanel` 任务树 + Agent 状态 | `ui/ai_milestone_panel.h/.cpp` 新增 |
| 4.2 | 审查发现展示 | 同上 |
| 4.3 | 轮询刷新机制 | 同上 |
| 4.4 | 替换/升级现有 PlanPanel | `ui/ai_agent_dock.cpp` 修改 |
| 4.5 | UI 测试 | 新增测试 |

### Phase 5: 打磨（1 周）

| 步骤 | 内容 |
|------|------|
| 5.1 | 崩溃恢复：重启后恢复中断的里程碑 |
| 5.2 | 修复轮次上限（最多3轮，超出请求用户介入） |
| 5.3 | 里程碑归档和历史查看 |
| 5.4 | 性能优化：大型 TaskGraph 的 UI 渲染 |

---

**总估算**: 6-8 周，与现有功能完全兼容，不影响当前单轮对话模式。

---

*本文档待用户审阅确认后进入实现阶段。*
