/**************************************************************************/
/*  ai_agent_next_session.h                                               */
/**************************************************************************/

#pragma once

#include "editor/ai_component/agent/ai_agent_base.h"
#include "editor/ai_component/next/ai_next_event_log.h"
#include "editor/ai_component/next/ai_next_project_state.h"
#include "editor/ai_component/next/ai_next_project_store.h"
#include "editor/ai_component/tools/ai_tool.h"
#include "core/templates/hash_map.h"
#include "scene/main/node.h"

class AIAgentNextSession : public Node {
	GDCLASS(AIAgentNextSession, Node);

	enum PendingOperation {
		PENDING_OPERATION_NONE,
		PENDING_OPERATION_GENERATE_PLAN,
		PENDING_OPERATION_RUN_TASK,
		PENDING_OPERATION_REVIEW,
		PENDING_OPERATION_FEEDBACK_TASKS,
	};

	Ref<AINextProjectState> project_state;
	Ref<AINextProjectStore> project_store;
	Ref<AINextEventLog> event_log;

	Ref<AIAgentBase> planning_agent;
	Ref<AIAgentBase> script_agent;
	Ref<AIAgentBase> scene_agent;
	Ref<AIAgentBase> shader_agent;
	Ref<AIAgentBase> review_agent;

	bool workflow_active = false;
	PendingOperation pending_operation = PENDING_OPERATION_NONE;
	String pending_agent_id;
	String pending_milestone_id;
	String pending_task_id;
	String workflow_milestone_id;
	String selected_task_id;
	int pending_feedback_previous_task_count = 0;
	int milestone_run_guard = 0;
	Array active_task_batch;
	int active_task_batch_index = 0;
	bool single_task_run = false;
	Array runtime_messages;
	HashMap<int, int> runtime_to_progress_indices;

	void _configure_agent(const Ref<AIAgentBase> &p_agent, const String &p_agent_id, const String &p_prompt, const Vector<Ref<AITool>> &p_tools);
	void _register_next_tools(const Ref<AIAgentBase> &p_agent);
	void _register_shared_read_tools(const Ref<AIAgentBase> &p_agent);
	void _register_specialist_write_tools(const Ref<AIAgentBase> &p_agent, const String &p_agent_id);
	Ref<AIAgentBase> _get_agent(const String &p_agent_id) const;
	bool _is_workflow_active() const;
	void _emit_project_state_changed();
	void _clear_pending_agent_run();
	void _clear_workflow();
	void _clear_runtime_messages();
	void _fail_workflow(const String &p_event_type, const String &p_milestone_id, const String &p_task_id, const String &p_agent_id, const String &p_error);
	bool _begin_agent_run(PendingOperation p_operation, const String &p_agent_id, const Vector<AIAgentMessage> &p_messages, const String &p_milestone_id = String(), const String &p_task_id = String());
	void _on_agent_runtime_finished(const String &p_agent_id);
	void _on_agent_runtime_message_added(int p_index, const Dictionary &p_message, const String &p_agent_id);
	void _on_agent_runtime_message_updated(int p_index, const Dictionary &p_message, const String &p_agent_id);
	void _planning_agent_runtime_finished();
	void _script_agent_runtime_finished();
	void _scene_agent_runtime_finished();
	void _shader_agent_runtime_finished();
	void _review_agent_runtime_finished();
	void _finish_generate_plan(const AIAgentRuntimeResult &p_result);
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

protected:
	static void _bind_methods();

public:
	AIAgentNextSession();

	Ref<AINextProjectState> get_project_state() const;
	Ref<AINextProjectStore> get_project_store() const;
	Ref<AINextEventLog> get_event_log() const;
	bool has_agent(const String &p_agent_id) const;
	Ref<AIAgentBase> get_agent_for_test(const String &p_agent_id) const;
	bool is_workflow_active() const;
	String get_active_operation_name() const;
	Array get_runtime_messages() const;
	String get_selected_task_id() const;
	bool can_run_active_milestone() const;
	bool can_run_task(const String &p_task_id) const;
	bool can_review_active_milestone() const;
	bool can_lock_active_milestone() const;

	void set_model_profile_id(const String &p_model_profile_id);
	void set_agent_model_profile_id(const String &p_agent_id, const String &p_model_profile_id);
	void submit_brief(const String &p_brief);
	bool select_milestone(const String &p_milestone_id);
	bool select_task(const String &p_task_id);
	void generate_plan();
	void approve_plan();
	void run_active_milestone();
	bool run_task(const String &p_task_id);
	void review_active_milestone();
	void generate_feedback_tasks(const String &p_feedback);
	void accept_and_lock_active_milestone();
	void cancel_current_operation();
};
