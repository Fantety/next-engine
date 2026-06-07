/**************************************************************************/
/*  ai_agent_next_session.h                                               */
/**************************************************************************/

#pragma once

#include "editor/ai_component/agent/ai_agent_base.h"
#include "editor/ai_component/agent/ai_session_base.h"
#include "editor/ai_component/context/ai_best_practices_context_provider.h"
#include "editor/ai_component/context/ai_editor_context_provider.h"
#include "editor/ai_component/context/ai_project_tree_context_provider.h"
#include "editor/ai_component/next/ai_next_event_log.h"
#include "editor/ai_component/next/ai_next_project_state.h"
#include "editor/ai_component/next/ai_next_project_store.h"
#include "editor/ai_component/next/ai_next_run_tracker.h"
#include "editor/ai_component/next/ai_next_workflow_snapshot.h"
#include "editor/ai_component/next/ai_next_workflow_store.h"
#include "editor/ai_component/rules/ai_rules_context_provider.h"
#include "editor/ai_component/skills/ai_skill_context_provider.h"
#include "core/templates/hash_map.h"

class AIAgentNextSession : public AISessionBase {
	GDCLASS(AIAgentNextSession, AISessionBase);

	enum PendingOperation {
		PENDING_OPERATION_NONE,
		PENDING_OPERATION_GENERATE_PLAN,
		PENDING_OPERATION_REFINE_PLAN,
		PENDING_OPERATION_RUN_TASK,
		PENDING_OPERATION_REVIEW,
		PENDING_OPERATION_FEEDBACK_TASKS,
	};

	Ref<AINextProjectState> project_state;
	Ref<AINextProjectStore> project_store;
	Ref<AINextWorkflowStore> workflow_store;
	Ref<AINextEventLog> event_log;
	Ref<AIProjectTreeContextProvider> project_tree_context;
	Ref<AIEditorContextProvider> editor_context;
	Ref<AIBestPracticesContextProvider> best_practices_context;
	Ref<AIRulesContextProvider> rules_context;
	Ref<AISkillIndexContextProvider> skill_context;
	AINextRunTracker run_tracker;

	HashMap<String, Ref<AIAgentBase>> agents;

	String workflow_id;
	String workflow_title = "New NEXT Workflow";
	uint64_t workflow_created_at = 0;
	uint64_t workflow_updated_at = 0;
	AINextWorkflowCheckpoint checkpoint;
	String active_workflow_run_id;
	String active_agent_run_id;
	bool workflow_active = false;
	PendingOperation pending_operation = PENDING_OPERATION_NONE;
	String pending_agent_id;
	String pending_milestone_id;
	String pending_task_id;
	String workflow_milestone_id;
	String selected_task_id;
	String pending_feedback_text;
	int pending_feedback_previous_task_count = 0;
	Array pending_feedback_attachments;
	Dictionary pending_requirement_form;
	int milestone_run_guard = 0;
	Array active_task_batch;
	int active_task_batch_index = 0;
	bool single_task_run = false;
	Array runtime_messages;
	HashMap<int, int> runtime_to_progress_indices;
	bool agent_progress_change_queued = false;

	void _add_agent(const String &p_agent_id, const Ref<AIAgentBase> &p_agent);
	void _connect_agent_runtime(const String &p_agent_id, const Ref<AIAgentBase> &p_agent);
	Ref<AIAgentBase> _get_agent(const String &p_agent_id) const;
	String _make_workflow_id() const;
	String _make_run_id(const String &p_prefix) const;
	bool _is_workflow_active() const;
	void _emit_project_state_changed(bool p_save = true);
	void _emit_workflow_session_changed();
	void _queue_agent_progress_changed();
	void _emit_agent_progress_changed_deferred();
	void _clear_pending_agent_run();
	void _clear_workflow();
	void _clear_runtime_messages();
	void _reset_checkpoint();
	Array _collect_initial_context();
	AINextWorkflowSnapshot _build_workflow_snapshot() const;
	void _apply_workflow_snapshot(const AINextWorkflowSnapshot &p_snapshot, bool p_normalize_running_checkpoint);
	void _load_initial_workflow();
	void _start_empty_workflow(bool p_save);
	void _save_current_workflow();
	void _sync_agent_project_state();
	void _normalize_interrupted_checkpoint();
	AINextAgentRunState *_find_agent_run(const String &p_run_id);
	const AINextAgentRunState *_find_agent_run(const String &p_run_id) const;
	AINextAgentRunState *_find_latest_task_agent_run(const String &p_task_id);
	const AINextAgentRunState *_find_latest_task_agent_run(const String &p_task_id) const;
	Vector<AIAgentMessage> _get_or_create_agent_run_messages(const String &p_agent_run_id, const Vector<AIAgentMessage> &p_default_messages) const;
	void _mark_active_agent_run_started(const String &p_run_id, const String &p_agent_id, PendingOperation p_operation, const String &p_milestone_id, const String &p_task_id, const Vector<AIAgentMessage> &p_messages);
	void _store_active_agent_run_progress_message(int p_index, const Dictionary &p_message);
	void _store_active_agent_run_result(const AIAgentRuntimeResult &p_result);
	bool _handle_pending_requirement_form_result(const AIAgentRuntimeResult &p_result, const String &p_agent_id);
	bool _resume_agent_run_after_requirement_form(const Vector<AIAgentMessage> &p_messages);
	void _fail_workflow(const String &p_event_type, const String &p_milestone_id, const String &p_task_id, const String &p_agent_id, const String &p_error);
	bool _begin_agent_run(PendingOperation p_operation, const String &p_agent_id, const Vector<AIAgentMessage> &p_messages, const String &p_milestone_id = String(), const String &p_task_id = String(), const String &p_existing_agent_run_id = String());
	void _on_agent_runtime_finished(const String &p_agent_id);
	void _on_agent_runtime_message_added(int p_index, const Dictionary &p_message, const String &p_agent_id);
	void _on_agent_runtime_message_updated(int p_index, const Dictionary &p_message, const String &p_agent_id);
	AIAgentMessage _make_plan_creation_message() const;
	AIAgentMessage _make_plan_refinement_message() const;
	bool _begin_plan_refinement();
	void _finish_generate_plan(const AIAgentRuntimeResult &p_result);
	void _finish_refine_plan(const AIAgentRuntimeResult &p_result);
	void _finish_review_active_milestone(const AIAgentRuntimeResult &p_result, const String &p_milestone_id);
	void _finish_feedback_tasks(const AIAgentRuntimeResult &p_result, const String &p_milestone_id, int p_previous_task_count);
	void _continue_active_milestone_run();
	void _start_next_task_from_batch();
	void _finish_active_task(const AIAgentRuntimeResult &p_result, const String &p_milestone_id, const String &p_task_id, const String &p_agent_id);
	void _complete_active_milestone_run(bool p_failed);
	void _complete_single_task_run(bool p_failed, const String &p_milestone_id, const String &p_task_id);
	void _sync_selected_task_to_active_milestone();
	String _find_next_unlocked_milestone_id(const String &p_after_milestone_id) const;
	void _set_idle_state_for_active_milestone();
	String _get_checkpoint_operation_name(PendingOperation p_operation) const;
	bool _can_edit_plan_target(const String &p_milestone_id = String()) const;
	bool _finish_user_plan_edit(const String &p_action, const String &p_milestone_id, const String &p_task_id, const String &p_message);

protected:
	static void _bind_methods();

public:
	AIAgentNextSession();

	Ref<AINextProjectState> get_project_state() const;
	Ref<AINextProjectStore> get_project_store() const;
	Ref<AINextEventLog> get_event_log() const;
	String get_workflow_id() const;
	String get_workflow_title() const;
	Array list_workflows() const;
	bool has_agent(const String &p_agent_id) const;
	Ref<AIAgentBase> get_agent_for_test(const String &p_agent_id) const;
	bool is_workflow_active() const;
	String get_active_operation_name() const;
	Array get_runtime_messages() const;
	Array get_recent_runtime_messages(int p_limit) const;
	Dictionary get_pending_requirement_form() const;
	String get_selected_task_id() const;
	bool can_run_active_milestone() const;
	bool can_run_task(const String &p_task_id) const;
	bool can_continue_task_session(const String &p_task_id) const;
	bool can_review_active_milestone() const;
	bool can_lock_active_milestone() const;
	bool can_edit_plan() const;
	Array get_task_session_messages(const String &p_task_id) const;

	void set_model_profile_id(const String &p_model_profile_id);
	void set_agent_model_profile_id(const String &p_agent_id, const String &p_model_profile_id);
	void submit_brief(const String &p_brief);
	void start_new_workflow();
	bool load_workflow(const String &p_workflow_id);
	bool delete_workflow(const String &p_workflow_id);
	bool can_continue_workflow() const;
	bool continue_workflow();
	bool select_milestone(const String &p_milestone_id);
	bool select_task(const String &p_task_id);
	String create_user_milestone(const String &p_title, const String &p_description);
	bool edit_user_milestone(const String &p_milestone_id, const String &p_title, const String &p_description);
	bool delete_user_milestone(const String &p_milestone_id);
	bool move_user_milestone(const String &p_milestone_id, int p_to_index);
	bool merge_user_milestones(const String &p_target_milestone_id, const String &p_source_milestone_id);
	String create_user_task(const String &p_milestone_id, const String &p_title, const String &p_assigned_agent_id, const Array &p_depends_on, const String &p_description);
	String create_user_task(const String &p_milestone_id, const String &p_title, const String &p_assigned_agent_id, const Array &p_depends_on, const String &p_description, const Array &p_attachments);
	bool edit_user_task(const String &p_task_id, const String &p_title, const String &p_description, const String &p_assigned_agent_id);
	bool edit_user_task(const String &p_task_id, const String &p_title, const String &p_description, const String &p_assigned_agent_id, const Array &p_attachments);
	bool delete_user_task(const String &p_task_id);
	bool move_user_task(const String &p_task_id, const String &p_target_milestone_id, int p_to_index);
	bool set_user_task_dependencies(const String &p_task_id, const Array &p_depends_on);
	void generate_plan();
	bool submit_pending_requirement_form(const Dictionary &p_answers);
	void approve_plan();
	void run_active_milestone();
	bool run_task(const String &p_task_id);
	bool send_task_session_message(const String &p_task_id, const String &p_message);
	bool send_task_session_message(const String &p_task_id, const String &p_message, const Array &p_attachments);
	void review_active_milestone();
	void generate_feedback_tasks(const String &p_feedback);
	void generate_feedback_tasks(const String &p_feedback, const Array &p_attachments);
	void accept_and_lock_active_milestone();
	void cancel_current_operation();

	void set_workflow_project_scope_for_test(const String &p_project_scope_key);
	Ref<AINextWorkflowStore> get_workflow_store_for_test() const;
	Dictionary get_workflow_checkpoint_for_test() const;
	Error save_workflow_for_test();
};
