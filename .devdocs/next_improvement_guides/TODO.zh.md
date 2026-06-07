# NextEngine AI 改进 TODO 清单

> 本清单根据 `.devdocs/next_improvement_guides/` 下的专题文档整理，用于后续排期、拆任务和验收。  
> 推荐先做 NEXT 闭环，再做项目记忆、资产面板和诊断能力；架构项穿插在相关功能开发中完成。

## 总体目标

- [ ] 让 NEXT 模式形成完整闭环：计划 -> 执行 -> 时间线 -> 验收 -> 反馈修复 -> 安全回滚 -> 锁定里程碑。
- [ ] 让 AI 行为更可控：项目记忆、设计约束、工具分组、权限、审计逐步落地。
- [ ] 让 AI 输出更可用：MarkdownViewer 承载报告、资源链接、代码块对比/应用。

## 阶段 1：NEXT 执行可观察性

来源文档：[NEXT Replayable Execution Timeline](next-replayable-execution-timeline.md)

- [ ] 设计 NEXT 时间线的统一行结构，至少包含时间、事件类型、Agent、里程碑、任务、摘要、metadata。
- [ ] 为 `AINextEventLog` 增加查询/格式化接口，例如按 milestone、task、agent 过滤。
- [ ] 将关键 runtime tool message 归一化为 timeline row，避免只显示原始消息。
- [ ] 在 NEXT Activity 区域显示更完整的时间线，而不是只显示最近几条混合消息。
- [ ] 支持打开历史 workflow 时查看已持久化的执行时间线。
- [ ] 为失败事件增加明确展示：失败 Agent、失败任务、错误原因、是否可恢复。
- [ ] 增加测试：事件持久化、过滤、历史 workflow 加载后时间线仍可重建。

验收标准：

- [ ] 用户能看懂每个 Agent 做了什么、调用了什么工具、在哪一步失败。
- [ ] 编辑器重启后，workflow 的时间线仍能显示。
- [ ] 时间线不会被 streaming 片段刷屏，只保留对用户有意义的状态变化和工具结果。

## 阶段 2：NEXT 里程碑验收系统

来源文档：[NEXT Milestone Acceptance System](next-milestone-acceptance-system.md)

- [ ] 增加里程碑验收摘要构建器，优先从结构化状态本地生成，不依赖模型生成。
- [ ] 摘要包含：里程碑状态、完成任务、失败/阻塞任务、结果摘要、产物路径、Review Findings、可回滚变更数量、不可自动回滚警告。
- [ ] 在 NEXT 面板中新增“里程碑验收”折叠区，使用 `MarkdownViewer` 渲染摘要。
- [ ] 将现有 `Review`、`Playtest Feedback`、`Accept & Lock` 与验收区形成清晰流程。
- [ ] 明确验收状态语义：已完成但未锁定 = 等待验收；锁定 = 已接受基线。
- [ ] 保留当前 Playtest Feedback 的 `append_tasks` 路径作为兜底。
- [ ] 新增反馈归因机制：选中任务或明确任务时，优先调用 `send_task_session_message()` 追加修复对话。
- [ ] 当反馈无法归因、属于新增需求或跨多个任务时，继续调用 `generate_feedback_tasks()` 让 Planning Agent 追加任务。
- [ ] 增加测试：验收摘要内容、锁定按钮状态、任务级反馈续修、无法归因反馈新增任务。

验收标准：

- [ ] 用户在一个地方看到“本里程碑交付了什么、还有什么风险、下一步能做什么”。
- [ ] 小修复不会默认污染计划树。
- [ ] 新需求仍能通过 Planning Agent 拆成新任务。

## 阶段 3：NEXT 变更包与安全回滚

来源文档：[NEXT Change Sandbox and Rollback](next-change-sandbox-rollback.md)

- [ ] 扩展 `AIToolExecutionContext` 或 change set metadata，记录 `workflow_id`、`milestone_id`、`task_id`、`agent_id`、`agent_run_id`。
- [ ] 确保 NEXT Script/Shader Agent 产生的 review change set 能按 milestone 聚合。
- [ ] 增加 session 查询接口：列出当前里程碑的安全变更包。
- [ ] 在验收摘要中显示可自动回滚和仅能人工检查的变更。
- [ ] 增加“查看变更包”入口，列出文件、change set 状态、来源任务。
- [ ] 增加“回滚本里程碑安全变更”动作，内部调用 `AIChangeSetStore::revert_change_set()`。
- [ ] 增加“保留本里程碑安全变更”动作，内部调用 `keep_change_set()`。
- [ ] 对 scene `.tscn`、二进制资源、导入资产等无安全 old/new 记录的变更只做警告，不自动回滚。
- [ ] 将回滚/保留结果写入 `AINextEventLog`。
- [ ] 增加测试：按 milestone 聚合、回滚成功、回滚失败、当前文件内容不匹配时拒绝覆盖。

验收标准：

- [ ] 用户清楚知道哪些文件能一键回滚，哪些只能人工检查。
- [ ] 回滚不会覆盖用户在 AI 修改后又手动改过的文件。
- [ ] 回滚事件能在时间线中追踪。

## 阶段 4：项目记忆与设计约束库

来源文档：[Project Memory and Design Constraints](project-memory-design-constraints.md)

- [ ] 设计项目记忆数据结构：游戏愿景、玩法规则、场景结构、资源命名、代码风格、禁改路径、Agent 备注。
- [ ] 新增项目级 memory storage，复用项目 scope。
- [ ] 新增 `AIProjectMemoryContextProvider`。
- [ ] 将项目记忆注入 Normal 模式上下文。
- [ ] 将项目记忆注入 NEXT planning/task/review 上下文。
- [ ] 增加 AI 设置页，用于编辑项目记忆和禁改路径。
- [ ] 明确规则：Agent 可以建议更新记忆，但不能静默修改。
- [ ] 在 Context Budget metadata 中标记项目记忆是否被注入/截断。
- [ ] 增加测试：保存/加载、上下文注入、禁用/空记忆、截断行为。

验收标准：

- [ ] 用户不用每次重复项目约定。
- [ ] Normal 和 NEXT 都能遵守同一份项目记忆。
- [ ] 项目记忆不会无限挤占任务上下文。

## 阶段 5：NEXT 资产注册与引用面板

来源文档：[NEXT Asset Registry and Reference Panel](next-asset-registry-reference-panel.md)

- [ ] 梳理 `AINextAssetRecord` 当前字段与缺口。
- [ ] 在任务完成时，将 `output_paths` 自动注册为生成资产。
- [ ] 记录资产来源：milestone、task、agent、parent asset、是否 protected。
- [ ] 增加只读资产面板，显示生成资产、用户资产、受保护资产。
- [ ] 锁定里程碑时，将本里程碑产物标记为 baseline。
- [ ] 支持资产筛选：全部、生成、用户、受保护。
- [ ] 支持基础操作：复制路径、后续可扩展打开资源、标记受保护。
- [ ] 暂不承诺完整引用分析；引用关系可先显示已知任务引用。
- [ ] 增加测试：任务产物自动注册、workflow 持久化、锁定后 baseline 标记。

验收标准：

- [ ] 用户能看到 NEXT 产生了哪些资源，以及它们来自哪个任务。
- [ ] 用户导入或受保护资产不会被 Agent 随意修改。
- [ ] 资产记录随 workflow 保存和恢复。

## 阶段 6：Context Budget 可视化

来源文档：[Context Budget Visualization](context-budget-visualization.md)

- [ ] 增加 `estimated_context_usage` 的统一格式化函数。
- [ ] 在 Normal 模式显示最近一次请求的上下文估算：输入字符、估算 token、历史省略数、tool result 截断数、context document 截断数。
- [ ] 在 NEXT 任务/Agent run 详情中显示对应上下文预算。
- [ ] 支持开发者复制上下文诊断摘要。
- [ ] 明确 UI 文案：这是估算，不是 provider 精确 token。
- [ ] 增加测试：metadata 格式化、缺字段兼容、UI 摘要不溢出。

验收标准：

- [ ] 用户/开发者能判断 AI 是否因为上下文截断而漏信息。
- [ ] 上下文诊断不会干扰普通聊天和 NEXT 主流程。

## 阶段 7：MarkdownViewer 作为 AI 报告视图

来源文档：[MarkdownViewer as AI Report View](markdownviewer-ai-report-view.md)

- [ ] 约定 AI 结构化报告优先使用 `MarkdownViewer` 渲染。
- [ ] 将 NEXT 里程碑验收摘要渲染为 Markdown 报告。
- [ ] 将变更包摘要渲染为 Markdown 报告。
- [ ] 将错误分析、Review Findings、Context Budget 诊断逐步统一为 Markdown 报告。
- [ ] 保持安全默认值：禁用远程图片，外链不自动打开。
- [ ] 增加测试：报告 markdown 生成、空数据展示、长文本布局。

验收标准：

- [ ] AI 报告统一、可读，不再散落为大量 ad hoc Label。
- [ ] 操作仍使用真实按钮，不通过 markdown 链接触发危险动作。

## 阶段 8：MarkdownViewer Godot 资源链接

来源文档：[MarkdownViewer Godot Resource Links](markdownviewer-godot-resource-links.md)

- [ ] 增加 `res://` 链接识别和安全校验。
- [ ] 支持显式 markdown 链接：`[player.gd](res://scripts/player.gd)`。
- [ ] 点击脚本资源时打开脚本编辑器。
- [ ] 点击场景/资源时打开对应编辑器或定位到文件系统。
- [ ] 缺失资源给出非阻塞提示。
- [ ] 外部链接继续默认禁用，避免 AI 报告触发不可信 URL。
- [ ] 增加测试：有效资源、缺失资源、外链禁用、主线程打开资源。

验收标准：

- [ ] 用户能从验收报告/任务报告直接跳到产物资源。
- [ ] 资源链接不会打开项目外路径或任意外部 URL。

## 阶段 9：MarkdownViewer 代码块增强

来源文档：[MarkdownViewer Code Block Actions](markdownviewer-code-block-actions.md)

- [ ] 保留当前已有代码块 Copy 能力。
- [ ] 为代码块补充可选 metadata：目标路径、语言、是否可操作。
- [ ] 增加 `Compare With File` 动作，打开现有 diff 视图对比代码块和目标文件。
- [ ] 增加 `Create File` 动作，但必须通过 review/change-set 流程。
- [ ] 增加 `Apply` 动作前的显式确认，不允许从 Markdown 自动写文件。
- [ ] 对普通报告默认只显示 Copy，避免 UI 过度拥挤。
- [ ] 增加测试：Copy 保持可用、Compare 不改文件、Apply 走 review/change-set。

验收标准：

- [ ] 用户可以把 AI 报告里的代码从“可读”推进到“可安全比较/应用”。
- [ ] 所有写文件行为都有显式确认和可审查路径。

## 阶段 10：架构巩固

来源文档：

- [Unify Normal and NEXT Session Base](unify-normal-next-session-base.md)
- [Unified Tool Registration Groups](unified-tool-registration-groups.md)
- [Unified AI Storage Base](unified-ai-storage-base.md)

- [ ] 审计 `AIAgentSession` 和 `AIAgentNextSession` 仍然重复的 helper，只移动确定通用的逻辑到 `AISessionBase`。
- [ ] 避免把 conversation、workflow、milestone 等业务模型塞进 `AISessionBase`。
- [ ] 基于现有 `AIToolFactory` 增加缺失分组：`next_project_tools`、`mcp_tools`、`destructive_tools`、`asset_tools`。
- [ ] 增加工具暴露测试：MainAgent、Planning Agent、Script Agent、Scene Agent、Shader Agent、Review Agent。
- [ ] 审计 storage 层重复的路径、sanitize、JSON、原子保存逻辑。
- [ ] 在不改变现有磁盘路径的前提下继续收敛到 `AIStorageBase`。
- [ ] 增加迁移风险说明：不做隐式用户数据迁移。

验收标准：

- [ ] 架构清理不改变用户可见行为。
- [ ] 新工具/新存储后续能用更少样板代码接入。
- [ ] 测试能防止 Normal/NEXT 工具集合漂移。

## 阶段 11：用户系统扩展

来源文档：

- [Cloud Sync for AI Configuration](cloud-sync-ai-configuration.md)
- [Team Collaboration Permissions](team-collaboration-permissions.md)
- [Agent Execution Audit](agent-execution-audit.md)

- [ ] 先实现本地 AI 设置 bundle 导出/导入，覆盖模型配置、MCP、Rules、Skills、NEXT 偏好。
- [ ] 明确敏感字段策略：API Key、MCP env、token 不默认上传。
- [ ] 后续接入用户系统云同步，支持 push/pull 和冲突提示。
- [ ] 设计 AI 权限点：`ai.ask`、`ai.plan`、`ai.write`、`ai.review`、`ai.next.run`、`ai.next.lock_milestone`、`ai.settings.mcp` 等。
- [ ] 增加 AI 权限服务，先接入一个高价值动作，例如 NEXT 里程碑锁定。
- [ ] 将用户身份写入人类触发的 `AINextEventLog` metadata。
- [ ] 审计关键行为：提交 brief、批准计划、运行里程碑、反馈、回滚、保留变更、锁定里程碑。
- [ ] 增加测试：未登录/匿名用户、权限拒绝、审计 metadata、敏感字段不入云。

验收标准：

- [ ] 单机离线使用不受影响。
- [ ] 团队场景中用户权限和 AI 操作边界清晰。
- [ ] 能回答“谁触发了 AI、AI 改了什么、谁批准/回滚了变更”。

## 建议优先级

1. [ ] NEXT 时间线
2. [ ] 里程碑验收摘要
3. [ ] 变更包聚合与安全回滚
4. [ ] Playtest Feedback 任务归因与续修
5. [ ] 项目记忆
6. [ ] 资产注册面板
7. [ ] Context Budget 可视化
8. [ ] MarkdownViewer 资源链接与代码块对比
9. [ ] 工具/存储/Session 架构巩固
10. [ ] 用户系统同步、权限、审计

## 每次实现前的通用检查

- [ ] 明确本次改动属于哪个专题文档。
- [ ] 明确用户视角流程，不只做底层接口。
- [ ] 明确哪些行为已经存在，哪些是新增目标。
- [ ] 明确安全边界，尤其是文件写入、回滚、外链、云同步、权限。
- [ ] 为 session/state/store 增加单元测试。
- [ ] 为 UI 增加最小可验证测试或手动验证步骤。
- [ ] 更新对应 `.devdocs` 文档和本 TODO 状态。
