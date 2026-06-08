# Multi-Agent Orchestration Architecture Upgrade Design

> **Status**: Draft  
> **Date**: 2026-05-29  
> **Related**: `docs/ai-multi-agent-architecture-review.md`  
> **Goal**: Achieve incremental milestone-based development, hierarchical task scheduling, shared context, and automated review loops with minimal changes to the existing architecture

---

## Table of Contents

1. [Design Goals](#1-design-goals)
2. [Existing Architecture Reuse Analysis](#2-existing-architecture-reuse-analysis)
3. [Change Overview: What Changes, What Stays](#3-change-overview-what-changes-what-stays)
4. [New Component Designs](#4-new-component-designs)
   - [4.1 TaskGraph: Hierarchical Task Data Structure](#41-taskgraph-hierarchical-task-data-structure)
   - [4.2 Planning Agent](#42-planning-agent)
   - [4.3 Review Agent](#43-review-agent)
   - [4.4 SharedContext: Cross-Agent Common Context](#44-sharedcontext-cross-agent-common-context)
   - [4.5 Upgraded SubAgentService: Dependency-Aware Scheduling](#45-upgraded-subagentservice-dependency-aware-scheduling)
   - [4.6 Super Panel UI](#46-super-panel-ui)
5. [Complete Workflow](#5-complete-workflow)
6. [Data Flow and Interfaces](#6-data-flow-and-interfaces)
7. [Implementation Roadmap](#7-implementation-roadmap)

---

## 1. Design Goals

### 1.1 Core Goals

| Goal | Description |
|------|-------------|
| **Incremental Milestone Development** | Replace "plan everything then execute all" with per-milestone cycles: Plan → Execute → Review → Confirm |
| **Hierarchical Task Scheduling** | Tasks support parent-child relationships, agent assignment, and dependency constraints; orchestrator schedules in topological order |
| **Cross-Agent Shared Context** | All agents can see completed task summaries, key decisions, and produced files |
| **Automated Review Loop** | Review Agent triggered automatically after each milestone, produces fix reports, auto re-schedules |
| **Full Visibility** | Super Panel displays task tree, agent run status, real-time progress, and review findings |

### 1.2 Constraints

- **Minimal Changes**: Maximize reuse of existing `AIAgentRuntime`, `AIAgentSession`, `AISubAgentService`, `AIToolRegistry`
- **No Breakage**: Current single-turn chat mode continues to work; new capabilities are additive
- **Extensible Agent Registration**: Planning/Review agents registered via `SubAgentRegistry`, no special-case logic

---

## 2. Existing Architecture Reuse Analysis

### 2.1 Components Reused As-Is

| Component | Reuse | Notes |
|-----------|-------|-------|
| `AIAgentRuntime` | Zero changes | Core LLM loop unchanged; Planning/Review agents reuse directly |
| `AIAgentRuntimeRunner` | Zero changes | Thread model unchanged |
| `AIAgentSession` | Minor extension | Add milestone state, TaskGraph reference |
| `AISubAgentService` | Upgraded | Add dependency-aware scheduling (see 4.5) |
| `AISubAgentRegistry` | Extended | Add Planning/Review agent definitions |
| `AIToolRegistry` | Zero changes | Tool registration mechanism unchanged |
| `AIToolPermissionPolicy` | Zero changes | Permission model unchanged |
| `AIContextManager` | Zero changes | Budget control unchanged; SharedContext plugs in as new ContextProvider |
| `AISubAgentRunStore` | Zero changes | Persistence mechanism unchanged |
| `AIChangeSetStore` | Zero changes | Change review unchanged |
| `AIPlanPanel` | Upgraded | Extended into Super Panel |

### 2.2 New Components

| Component | Type | Purpose |
|-----------|------|---------|
| `AITaskGraph` | Data structure (RefCounted) | Hierarchical task tree, replaces flat `AIPlanTask` |
| `AISharedContext` | Service (RefCounted, Singleton) | Cross-agent common context with read/write interface |
| Planning Agent | Agent definition | Registered in SubAgentRegistry; requirement analysis → task decomposition |
| Review Agent | Agent definition | Registered in SubAgentRegistry; code review → fix reports |
| `AIMilestonePanel` | UI | Super Panel, replaces/upgrades existing PlanPanel |

---

## 3. Change Overview: What Changes, What Stays

```
Current Architecture                  Upgraded Architecture
══════════════════════              ═══════════════════════════

AIAgentSession                      AIAgentSession (minor extension)
  ├── AIAgentRuntime                  ├── AIAgentRuntime (unchanged)
  ├── AISubAgentService               ├── AISubAgentService (upgraded)
  │     ├── Shader Agent              │     ├── Shader Agent (unchanged)
  │     ├── Scene Agent               │     ├── Scene Agent (unchanged)
  │     └── Script Agent              │     ├── Script Agent (unchanged)
  ├── PlanManager (flat tasks)        │     ├── Planning Agent (new)
  ├── ContextProviders[]              │     └── Review Agent (new)
  └── PlanPanel (simple list)         ├── AITaskGraph (new, replaces PlanManager)
                                      ├── AISharedContext (new)
                                      ├── ContextProviders[] + SharedContextProvider (new)
                                      └── AIMilestonePanel (new, replaces PlanPanel)
```

**Effort Estimate**:
- Files modified: ~6 (SubAgentRegistry, SubAgentService, Session, manage_plan tool, PlanPanel → MilestonePanel, system prompt)
- Files added: ~8 (TaskGraph, SharedContext, SharedContextProvider, Planning agent tools, Review agent tools, MilestonePanel, tests)
- Total: ~2500 lines added + ~300 lines modified

---

## 4. New Component Designs

### 4.1 TaskGraph: Hierarchical Task Data Structure

**Motivation**: Current `AIPlanManager` only supports flat `pending/in_progress/completed` task lists. Cannot express parent-child relationships, agent assignments, or dependency constraints.

**Design**:

```cpp
// editor/ai_component/planning/ai_task_graph.h

enum AITaskStatus {
    AI_TASK_PENDING,
    AI_TASK_BLOCKED,       // New: waiting for dependencies
    AI_TASK_READY,         // New: dependencies met, awaiting dispatch
    AI_TASK_IN_PROGRESS,
    AI_TASK_COMPLETED,
    AI_TASK_FAILED,
    AI_TASK_SKIPPED,
};

struct AITaskNode {
    String task_id;              // Unique ID, e.g. "milestone_1.task_3"
    String title;                // Human-readable title
    String description;          // Detailed description (for agent comprehension)
    String assigned_agent;       // "scene_agent" / "script_agent" / "shader_agent"
    String parent_task_id;       // Parent task ID; empty for top-level tasks
    Vector<String> depends_on;   // Dependency task IDs
    AITaskStatus status;
    
    // Runtime information
    String sub_agent_run_id;     // Corresponding SubAgent run_id (populated after dispatch)
    String result_summary;       // Execution result summary (for shared context)
    Vector<String> produced_files; // Files produced
    
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
    
    // Review-related
    String review_run_id;
    Vector<String> review_findings;  // Review Agent findings
    int fix_iteration;               // Fix loop iteration count
    
    Dictionary to_dict() const;
    static AIMilestone from_dict(const Dictionary &p_dict);
};

class AITaskGraph : public RefCounted {
    GDCLASS(AITaskGraph, RefCounted);
    
    String active_milestone_id;
    HashMap<String, AIMilestone> milestones;
    
    // Topological sort
    Vector<String> _topological_order(const Vector<AITaskNode> &p_tasks) const;
    bool _all_dependencies_satisfied(const AITaskNode &p_task, const Vector<AITaskNode> &p_all_tasks) const;
    
public:
    // Milestone management
    String create_milestone(const String &p_title, const String &p_description);
    void add_task(const String &p_milestone_id, const AITaskNode &p_task);
    
    // Scheduling queries (used by orchestrator to decide which sub-agent to activate next)
    Vector<AITaskNode> get_ready_tasks(const String &p_milestone_id) const;
    AITaskNode get_next_task(const String &p_milestone_id) const;
    
    // Status updates
    void mark_task_status(const String &p_task_id, AITaskStatus p_status);
    void set_task_result(const String &p_task_id, const String &p_run_id, const String &p_summary, const Vector<String> &p_files);
    
    // Review
    void add_review_findings(const String &p_milestone_id, const Vector<String> &p_findings);
    Vector<AITaskNode> generate_fix_tasks(const String &p_milestone_id, const Vector<String> &p_findings);
    
    // Serialization
    Dictionary to_dict() const;
    void from_dict(const Dictionary &p_dict);
};
```

**Key Behaviors**:
1. `get_ready_tasks()`: Returns all tasks with satisfied dependencies and PENDING status → orchestrator uses this to decide which sub-agents to activate in parallel
2. `generate_fix_tasks()`: Converts Review Agent findings into fix tasks appended to the current milestone
3. Cycle detection: `_topological_order()` detects and rejects circular dependencies

---

### 4.2 Planning Agent

**Role**: A standard sub-agent specialized in requirement analysis → task decomposition → structured TaskGraph output.

**Registration** (added to `ai_sub_agent_registry.cpp`):

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
    def.owned_tools.insert("agent.manage_plan");  // Upgraded planning tool
    def.max_provider_turns = 30;
    def.max_tool_calls = 20;
    return def;
}
```

**Available Tools**:
- Shared read tools (project.list_tree, project.read_file, project.search_text, editor.get_context, scene.describe_tree, scene.inspect_node, scene.list_properties, script.inspect)
- `agent.manage_plan` (upgraded, supports hierarchical task creation)

**Input/Output**:
- Input: User milestone description + project context
- Output: Populated TaskGraph (written to AITaskGraph, passed back to orchestrator via SharedContext)

---

### 4.3 Review Agent

**Role**: A standard sub-agent specialized in code review → actionable fix reports.

**Registration**:

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

**Available Tools**:
- Shared read tools (same as Planning Agent)
- `agent.manage_plan` (to output fix tasks)

**Input/Output**:
- Input: `produced_files` list from SharedContext + current code state
- Output: Fix task list (written to TaskGraph `review_findings`; orchestrator generates fix sub-tasks from them)

---

### 4.4 SharedContext: Cross-Agent Common Context

**Motivation**: Current `AISubAgentContextProvider` only passes minimal context one-way at sub-agent startup. We need a shared information space all agents can read/write, enabling cross-agent awareness of each other's work.

**Design**:

```cpp
// editor/ai_component/context/ai_shared_context.h

class AISharedContext : public RefCounted {
    GDCLASS(AISharedContext, RefCounted);
    
    struct MilestoneContext {
        String milestone_id;
        String summary;                          // Milestone overview
        Vector<String> completed_task_summaries;  // Summaries of completed tasks
        Vector<String> produced_file_paths;       // All files produced
        Dictionary key_decisions;                 // Key decision records
    };
    
    MilestoneContext current_milestone;
    Vector<MilestoneContext> completed_milestones; // Historical milestone summaries
    
    static inline Ref<AISharedContext> singleton;
    
public:
    static Ref<AISharedContext> get_singleton();
    
    // Write interface (called by sub-agents upon task completion)
    void record_task_completion(const String &p_task_title, const String &p_summary, const Vector<String> &p_files);
    void record_decision(const String &p_key, const String &p_value);
    void record_milestone_summary(const String &p_summary);
    
    // Read interface (consumed as ContextProvider by all agents)
    String build_context_text() const;           // Generates compact context text
    Vector<String> get_all_produced_files() const; // All files produced in milestone
    
    // Milestone lifecycle
    void start_milestone(const String &p_id, const String &p_title);
    void complete_milestone();
    
    // Serialization (crash recovery)
    Dictionary to_dict() const;
    void from_dict(const Dictionary &p_dict);
};

// Plugs into existing ContextProvider system
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

**Context Text Format** (what agents see):

```
[Shared Context - Current Milestone: "Inventory System"]
Completed tasks (3/5):
✅ [scene_agent] Create inventory UI scene → produced: res://scenes/inventory.tscn
✅ [script_agent] Item data model → produced: res://scripts/item_data.gd, res://scripts/item_database.gd
✅ [script_agent] Inventory logic script → produced: res://scripts/inventory.gd
⏳ [script_agent] Item drag-and-drop interaction (in progress)
⏳ [shader_agent] Item rarity border shader (pending)

Key decisions:
- Items stored as Resource type
- Inventory capacity: 30 slots
- Item rarities: Common/Uncommon/Rare/Epic/Legendary
```

**Integration Point**: In every agent's `build_tool_registry()` and `build_profile()`, `AISharedContextProvider` is added to the ContextProviders list. Whether orchestrator, planning agent, review agent, or specialist agent — all see this shared context.

---

### 4.5 Upgraded SubAgentService: Dependency-Aware Scheduling

**Scope**: `ai_sub_agent_service.h/.cpp` (new methods added, existing signatures unchanged)

**New Capabilities**:

```cpp
class AISubAgentService : public RefCounted {
    // ... existing interface unchanged ...
    
    // === New: Milestone Scheduling ===
    
    // Activate all ready tasks in dependency order
    // Returns list of activated run_ids
    Vector<String> dispatch_ready_tasks(const String &p_milestone_id);
    
    // When a sub-agent completes, check for newly-ready tasks
    // Returns tasks that are now ready (orchestrator may auto-dispatch)
    Vector<AITaskNode> on_sub_agent_completed(const String &p_run_id);
    
    // Query milestone progress
    Dictionary get_milestone_progress(const String &p_milestone_id) const;
    
    // Activate review process
    String start_milestone_review(const String &p_milestone_id);
    
private:
    Ref<AITaskGraph> task_graph;          // New reference
    Ref<AISharedContext> shared_context;  // New reference
    
    void _on_sub_agent_run_completed(const String &p_run_id);
    void _try_dispatch_next(const String &p_milestone_id);
};
```

**Scheduling Flow**:

```
_on_sub_agent_run_completed(run_id):
    1. Find task_node in task_graph matching run_id
    2. Mark task_node status → COMPLETED
    3. Write result to shared_context (summary + produced_files)
    4. Call task_graph.get_ready_tasks(milestone_id)
    5. If ready tasks exist → dispatch_ready_tasks(milestone_id)
    6. If all tasks complete → notify orchestrator to enter review phase
```

**Orchestrator Tool (upgraded `agent.manage_plan`)**:

The orchestrator drives the entire flow via tool calls, not hardcoded logic:

```
Tool: agent.manage_plan (upgraded)

New operations:
- create_milestone(title, description) → milestone_id
- add_task(milestone_id, title, description, agent, depends_on[], parent_id)
- dispatch_milestone(milestone_id) → begin execution
- get_milestone_status(milestone_id) → progress report
- start_review(milestone_id) → trigger Review Agent
- get_review_findings(milestone_id) → get review results
- apply_fix_tasks(milestone_id) → convert findings to fix tasks and re-dispatch
```

---

### 4.6 Super Panel UI

**Role**: Upgrades existing `AIPlanPanel` to display milestone-level task management and agent status.

**Layout Design**:

```
┌─────────────────────────────────────────────────────┐
│  Super Panel - Inventory System               [×]   │
├─────────────────────────────────────────────────────┤
│  Milestone: Inventory System       Status: Executing│
│  Progress: ████████░░░░░░░░ 3/5 tasks complete      │
│─────────────────────────────────────────────────────│
│  📋 Task Tree                                       │
│                                                     │
│  ✅ Item data model             [script_agent] Done │
│     └─ Produced: item_data.gd, item_database.gd     │
│  ✅ Create inventory UI scene   [scene_agent]  Done │
│     └─ Produced: inventory.tscn                     │
│  ⏳ Item drag-and-drop          [script_agent] Active│
│  ⏸️ Item rarity border shader   [shader_agent] Wait │
│     └─ Depends on: Item data model ✅                │
│  ⏸️ Inventory capacity logic    [script_agent] Wait │
│─────────────────────────────────────────────────────│
│  🤖 Agent Run Status                                │
│                                                     │
│  script_agent  ████████░░  Running (drag-and-drop)  │
│  scene_agent   ✅         Idle                      │
│  shader_agent  ⏸️         Waiting                   │
│─────────────────────────────────────────────────────│
│  🔍 Review (previous round)                         │
│  ⚠️  item_data.gd:23 - Unused variable 'temp'      │
│  ℹ️  inventory.gd:45 - Consider adding type hint    │
│─────────────────────────────────────────────────────│
│  [📋 View Full TaskGraph] [🔄 Re-dispatch] [▶️ Review]│
└─────────────────────────────────────────────────────┘
```

**Implementation**:

```cpp
// editor/ai_component/ui/ai_milestone_panel.h

class AIMilestonePanel : public Control {
    GDCLASS(AIMilestonePanel, Control);
    
    Ref<AITaskGraph> task_graph;
    Ref<AISharedContext> shared_context;
    
    // UI components
    Tree *task_tree;              // Task tree (replaces flat list)
    Label *milestone_title;
    ProgressBar *progress_bar;
    Container *agent_status_area; // Agent status bars
    Container *review_findings;   // Review findings list
    
    void _refresh_task_tree();
    void _refresh_agent_status();
    void _refresh_review_findings();
    void _on_task_selected();
    
public:
    void set_task_graph(const Ref<AITaskGraph> &p_graph);
    void set_shared_context(const Ref<AISharedContext> &p_ctx);
    void refresh(); // Polling refresh (1s interval)
};
```

**Refresh Mechanism**: Uses `SceneTreeTimer` to poll `task_graph->get_milestone_progress()` and SubAgentService run status every second, incrementally updating the UI.

---

## 5. Complete Workflow

### 5.1 Milestone Lifecycle

```
┌──────────────────────────────────────────────────────────────┐
│                      MILESTONE LIFECYCLE                      │
├──────────────────────────────────────────────────────────────┤
│                                                               │
│  User input                                                   │
│  "Implement an inventory system with UI, drag-and-drop,      │
│   and rarity display"                                         │
│     │                                                         │
│     ▼                                                         │
│  ┌──────────┐                                                │
│  │ PLANNING │  Orchestrator → activates planning_agent        │
│  │          │  planning_agent reads project context           │
│  │          │  → produces TaskGraph (writes to shared ctx)   │
│  │          │  → returns summary to orchestrator              │
│  └────┬─────┘                                                │
│       │                                                       │
│       ▼                                                       │
│  ┌──────────────┐                                            │
│  │ USER CONFIRM │  Orchestrator presents TaskGraph           │
│  │ (human gate) │  → waits for user confirmation             │
│  └──────┬───────┘                                            │
│         │                                                     │
│         ▼                                                     │
│  ┌───────────┐                                               │
│  │ EXECUTING │  Orchestrator: dispatch_milestone()           │
│  │           │  SubAgentService schedules topologically:     │
│  │           │  Round 1: parallel [scene_agent, script_agent]│
│  │           │  Round 2: script_agent (depends on round 1)  │
│  │           │  Round 3: shader_agent (depends on round 2)  │
│  │           │  On each completion: shared_context updated   │
│  └─────┬─────┘                                              │
│        │                                                      │
│        ▼ (all tasks complete)                                 │
│  ┌───────────┐                                               │
│  │ REVIEWING │  Orchestrator: start_review()                 │
│  │           │  → activates review_agent                     │
│  │           │  → review_agent reads shared_context          │
│  │           │  → produces fix_tasks (writes to TaskGraph)   │
│  └─────┬─────┘                                              │
│        │                                                      │
│        ▼                                                      │
│  ┌──────────┐                                                │
│  │ FIXING   │  Fix items found?                              │
│  │ (loop)   │  → Orchestrator: apply_fix_tasks()             │
│  │          │  → Re-dispatch fix tasks                       │
│  │          │  → After completion, start_review() again      │
│  │          │  → Max 3 iterations, then request user help    │
│  └─────┬────┘                                                │
│        │ (no fixes OR max iterations reached)                 │
│        ▼                                                      │
│  ┌───────────┐                                               │
│  │ COMPLETED │  Orchestrator reports results                 │
│  │           │  User confirms → milestone archived           │
│  │           │  shared_context moved to completed_milestones │
│  └───────────┘                                               │
│                                                               │
└──────────────────────────────────────────────────────────────┘
```

### 5.2 Orchestrator System Prompt (Critical)

The orchestrator's system prompt must clearly define its **orchestration-only role**:

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

## 6. Data Flow and Interfaces

### 6.1 Inter-Agent Communication

```
                    ┌──────────────────┐
                    │   SharedContext   │ (Singleton, all agents read/write)
                    │  ───────────────  │
                    │  completed_tasks  │
                    │  produced_files   │
                    │  key_decisions    │
                    └───┬──────┬───────┘
            ▲ write     │      │  read
            │           │      │
    ┌───────┴──┐  ┌────┴──────┴───┐  ┌──────────┐
    │ Specialist│  │ Orchestrator  │  │ Review   │
    │ Agent     │  │ Agent         │  │ Agent    │
    └──────────┘  └───────┬───────┘  └──────────┘
                          │
                   ┌──────┴───────┐
                   │  TaskGraph    │ (managed by orchestrator; all agents see summary via context)
                   │  ───────────  │
                   │  milestones   │
                   │  task_nodes   │
                   │  dependencies │
                   └──────────────┘
```

### 6.2 Integration with Existing Session

```cpp
// AIAgentSession extension (minimal changes)

class AIAgentSession : public Node {
    // ... existing members ...
    
    // New
    Ref<AITaskGraph> task_graph;
    Ref<AISharedContext> shared_context;
    
    // Called during configure_provider() or initialization
    void _init_orchestration_layer();
    
    // Milestone mode detection
    bool is_milestone_mode() const;  // true when task_graph has an active milestone
    
    // Enhanced sub-agent completion callback
    void _on_sub_agent_completed(const String &p_run_id) {
        // Original logic...
        // New: update task_graph + shared_context
        if (task_graph.is_valid()) {
            sub_agent_service->on_sub_agent_completed(p_run_id);
            _emit_milestone_progress_update();
        }
    }
};
```

### 6.3 Tool Registration

Planning and Review agents use the same tool registration mechanism as existing specialist agents:

```cpp
// AISubAgentService::build_tool_registry() extension

Ref<AIToolRegistry> AISubAgentService::build_tool_registry(const String &p_agent_id) const {
    Ref<AIToolRegistry> registry;
    registry.instantiate();
    
    if (p_agent_id == "planning_agent" || p_agent_id == "review_agent") {
        // Shared read tools
        HashSet<String> shared_tools = AISubAgentRegistry::get_shared_read_tools();
        for (auto &tool : shared_tools) {
            register_builtin_tool_by_name(registry, tool);
        }
        // Planning/review-specific tool
        register_tool<AIManagePlanTool>(registry);
        
        // Shared context read tool
        register_tool<AISharedContextReadTool>(registry);
    }
    
    // ... existing specialist agent logic unchanged ...
    
    return registry;
}
```

---

## 7. Implementation Roadmap

### Phase 1: Data Layer (1-2 weeks)

| Step | Content | Files |
|------|---------|-------|
| 1.1 | Implement `AITaskGraph` data structure | `planning/ai_task_graph.h/.cpp` new |
| 1.2 | Implement `AISharedContext` service | `context/ai_shared_context.h/.cpp` new |
| 1.3 | Implement `AISharedContextProvider` | `context/ai_shared_context.h` new |
| 1.4 | Unit tests: TaskGraph topological sort, dependency detection | `tests/editor/test_ai_task_graph.cpp` new |
| 1.5 | Unit tests: SharedContext read/write, serialization | `tests/editor/test_ai_shared_context.cpp` new |

### Phase 2: Agent Registration (1 week)

| Step | Content | Files |
|------|---------|-------|
| 2.1 | Register Planning Agent definition | `ai_sub_agent_registry.cpp` modify |
| 2.2 | Register Review Agent definition | `ai_sub_agent_registry.cpp` modify |
| 2.3 | Upgrade `agent.manage_plan` tool for hierarchical tasks | `planning/ai_manage_plan_tool.cpp` modify |
| 2.4 | Integrate SharedContextProvider into context chain | `ai_sub_agent_service.cpp` modify |
| 2.5 | Tests: Planning Agent task decomposition workflow | Extend existing `test_ai_sub_agents.cpp` |

### Phase 3: Scheduling Engine (1-2 weeks)

| Step | Content | Files |
|------|---------|-------|
| 3.1 | `dispatch_ready_tasks()` dependency-aware scheduling | `ai_sub_agent_service.cpp` modify |
| 3.2 | Sub-agent completion → auto-trigger next | `ai_sub_agent_service.cpp` modify |
| 3.3 | Review trigger → fix task generation → re-dispatch | `ai_sub_agent_service.cpp` modify |
| 3.4 | Orchestrator prompt update | `prompts/agent_system_prompt.h` modify |
| 3.5 | Integration test: full milestone workflow | New test |

### Phase 4: UI (1-2 weeks)

| Step | Content | Files |
|------|---------|-------|
| 4.1 | `AIMilestonePanel` task tree + agent status | `ui/ai_milestone_panel.h/.cpp` new |
| 4.2 | Review findings display | Same as above |
| 4.3 | Polling refresh mechanism | Same as above |
| 4.4 | Replace/upgrade existing PlanPanel | `ui/ai_agent_dock.cpp` modify |
| 4.5 | UI tests | New tests |

### Phase 5: Polish (1 week)

| Step | Content |
|------|---------|
| 5.1 | Crash recovery: restore interrupted milestones on restart |
| 5.2 | Fix iteration cap (max 3 rounds, then request user intervention) |
| 5.3 | Milestone archiving and history browsing |
| 5.4 | Performance optimization: large TaskGraph UI rendering |

---

**Total Estimate**: 6-8 weeks, fully backward compatible with existing single-turn chat mode.

---

*This document is pending user review before implementation begins.*
