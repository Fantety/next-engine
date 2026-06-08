# NEXT Engine AI Multi-Agent 架构审查报告

> **审查日期**: 2026-05-29  
> **审查范围**: `editor/ai_component/` 全部模块  
> **审查维度**: 大型游戏项目能力、长程任务能力、架构扩展性、鲁棒性、易维护性

---

## 目录

1. [架构概览](#1-架构概览)
2. [模块分层分析](#2-模块分层分析)
3. [逐维度评估](#3-逐维度评估)
4. [问题清单（按严重程度）](#4-问题清单按严重程度)
5. [改进建议（按优先级）](#5-改进建议按优先级)
6. [路线图建议](#6-路线图建议)

---

## 1. 架构概览

### 1.1 系统定位

NEXT Engine 的 AI 模块是一个嵌入 Godot 编辑器内部的多 Agent 对话系统，通过 OpenAI-compatible function calling 协议将 LLM 能力与编辑器工具连接。系统支持主 Agent 编排子 Agent（Shader Agent、Scene Agent、Script Agent）完成专项任务，通过 MCP 协议接入外部工具服务器，并提供 Skills/Rules/Planning 等扩展机制。

### 1.2 模块组成

```
editor/ai_component/
├── agent/          # 核心运行时: Runtime, Runner, Session, SubAgentService
├── tools/          # 工具框架: Tool, Registry, Permission, ExecutionContext
│   ├── editor/     # 编辑器工具: 场景/脚本/Shader 编辑
│   └── project/    # 项目工具: 文件搜索/读取/列表
├── providers/      # AI 供应商: OpenAI Codec, MCP Protocol/Client
├── context/        # 上下文提供者: Editor/File/ProjectTree/BestPractices/SubAgent
├── planning/       # 计划管理: PlanManager + manage_plan tool
├── storage/        # 持久化: ConversationStore, SubAgentRunStore
├── review/         # 变更审查: DiffService, ChangeSetStore
├── skills/         # Skill 系统: 激活/配置/上下文注入
├── rules/          # 规则系统: RuleSettings, RulesContextProvider
├── prompts/        # 系统提示词
└── ui/             # 编辑器 UI: Dock, Composer, MessageList, Panels
```

### 1.3 架构分层

```
┌─────────────────────────────────────────────┐
│                  UI Layer                    │
│  Dock / Composer / MessageList / Panels     │
├─────────────────────────────────────────────┤
│               Session Layer                  │
│     AIAgentSession (状态机 + 消息总线)        │
├─────────────────────────────────────────────┤
│               Runtime Layer                  │
│  AIAgentRuntime + AIAgentRuntimeRunner       │
│  (多轮 LLM 交互 + 工具调用循环)              │
├─────────────────┬───────────────────────────┤
│  Provider Layer │  Sub-Agent Layer           │
│  OpenAI Codec   │  AISubAgentService         │
│  MCP Protocol   │  (并行 Agent 编排)         │
├─────────────────┴───────────────────────────┤
│               Tool Layer                     │
│  Tool → Registry → Permission → Execution   │
├─────────────────────────────────────────────┤
│              Context Layer                   │
│  ContextManager + ContextProviders           │
├─────────────────────────────────────────────┤
│              Storage Layer                   │
│  Conversation / SubAgentRun / ChangeSet      │
└─────────────────────────────────────────────┘
```

---

## 2. 模块分层分析

### 2.1 Agent Runtime（核心循环）

**文件**: `ai_agent_runtime.h/.cpp`, `ai_agent_runtime_runner.h/.cpp`

**设计要点**:
- `AIAgentRuntime::run()` 实现了标准的多轮 `provider→tool→provider` 循环
- 通过 `max_provider_turns`（默认255）和 `max_tool_calls`（默认60）控制循环上限
- 流式响应通过 `complete_streaming()` + `_on_provider_partial_response` 回调实现
- `AIAgentRuntimeRunner` 将 Runtime 执行放到独立线程，通过 `SafeFlag` 控制生命周期
- 通过 `call_deferred` 将进度回调安全地发送回主线程

**评价**: ✅ 核心循环设计合理，流式处理正确。⚠️ 见下方问题。

### 2.2 Sub-Agent Service（多 Agent 编排）

**文件**: `ai_sub_agent_service.h/.cpp`, `ai_sub_agent_types.h/.cpp`, `ai_sub_agent_registry.h/.cpp`

**设计要点**:
- `AISubAgentService` 管理子 Agent 的完整生命周期：activate → run → complete/fail/interrupt → resume → dismiss
- 子 Agent 定义（`AISubAgentDefinition`）包含：agent_id、owned_tools、max_provider_turns 等
- 每个子 Agent 有自己的 `Runtime + Runner + ToolRegistry`，在独立线程中执行
- 通过 `AISubAgentCancellationToken` 实现工具级取消
- 子 Agent 的 tool registry 通过 `AICancellableSubAgentTool` wrapper 拦截取消
- Run 状态通过 JSON 文件持久化（`AISubAgentRunStore`）

**Agent 拓扑（当前）**:
```
Main Agent (orchestrator)
  ├── agent.activate_sub_agent("shader_agent")  →  Shader Agent
  ├── agent.activate_sub_agent("scene_agent")   →  Scene Agent
  ├── agent.activate_sub_agent("script_agent")  →  Script Agent
  ├── agent.get_sub_agent_status()              →  查询状态
  ├── agent.resume_sub_agent()                  →  恢复中断
  └── agent.dismiss_sub_agent_run()             →  关闭运行
```

**Agent 权限模型**:
| Agent | 可读工具 | 写入工具 |
|-------|---------|---------|
| Main Agent | project.*, editor.get_context, scene.describe_tree, scene.inspect_node, scene.list_properties, script.inspect | agent.manage_plan, agent.activate_skill + 编排工具 |
| Shader Agent | 共享只读工具 | shader.apply_to_node |
| Scene Agent | 共享只读工具 | scene.apply_patch |
| Script Agent | 共享只读工具 | script.create/write/patch_function/bind/unbind/delete |

**评价**: ✅ 关注点分离清晰，权限模型合理。⚠️ 见下方问题。

### 2.3 Tool System（工具框架）

**文件**: `tools/ai_tool.h`, `ai_tool_registry.h`, `ai_tool_permission.h/.cpp`, `ai_tool_execution_context.h`

**设计要点**:
- `AITool` 抽象基类：`get_name()` / `get_description()` / `get_parameters_schema()` / `execute()`
- `AIToolRegistry` 简单 HashMap 注册表，支持动态注册/查询
- `AIToolPermissionPolicy` 三级权限：ALLOW → ASK → DENY
- `AIToolExecutionContext` 使用 thread_local 存储当前执行上下文
- `AIMCPTool` 将 MCP 工具包装为 AITool 接口

**评价**: ✅ 接口简洁，扩展性好。⚠️ 权限模型缺少参数级过滤能力。

### 2.4 Context Management（上下文管理）

**文件**: `agent/ai_context_manager.h`, `context/ai_context_provider.h`

**设计要点**:
- `AIContextManager::build_messages()` 负责组装发给 LLM 的消息列表
- 通过字符级别的预算控制（`max_input_chars`, `max_context_chars`, `max_history_chars`, `max_tool_result_chars`）防止上下文溢出
- `min_recent_messages` 保证最近消息不被截断
- ContextProvider 接口简单（`collect_context()` 返回 Array）
- 支持多种上下文来源：EditorContext、FileContext、ProjectTreeContext、BestPractices、Skills、Rules、SubAgent

**评价**: ✅ 预算控制基本到位。⚠️ 字符级估算不够精确。

### 2.5 Storage & Serialization（持久化）

**设计要点**:
- `AIConversationStore`: JSON 文件存储会话（`user://ai_agent/conversations/`）
- `AISubAgentRunStore`: JSON 文件存储子 Agent 运行状态
- `AIChangeSetStore`: 内存 + Mutex 保护，变更集追踪
- 序列化方向：`AIAgentMessage::to_dict()` / `from_dict()`
- `AISubAgentRun::to_dict()` / `from_dict()` 包含完整消息历史

**评价**: ✅ 基础持久化完善。⚠️ 缺乏事务性、无 WAL/Journal。

### 2.6 UI Layer（用户界面）

**文件**: `ui/` 目录下 28 个 .h/.cpp 文件

**设计要点**:
- `AIAgentDock`: 主入口，管理会话切换、消息展示、工具审批
- `AIComposer`: 输入框 + 发送逻辑
- `AIMessageList`: 消息列表 + Markdown 渲染
- `AIChangeReviewPanel`: Before/After diff 审查
- `AIPlanPanel`: 任务计划可视化
- 设置页面：Models / MCP / Skills / Rules

**评价**: ✅ UI 功能完整。⚠️ UI 与业务逻辑耦合度偏高。

---

## 3. 逐维度评估

### 3.1 大型游戏项目能力 ⭐⭐⭐ (3/5)

| 维度 | 评估 | 说明 |
|------|------|------|
| 项目规模适应性 | ⚠️ 中等 | Context 预算基于字符估算，大型项目上下文可能不足 |
| 多场景支持 | ❌ 不足 | Agent 对多场景/多资源协同无感知 |
| 资产管线集成 | ❌ 缺失 | 无纹理/模型/音频资源工具 |
| 构建系统集成 | ❌ 缺失 | 无编译/打包/部署相关工具 |
| 性能分析 | ❌ 缺失 | 无 Profiler 上下文 |

**核心问题**: 当前工具集中于场景节点操作和脚本编辑，对于大型游戏项目常见的资产管线、构建配置、性能分析等环节缺乏覆盖。Agent 对项目结构的理解仅限于文件树，不感知资源依赖关系。

### 3.2 长程任务能力 ⭐⭐ (2/5)

| 维度 | 评估 | 说明 |
|------|------|------|
| 任务分解 | ✅ 良好 | PlanManager 支持 pending/in_progress/completed |
| 任务持久化 | ✅ 良好 | SubAgentRun 完整序列化到 JSON |
| 中断恢复 | ⚠️ 部分 | 支持手动 resume，但不支持崩溃自动恢复 |
| 超时处理 | ❌ 不足 | 无任务级超时，仅 provider 级别 timeout |
| 进度报告 | ⚠️ 部分 | 仅 polling 轮询，无推送式进度 |
| 错误恢复 | ❌ 不足 | 失败后无自动重试，无回滚机制 |
| 资源管理 | ⚠️ 中等 | 线程资源有清理，但无内存/Token 预算监控 |

**核心问题**:
1. **无事务性保证**: 子 Agent 执行中编辑器崩溃，重启后状态可能不一致（最后一次 save_run 前的状态丢失）
2. **无 WAL/Journal**: JSON 文件直接覆写，写入中断导致数据损坏
3. **无自动恢复**: 需要手动 `resume_sub_agent`，不能自动检测并恢复中断的运行
4. **无任务优先级**: 所有子 Agent 平等调度，无优先级/依赖关系

### 3.3 架构扩展性 ⭐⭐⭐ (3/5)

| 维度 | 评估 | 说明 |
|------|------|------|
| 新增工具 | ✅ 良好 | 实现 AITool 接口 + 注册即可 |
| 新增子 Agent | ⚠️ 部分 | 定义硬编码在 registry.cpp，需修改源码 |
| 新增 Provider | ✅ 良好 | AIAgentRuntimeClient 抽象基类 |
| 新增 Context Provider | ✅ 良好 | AIContextProvider 抽象基类 |
| MCP 集成 | ✅ 良好 | 支持 stdio/http/sse 三种 transport |
| 插件系统 | ❌ 缺失 | 无动态加载/热插拔能力 |
| 工具组合 | ❌ 缺失 | 无工具管道/链式调用 |

**核心问题**:
1. **子 Agent 注册硬编码**: `ai_sub_agent_registry.cpp` 中 `make_shader_agent()` 等函数直接构造定义，新增 Agent 需修改 C++ 源码并重新编译
2. **无声明式 Agent 配置**: 没有 JSON/YAML 配置文件定义 Agent
3. **工具权限粒度不足**: 仅工具名级别，不支持"允许读 /tmp 但不允许读 /etc"

### 3.4 鲁棒性 ⭐⭐ (2/5)

| 维度 | 评估 | 说明 |
|------|------|------|
| 错误传播 | ✅ 良好 | Runtime → Session → UI 链路清晰 |
| 异常安全 | ✅ 良好 | 无 try-catch（Godot 风格），ERR_FAIL 守卫 |
| 线程安全 | ⚠️ 部分 | SafeFlag + Mutex 分散使用，无统一策略 |
| 取消机制 | ⚠️ 部分 | 子 Agent 有取消，主 Agent 取消不完整 |
| 重试机制 | ❌ 缺失 | Provider 调用无重试/退避 |
| 降级策略 | ❌ 缺失 | 工具失败无 fallback |
| 输入校验 | ⚠️ 部分 | Tool execute 前有基本校验，但不统一 |
| 资源泄漏 | ⚠️ 中等 | Runner 析构时 wait_to_finish，但无强制超时 |

**核心问题**:
1. **Provider 调用无重试**: `ai_agent_runtime.cpp:304` `client->complete_streaming()` 返回 error 直接终止，不重试
2. **取消机制不完整**: Session 的 `cancel_request()` 存在但 Runtime 内部不支持中断正在执行的工具
3. **线程清理隐患**: `stale_runtime_by_token` 仅在 `_clear_stale_runtimes` 时清理，可能积累
4. **无健康检查**: Provider / MCP Server 无定期健康检查

### 3.5 易维护性 ⭐⭐⭐⭐ (4/5)

| 维度 | 评估 | 说明 |
|------|------|------|
| 代码组织 | ✅ 良好 | 按功能分层，文件命名一致 |
| 接口设计 | ✅ 良好 | 抽象基类清晰，依赖注入 |
| 测试覆盖 | ✅ 良好 | 4 个测试文件，Mock 客户端/工具 |
| 日志/调试 | ✅ 良好 | print_line 覆盖关键路径 |
| 文档 | ⚠️ 部分 | README 完善，代码注释较少 |
| 配置管理 | ✅ 良好 | ProviderConfig / ModelSettings 集中管理 |
| 向后兼容 | ✅ 良好 | Store 有 normalize 逻辑 |

**核心问题**:
1. **测试侵入性**: 大量 `for_test` 方法和 `set_*_for_test` API，暴露内部实现
2. **日志级别缺失**: 全部使用 `print_line`，无分级（info/warn/error）
3. **魔法数字**: `255` (max_provider_turns)、`60` (max_tool_calls)、`240` (clip_status_text) 散布在代码中

---

## 4. 问题清单（按严重程度）

### 🔴 严重（影响生产可用性）

| ID | 问题 | 位置 | 影响 |
|----|------|------|------|
| P1 | **无 Provider 重试机制** | `ai_agent_runtime.cpp:304` | LLM API 临时故障导致整个 Agent 运行失败 |
| P2 | **无事务性持久化** | `ai_sub_agent_run_store.cpp` | 崩溃时 JSON 文件可能损坏，丢失子 Agent 运行状态 |
| P3 | **子 Agent 无自动恢复** | `ai_sub_agent_service.cpp` | 编辑器重启后，中断的运行需要手动检查并 resume |
| P4 | **主 Agent 取消不完整** | `ai_agent_runtime.cpp:290-499` | `cancel_request()` 无法中断正在执行的工具调用 |

### 🟡 中等（影响复杂场景可用性）

| ID | 问题 | 位置 | 影响 |
|----|------|------|------|
| P5 | **字符级 Token 估算不精确** | `ai_agent_runtime.cpp:17-22` | 可能导致上下文溢出或浪费 |
| P6 | **子 Agent 注册硬编码** | `ai_sub_agent_registry.cpp` | 新增 Agent 需修改 C++ 源码并重新编译 |
| P7 | **无 Agent 并发限制** | `AISubAgentService::active_run_by_agent` | 同类型 Agent 只能运行一个实例 |
| P8 | **无工具参数级权限** | `ai_tool_permission.cpp` | 无法限制"只读特定目录" |
| P9 | **无任务优先级/依赖** | `AISubAgentService` | 子 Agent 完全平等，无法编排工作流 |
| P10 | **缺少资源/资产工具** | `tools/` 目录 | 无纹理、模型、音频等资产操作工具 |

### 🟢 轻微（影响开发体验和可维护性）

| ID | 问题 | 位置 | 影响 |
|----|------|------|------|
| P11 | **测试 API 侵入性强** | 多处 `*_for_test` 方法 | 测试与实现耦合，重构困难 |
| P12 | **日志无分级** | 全部 `print_line` | 生产调试困难 |
| P13 | **魔法数字散布** | 多处硬编码值 | 调整参数需搜索多个位置 |
| P14 | **无声明式 Agent 配置** | `ai_sub_agent_registry.cpp` | 用户无法自定义 Agent |
| P15 | **UI 与业务逻辑耦合** | `ui/` 目录 | UI 重构影响面大 |

---

## 5. 改进建议（按优先级）

### 5.1 立即修复（P0 - 1-2 周）

#### P1/P4: Provider 重试 + 完整取消

```cpp
// 建议在 AIAgentRuntime 中增加重试逻辑
struct ProviderRetryConfig {
    int max_retries = 3;
    int base_delay_ms = 1000;
    int max_delay_ms = 30000;
    float backoff_multiplier = 2.0f;
};

// 建议的工具执行支持取消检查
// 在 execute() 循环中定期检查 AIToolExecutionContext::is_cancelled()
```

#### P2: 事务性持久化

```cpp
// 建议: 写临时文件 → fsync → 原子 rename
Error AISubAgentRunStore::save_run_atomic(const AISubAgentRun &p_run) {
    String tmp_path = run_path + ".tmp";
    // 1. 写入临时文件
    // 2. fsync 确保落盘
    // 3. 原子 rename 替换原文件
}
```

#### P3: 自动恢复

```cpp
// 建议: AISubAgentService 启动时自动检测并恢复中断的运行
void AISubAgentService::recover_interrupted_runs() {
    Vector<AISubAgentRun> runs = store->load_runs();
    for (auto &run : runs) {
        if (run.status == AI_SUB_AGENT_RUN_RUNNING) {
            // 标记为 INTERRUPTED，等待用户决定 resume
            run.status = AI_SUB_AGENT_RUN_INTERRUPTED;
            run.resumable = true;
            store->save_run(run);
        }
    }
}
```

### 5.2 短期改进（P1 - 1-2 月）

#### P5: Token 级上下文预算

```cpp
// 建议: 集成 tiktoken 或使用更精确的估算
// 当前: chars / 4 → 建议: 语言相关的 token 估算
// 或者让 Provider 返回实际 token 数用于下一次预算计算
```

#### P6/P14: 声明式 Agent 配置

```json
// 建议: 支持从 JSON 加载 Agent 定义
// user://ai_agent/agents/shader_agent.json
{
    "agent_id": "shader_agent",
    "display_name": "Shader Agent",
    "instruction": "Handle shader-focused implementation work...",
    "owned_tools": ["shader.apply_to_node"],
    "model_profile_id": "write",
    "max_provider_turns": 50
}
```

#### P7: Agent 实例池

```cpp
// 建议: 支持同一类型 Agent 多实例
// 通过 run_id 区分，而非 agent_id 限制
// 增加 max_concurrent_instances 配置
```

#### P8: 参数级权限

```cpp
// 建议: 扩展 AIToolPermissionPolicy 支持参数过滤
struct ToolPermissionRule {
    String tool_name;
    String argument_path;    // e.g., "path" for file operations
    String allowed_pattern;  // e.g., "res://scripts/*.gd"
    AIToolPermissionDecision decision;
};
```

### 5.3 中期改进（P2 - 3-6 月）

#### Agent 工作流引擎

```
建议引入 DAG 工作流:
Main Agent 定义任务图 → SubAgentService 解析依赖 → 并行/串行调度

Example:
  [Script Agent] ──→ [Scene Agent] ──→ [Shader Agent]
       │                                      │
       └──→ [Build Verification Agent] ←──────┘
```

#### 资产管线工具

新增工具类别:
- `asset.import_texture` - 导入纹理并配置压缩
- `asset.create_material` - 创建 StandardMaterial3D
- `audio.import_sample` - 导入音频并配置流式
- `build.compile_script` - 编译 GDScript 检查错误
- `project.analyze_dependencies` - 分析资源依赖

#### 长程任务增强

- **Checkpoint 系统**: 每隔 N 步自动保存快照，支持回滚到任意检查点
- **进度回调**: 子 Agent 定期报告进度（百分比 + 描述）
- **资源预算**: 限制子 Agent 的 Token/时间/文件修改数量

### 5.4 长期愿景（P3 - 6-12 月）

#### 插件化 Agent 生态

```
Agent Plugin 规范:
├── agent.json          # 元数据
├── system_prompt.md    # 系统提示词
├── tools/              # 自定义工具（脚本/二进制）
├── skills/             # 关联 Skills
└── context_providers/  # 自定义上下文
```

#### 多模态上下文

- 场景截图作为视觉上下文提供给多模态模型
- 材质预览/Shader 效果图
- UI 布局截图对比

#### 协作开发支持

- 多开发者 Agent 会话隔离
- Agent 变更审查 + Code Review 流程
- 团队共享的 Rules/Skills/Prompts 库

---

## 6. 路线图建议

```
Phase 1 (1-2 周): 生产稳定性
├── Provider 重试机制
├── 事务性文件写入
├── 崩溃自动恢复
└── 完整取消支持

Phase 2 (1-2 月): 能力增强
├── Token 级上下文预算
├── 声明式 Agent 配置
├── 参数级工具权限
└── Agent 实例池

Phase 3 (3-6 月): 游戏开发深度集成
├── 资产管线工具
├── Agent 工作流引擎
├── Checkpoint/回滚系统
└── 资源依赖分析上下文

Phase 4 (6-12 月): 生态建设
├── Agent Plugin 系统
├── 多模态上下文
├── 协作开发支持
└── 团队配置共享
```

---

## 附录

### A. 文件统计

| 模块 | .h 文件 | .cpp 文件 | 估算代码行数 |
|------|---------|----------|-------------|
| agent/ | 12 | 11 | ~3500 |
| tools/ (含子目录) | 25 | 25 | ~5000 |
| providers/ | 10 | 9 | ~2500 |
| context/ | 7 | 6 | ~800 |
| planning/ | 2 | 2 | ~400 |
| storage/ | 3 | 3 | ~600 |
| review/ | 2 | 2 | ~400 |
| skills/ | 3 | 3 | ~300 |
| rules/ | 2 | 2 | ~200 |
| prompts/ | 1 | 0 | ~300 |
| ui/ | 28 | 26 | ~6000 |
| **总计** | **95** | **89** | **~20,000** |

### B. 测试覆盖

| 测试文件 | 行数 | 覆盖范围 |
|----------|------|----------|
| test_ai_sub_agents.cpp | 2079 | SubAgent 生命周期、持久化、工具路由 |
| test_ai_agent_runtime.cpp | 1923 | Runtime 循环、流式、工具调用、上下文管理 |
| test_ai_agent_tools.cpp | - | 工具接口、权限、执行 |
| test_ai_model_settings.cpp | - | 模型配置 |

### C. 关键类依赖图

```
AIAgentSession (Node)
  ├── AIAgentRuntime (RefCounted)
  │     ├── AIAgentRuntimeClient (抽象)
  │     │     └── AIOpenAICompatibleRuntimeClient
  │     ├── AIToolRegistry
  │     │     └── AITool[] (N 个工具实现)
  │     ├── AIContextManager
  │     └── AIAgentProfile
  ├── AIAgentRuntimeRunner (RefCounted, Thread)
  ├── AISubAgentService (RefCounted)
  │     ├── AISubAgentRunStore
  │     ├── AIAgentRuntime (per sub-agent)
  │     ├── AIAgentRuntimeRunner (per sub-agent)
  │     └── AISubAgentCancellationToken
  ├── AIConversationStore
  ├── ContextProviders[] (RefCounted)
  └── PlanManager (Singleton)
```

### D. 并发模型

```
Main Thread (UI):
  AIAgentSession → 消息管理、UI 更新

Worker Thread (per run):
  AIAgentRuntimeRunner → AIAgentRuntime::run()
    ├── complete_streaming() → HTTP 请求（同步阻塞）
    └── tool.execute() → 编辑器操作（需线程安全）
  
  通过 call_deferred 将进度发送回主线程
  通过 SafeFlag 控制运行状态
```

---

*本报告由 Sisyphus 架构审查生成。建议在每次重大架构变更后重新审查。*
