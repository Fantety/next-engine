/**************************************************************************/
/*  ai_agent_next_session.cpp                                             */
/**************************************************************************/

#include "ai_agent_next_session.h"

#include "core/object/class_db.h"
#include "core/object/callable_mp.h"
#include "core/os/time.h"
#include "editor/ai_component/agent/ai_agent_message.h"
#include "editor/ai_component/next/ai_next_agent_registry.h"
#include "editor/ai_component/next/ai_next_workflow_context_builder.h"
#include "editor/ai_component/next/agents/ai_next_agents.h"

namespace {

const int NEXT_MAX_TASK_BATCH_SIZE = 2;

String _planning_agent_id() {
	return AINextAgentRegistry::get_planning_agent_id();
}

String _review_agent_id() {
	return AINextAgentRegistry::get_review_agent_id();
}

String _get_last_response_summary(const AIAgentRuntimeResult &p_result) {
	for (int i = p_result.messages.size() - 1; i >= 0; i--) {
		if (p_result.messages[i].role == AI_AGENT_ROLE_ASSISTANT && !p_result.messages[i].content.strip_edges().is_empty()) {
			return p_result.messages[i].content.strip_edges();
		}
	}
	return "Task completed.";
}

bool _task_conflicts_with_batch(const Dictionary &p_task, const Array &p_claimed_output_paths, const Array &p_claimed_agent_ids) {
	const String agent_id = String(p_task.get("assigned_agent_id", String())).strip_edges();
	if (!agent_id.is_empty() && p_claimed_agent_ids.has(agent_id)) {
		return true;
	}

	Array output_paths = p_task.get("output_paths", Array());
	for (int i = 0; i < output_paths.size(); i++) {
		const String output_path = String(output_paths[i]).strip_edges();
		if (!output_path.is_empty() && p_claimed_output_paths.has(output_path)) {
			return true;
		}
	}
	return false;
}

void _claim_batch_task(const Dictionary &p_task, Array &r_claimed_output_paths, Array &r_claimed_agent_ids) {
	const String agent_id = String(p_task.get("assigned_agent_id", String())).strip_edges();
	if (!agent_id.is_empty() && !r_claimed_agent_ids.has(agent_id)) {
		r_claimed_agent_ids.push_back(agent_id);
	}

	Array output_paths = p_task.get("output_paths", Array());
	for (int i = 0; i < output_paths.size(); i++) {
		const String output_path = String(output_paths[i]).strip_edges();
		if (!output_path.is_empty() && !r_claimed_output_paths.has(output_path)) {
			r_claimed_output_paths.push_back(output_path);
		}
	}
}

Array _select_task_batch(const Array &p_ready_tasks) {
	Array batch;
	Array claimed_output_paths;
	Array claimed_agent_ids;

	for (int i = 0; i < p_ready_tasks.size() && batch.size() < NEXT_MAX_TASK_BATCH_SIZE; i++) {
		if (Variant(p_ready_tasks[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary task = p_ready_tasks[i];
		if (_task_conflicts_with_batch(task, claimed_output_paths, claimed_agent_ids)) {
			continue;
		}
		batch.push_back(task);
		_claim_batch_task(task, claimed_output_paths, claimed_agent_ids);
	}
	return batch;
}

Array _task_ids_from_batch(const Array &p_batch) {
	Array task_ids;
	for (int i = 0; i < p_batch.size(); i++) {
		if (Variant(p_batch[i]).get_type() == Variant::DICTIONARY) {
			Dictionary task = p_batch[i];
			task_ids.push_back(String(task.get("id", String())));
		}
	}
	return task_ids;
}

String _truncate_progress_text(const String &p_text) {
	const String text = p_text.strip_edges().replace("\n", " ");
	if (text.length() <= 180) {
		return text;
	}
	return text.substr(0, 177) + "...";
}

String _summarize_runtime_message(const Dictionary &p_message) {
	const String content = String(p_message.get("content", String())).strip_edges();
	if (!content.is_empty()) {
		return _truncate_progress_text(content);
	}

	Dictionary metadata = p_message.get("metadata", Dictionary());
	if (metadata.has("tool_calls") && Variant(metadata["tool_calls"]).get_type() == Variant::ARRAY) {
		Array tool_calls = metadata["tool_calls"];
		if (!tool_calls.is_empty() && Variant(tool_calls[0]).get_type() == Variant::DICTIONARY) {
			Dictionary tool_call = tool_calls[0];
			const String tool_name = String(tool_call.get("tool_name", tool_call.get("name", String())));
			if (!tool_name.is_empty()) {
				return vformat("Calling %s", tool_name);
			}
		}
		return vformat("Calling %d tool(s)", tool_calls.size());
	}

	const String tool_name = String(metadata.get("tool_name", String()));
	const String status = String(metadata.get("status", String()));
	if (!tool_name.is_empty()) {
		return status.is_empty() ? vformat("Tool result from %s", tool_name) : vformat("%s %s", tool_name, status);
	}

	return String(p_message.get("role", "assistant")).capitalize();
}

bool _runtime_message_records_completed_next_write(const Dictionary &p_message) {
	Dictionary metadata = p_message.get("metadata", Dictionary());
	const String tool_name = String(metadata.get("tool_name", String()));
	const String status = String(metadata.get("status", String()));
	if (tool_name == "ai_next.manage_project" && status == "completed") {
		return true;
	}

	if (!metadata.has("tool_calls") || Variant(metadata["tool_calls"]).get_type() != Variant::ARRAY) {
		return false;
	}

	Array tool_calls = metadata["tool_calls"];
	for (int i = 0; i < tool_calls.size(); i++) {
		if (Variant(tool_calls[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary tool_call = tool_calls[i];
		const String call_tool_name = String(tool_call.get("tool_name", tool_call.get("name", String())));
		const String call_status = String(tool_call.get("status", String()));
		if (call_tool_name == "ai_next.manage_project" && call_status == "completed") {
			return true;
		}
	}
	return false;
}

bool _runtime_message_should_checkpoint(const Dictionary &p_message) {
	const String role = String(p_message.get("role", String()));
	if (role == "tool" || role == "error") {
		return true;
	}

	Dictionary metadata = p_message.get("metadata", Dictionary());
	return metadata.has("tool_calls") && Variant(metadata["tool_calls"]).get_type() == Variant::ARRAY;
}

void _set_message_attachments(AIAgentMessage &r_message, const Array &p_attachments) {
	if (p_attachments.is_empty()) {
		return;
	}

	Dictionary metadata = r_message.metadata;
	metadata["attachments"] = p_attachments.duplicate(true);
	r_message.metadata = metadata;
}

AIAgentMessage _make_task_run_message(const Dictionary &p_task) {
	AIAgentMessage user_message;
	user_message.role = AI_AGENT_ROLE_USER;
	user_message.created_at = Time::get_singleton()->get_unix_time_from_system();
	user_message.content = vformat("Run NEXT task `%s`.\n\nDescription:\n%s\n\nReturn a concise summary and list produced paths if any.",
			String(p_task.get("title", String())),
			String(p_task.get("description", String())));
	if (p_task.has("attachments") && Variant(p_task["attachments"]).get_type() == Variant::ARRAY) {
		_set_message_attachments(user_message, p_task["attachments"]);
	}
	return user_message;
}

} // namespace

void AIAgentNextSession::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_emit_agent_progress_changed_deferred"), &AIAgentNextSession::_emit_agent_progress_changed_deferred);

	ClassDB::bind_method(D_METHOD("get_project_state"), &AIAgentNextSession::get_project_state);
	ClassDB::bind_method(D_METHOD("get_project_store"), &AIAgentNextSession::get_project_store);
	ClassDB::bind_method(D_METHOD("get_event_log"), &AIAgentNextSession::get_event_log);
	ClassDB::bind_method(D_METHOD("get_workflow_id"), &AIAgentNextSession::get_workflow_id);
	ClassDB::bind_method(D_METHOD("get_workflow_title"), &AIAgentNextSession::get_workflow_title);
	ClassDB::bind_method(D_METHOD("list_workflows"), &AIAgentNextSession::list_workflows);
	ClassDB::bind_method(D_METHOD("has_agent", "agent_id"), &AIAgentNextSession::has_agent);
	ClassDB::bind_method(D_METHOD("is_workflow_active"), &AIAgentNextSession::is_workflow_active);
	ClassDB::bind_method(D_METHOD("get_active_operation_name"), &AIAgentNextSession::get_active_operation_name);
	ClassDB::bind_method(D_METHOD("get_runtime_messages"), &AIAgentNextSession::get_runtime_messages);
	ClassDB::bind_method(D_METHOD("get_selected_task_id"), &AIAgentNextSession::get_selected_task_id);
	ClassDB::bind_method(D_METHOD("can_run_active_milestone"), &AIAgentNextSession::can_run_active_milestone);
	ClassDB::bind_method(D_METHOD("can_run_task", "task_id"), &AIAgentNextSession::can_run_task);
	ClassDB::bind_method(D_METHOD("can_continue_task_session", "task_id"), &AIAgentNextSession::can_continue_task_session);
	ClassDB::bind_method(D_METHOD("can_review_active_milestone"), &AIAgentNextSession::can_review_active_milestone);
	ClassDB::bind_method(D_METHOD("can_lock_active_milestone"), &AIAgentNextSession::can_lock_active_milestone);
	ClassDB::bind_method(D_METHOD("can_edit_plan"), &AIAgentNextSession::can_edit_plan);
	ClassDB::bind_method(D_METHOD("get_task_session_messages", "task_id"), &AIAgentNextSession::get_task_session_messages);
	ClassDB::bind_method(D_METHOD("set_model_profile_id", "model_profile_id"), &AIAgentNextSession::set_model_profile_id);
	ClassDB::bind_method(D_METHOD("set_agent_model_profile_id", "agent_id", "model_profile_id"), &AIAgentNextSession::set_agent_model_profile_id);
	ClassDB::bind_method(D_METHOD("submit_brief", "brief"), &AIAgentNextSession::submit_brief);
	ClassDB::bind_method(D_METHOD("start_new_workflow"), &AIAgentNextSession::start_new_workflow);
	ClassDB::bind_method(D_METHOD("load_workflow", "workflow_id"), &AIAgentNextSession::load_workflow);
	ClassDB::bind_method(D_METHOD("delete_workflow", "workflow_id"), &AIAgentNextSession::delete_workflow);
	ClassDB::bind_method(D_METHOD("can_continue_workflow"), &AIAgentNextSession::can_continue_workflow);
	ClassDB::bind_method(D_METHOD("continue_workflow"), &AIAgentNextSession::continue_workflow);
	ClassDB::bind_method(D_METHOD("select_milestone", "milestone_id"), &AIAgentNextSession::select_milestone);
	ClassDB::bind_method(D_METHOD("select_task", "task_id"), &AIAgentNextSession::select_task);
	ClassDB::bind_method(D_METHOD("create_user_milestone", "title", "description"), &AIAgentNextSession::create_user_milestone);
	ClassDB::bind_method(D_METHOD("edit_user_milestone", "milestone_id", "title", "description"), &AIAgentNextSession::edit_user_milestone);
	ClassDB::bind_method(D_METHOD("delete_user_milestone", "milestone_id"), &AIAgentNextSession::delete_user_milestone);
	ClassDB::bind_method(D_METHOD("move_user_milestone", "milestone_id", "to_index"), &AIAgentNextSession::move_user_milestone);
	ClassDB::bind_method(D_METHOD("merge_user_milestones", "target_milestone_id", "source_milestone_id"), &AIAgentNextSession::merge_user_milestones);
	ClassDB::bind_method(D_METHOD("create_user_task", "milestone_id", "title", "assigned_agent_id", "depends_on", "description"), static_cast<String (AIAgentNextSession::*)(const String &, const String &, const String &, const Array &, const String &)>(&AIAgentNextSession::create_user_task));
	ClassDB::bind_method(D_METHOD("edit_user_task", "task_id", "title", "description", "assigned_agent_id"), static_cast<bool (AIAgentNextSession::*)(const String &, const String &, const String &, const String &)>(&AIAgentNextSession::edit_user_task));
	ClassDB::bind_method(D_METHOD("delete_user_task", "task_id"), &AIAgentNextSession::delete_user_task);
	ClassDB::bind_method(D_METHOD("move_user_task", "task_id", "target_milestone_id", "to_index"), &AIAgentNextSession::move_user_task);
	ClassDB::bind_method(D_METHOD("set_user_task_dependencies", "task_id", "depends_on"), &AIAgentNextSession::set_user_task_dependencies);
	ClassDB::bind_method(D_METHOD("generate_plan"), &AIAgentNextSession::generate_plan);
	ClassDB::bind_method(D_METHOD("approve_plan"), &AIAgentNextSession::approve_plan);
	ClassDB::bind_method(D_METHOD("run_active_milestone"), &AIAgentNextSession::run_active_milestone);
	ClassDB::bind_method(D_METHOD("run_task", "task_id"), &AIAgentNextSession::run_task);
	ClassDB::bind_method(D_METHOD("send_task_session_message", "task_id", "message", "attachments"), static_cast<bool (AIAgentNextSession::*)(const String &, const String &, const Array &)>(&AIAgentNextSession::send_task_session_message), DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("review_active_milestone"), &AIAgentNextSession::review_active_milestone);
	ClassDB::bind_method(D_METHOD("generate_feedback_tasks", "feedback", "attachments"), static_cast<void (AIAgentNextSession::*)(const String &, const Array &)>(&AIAgentNextSession::generate_feedback_tasks), DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("accept_and_lock_active_milestone"), &AIAgentNextSession::accept_and_lock_active_milestone);
	ClassDB::bind_method(D_METHOD("cancel_current_operation"), &AIAgentNextSession::cancel_current_operation);

	ADD_SIGNAL(MethodInfo("state_changed", PropertyInfo(Variant::STRING, "state")));
	ADD_SIGNAL(MethodInfo("workflow_session_changed"));
	ADD_SIGNAL(MethodInfo("project_state_changed"));
	ADD_SIGNAL(MethodInfo("agent_progress_changed"));
}

AIAgentNextSession::AIAgentNextSession() {
	project_state.instantiate();
	project_store.instantiate();
	workflow_store.instantiate();
	event_log.instantiate();
	project_tree_context.instantiate();
	editor_context.instantiate();
	best_practices_context.instantiate();
	rules_context.instantiate();
	skill_context.instantiate();

	Vector<String> agent_ids = AINextAgentRegistry::get_agent_ids();
	for (int i = 0; i < agent_ids.size(); i++) {
		const String agent_id = agent_ids[i];
		_add_agent(agent_id, AINextAgents::create_agent(agent_id, project_state));
	}

	workflow_store->set_project_scope(_get_project_scope_key());
	_load_initial_workflow();
}

void AIAgentNextSession::_add_agent(const String &p_agent_id, const Ref<AIAgentBase> &p_agent) {
	ERR_FAIL_COND(p_agent.is_null());
	agents[p_agent_id] = p_agent;
	_connect_agent_runtime(p_agent_id, p_agent);
}

void AIAgentNextSession::_connect_agent_runtime(const String &p_agent_id, const Ref<AIAgentBase> &p_agent) {
	ERR_FAIL_COND(p_agent.is_null());

	_connect_runtime_signals(
			p_agent->get_runtime_runner(),
			callable_mp(this, &AIAgentNextSession::_on_agent_runtime_finished).bind(p_agent_id),
			callable_mp(this, &AIAgentNextSession::_on_agent_runtime_message_added).bind(p_agent_id),
			callable_mp(this, &AIAgentNextSession::_on_agent_runtime_message_updated).bind(p_agent_id));
}

Ref<AIAgentBase> AIAgentNextSession::_get_agent(const String &p_agent_id) const {
	const Ref<AIAgentBase> *agent = agents.getptr(p_agent_id);
	if (agent) {
		return *agent;
	}
	return Ref<AIAgentBase>();
}

String AIAgentNextSession::_make_workflow_id() const {
	return _make_unique_id();
}

String AIAgentNextSession::_make_run_id(const String &p_prefix) const {
	return _make_unique_id(p_prefix);
}

bool AIAgentNextSession::_is_workflow_active() const {
	return workflow_active || pending_operation != PENDING_OPERATION_NONE;
}

void AIAgentNextSession::_emit_project_state_changed(bool p_save) {
	if (p_save) {
		_save_current_workflow();
	}
	emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
	emit_signal(SNAME("project_state_changed"));
}

void AIAgentNextSession::_emit_workflow_session_changed() {
	emit_signal(SNAME("workflow_session_changed"));
}

void AIAgentNextSession::_queue_agent_progress_changed() {
	if (agent_progress_change_queued) {
		return;
	}
	agent_progress_change_queued = true;
	call_deferred(SNAME("_emit_agent_progress_changed_deferred"));
}

void AIAgentNextSession::_emit_agent_progress_changed_deferred() {
	agent_progress_change_queued = false;
	emit_signal(SNAME("agent_progress_changed"));
}

void AIAgentNextSession::_clear_pending_agent_run() {
	pending_operation = PENDING_OPERATION_NONE;
	pending_agent_id = String();
	pending_milestone_id = String();
	pending_task_id = String();
	pending_feedback_text.clear();
	pending_feedback_previous_task_count = 0;
	pending_feedback_attachments.clear();
	active_workflow_run_id.clear();
	active_agent_run_id.clear();
}

void AIAgentNextSession::_clear_workflow() {
	workflow_active = false;
	_clear_pending_agent_run();
	workflow_milestone_id.clear();
	single_task_run = false;
	milestone_run_guard = 0;
	active_task_batch = Array();
	active_task_batch_index = 0;
	_reset_checkpoint();
}

void AIAgentNextSession::_clear_runtime_messages() {
	runtime_messages.clear();
	runtime_to_progress_indices.clear();
	_queue_agent_progress_changed();
}

void AIAgentNextSession::_reset_checkpoint() {
	checkpoint = AINextWorkflowCheckpoint();
}

Array AIAgentNextSession::_collect_initial_context() {
	Array context;
	context.append_array(editor_context->collect_context());
	context.append_array(project_tree_context->collect_context());
	context.append_array(best_practices_context->collect_context());
	context.append_array(rules_context->collect_context());
	context.append_array(skill_context->collect_context());
	return context;
}

AINextWorkflowSnapshot AIAgentNextSession::_build_workflow_snapshot() const {
	AINextWorkflowSnapshot snapshot;
	snapshot.id = workflow_id;
	snapshot.title = workflow_title;
	snapshot.created_at = workflow_created_at;
	snapshot.updated_at = workflow_updated_at;
	snapshot.project_state = project_state;
	snapshot.event_log = event_log.is_valid() ? event_log->to_array() : Array();
	snapshot.checkpoint = checkpoint;
	snapshot.agent_runs = run_tracker.get_runs();
	return snapshot;
}

void AIAgentNextSession::_apply_workflow_snapshot(const AINextWorkflowSnapshot &p_snapshot, bool p_normalize_running_checkpoint) {
	workflow_id = p_snapshot.id;
	workflow_title = p_snapshot.title.is_empty() ? String("New NEXT Workflow") : p_snapshot.title;
	workflow_created_at = p_snapshot.created_at;
	workflow_updated_at = p_snapshot.updated_at;
	project_state = p_snapshot.project_state;
	if (project_state.is_null()) {
		project_state.instantiate();
	}
	_sync_agent_project_state();
	if (event_log.is_null()) {
		event_log.instantiate();
	}
	event_log->load_from_array(p_snapshot.event_log);
	checkpoint = p_snapshot.checkpoint;
	run_tracker.set_runs(p_snapshot.agent_runs);

	workflow_active = false;
	_clear_pending_agent_run();
	workflow_milestone_id = checkpoint.milestone_id;
	selected_task_id = checkpoint.selected_task_id;
	pending_feedback_text = checkpoint.feedback_text;
	pending_feedback_previous_task_count = checkpoint.feedback_previous_task_count;
	pending_feedback_attachments = checkpoint.feedback_attachments.duplicate(true);
	active_task_batch = checkpoint.active_task_batch.duplicate(true);
	active_task_batch_index = checkpoint.active_task_batch_index;
	single_task_run = checkpoint.single_task_run;
	milestone_run_guard = 0;
	_clear_runtime_messages();

	if (p_normalize_running_checkpoint && checkpoint.status == "running") {
		_normalize_interrupted_checkpoint();
	}
	if (selected_task_id.is_empty()) {
		_sync_selected_task_to_active_milestone();
	}
}

void AIAgentNextSession::_load_initial_workflow() {
	String latest_workflow_id;
	AINextWorkflowSnapshot snapshot;
	if (workflow_store.is_valid() && workflow_store->get_most_recent_workflow_id(latest_workflow_id) && workflow_store->load_workflow(latest_workflow_id, snapshot)) {
		_apply_workflow_snapshot(snapshot, true);
		_save_current_workflow();
		_emit_workflow_session_changed();
		_emit_project_state_changed(false);
		return;
	}

	_start_empty_workflow(true);
}

void AIAgentNextSession::_start_empty_workflow(bool p_save) {
	project_state.instantiate();
	_sync_agent_project_state();
	event_log.instantiate();
	run_tracker.clear();
	workflow_id = _make_workflow_id();
	workflow_title = "New NEXT Workflow";
	workflow_created_at = Time::get_singleton()->get_unix_time_from_system();
	workflow_updated_at = workflow_created_at;
	workflow_active = false;
	_clear_pending_agent_run();
	workflow_milestone_id.clear();
	selected_task_id.clear();
	pending_feedback_text.clear();
	pending_feedback_previous_task_count = 0;
	pending_feedback_attachments.clear();
	milestone_run_guard = 0;
	active_task_batch = Array();
	active_task_batch_index = 0;
	single_task_run = false;
	_reset_checkpoint();
	_clear_runtime_messages();
	if (p_save) {
		_save_current_workflow();
	}
	_emit_workflow_session_changed();
	_emit_project_state_changed(!p_save);
}

void AIAgentNextSession::_save_current_workflow() {
	if (workflow_store.is_null() || workflow_id.strip_edges().is_empty()) {
		return;
	}
	workflow_updated_at = Time::get_singleton()->get_unix_time_from_system();
	Error err = workflow_store->save_workflow(_build_workflow_snapshot());
	if (err != OK) {
		print_line(vformat("[AI Agent][NEXT] Failed to save workflow %s (error %d).", workflow_id, err));
	}
}

void AIAgentNextSession::_sync_agent_project_state() {
	Ref<AIAgentBase> base_agent = _get_agent(_planning_agent_id());
	if (base_agent.is_null()) {
		return;
	}
	AINextPlanningAgent *planning_agent = Object::cast_to<AINextPlanningAgent>(*base_agent);
	if (planning_agent) {
		planning_agent->set_project_state(project_state);
	}
}

void AIAgentNextSession::_normalize_interrupted_checkpoint() {
	if (checkpoint.operation.is_empty() || checkpoint.operation == "none") {
		checkpoint.status = "idle";
		return;
	}

	checkpoint.status = "user_terminated";
	if (!checkpoint.task_id.is_empty() && project_state.is_valid()) {
		project_state->reset_interrupted_task(checkpoint.task_id);
	}
	if (!checkpoint.agent_run_id.is_empty()) {
		AINextAgentRunState *run_state = _find_agent_run(checkpoint.agent_run_id);
		if (run_state) {
			run_state->status = "user_terminated";
			run_state->updated_at = Time::get_singleton()->get_unix_time_from_system();
		}
	}
	if (project_state.is_valid()) {
		project_state->set_session_state(AI_NEXT_SESSION_IDLE);
	}
}

AINextAgentRunState *AIAgentNextSession::_find_agent_run(const String &p_run_id) {
	return run_tracker.find_run(p_run_id);
}

const AINextAgentRunState *AIAgentNextSession::_find_agent_run(const String &p_run_id) const {
	return run_tracker.find_run(p_run_id);
}

AINextAgentRunState *AIAgentNextSession::_find_latest_task_agent_run(const String &p_task_id) {
	return run_tracker.find_latest_task_run(p_task_id);
}

const AINextAgentRunState *AIAgentNextSession::_find_latest_task_agent_run(const String &p_task_id) const {
	return run_tracker.find_latest_task_run(p_task_id);
}

Vector<AIAgentMessage> AIAgentNextSession::_get_or_create_agent_run_messages(const String &p_agent_run_id, const Vector<AIAgentMessage> &p_default_messages) const {
	return run_tracker.get_or_create_run_messages(p_agent_run_id, p_default_messages);
}

void AIAgentNextSession::_mark_active_agent_run_started(const String &p_run_id, const String &p_agent_id, PendingOperation p_operation, const String &p_milestone_id, const String &p_task_id, const Vector<AIAgentMessage> &p_messages) {
	run_tracker.mark_run_started(p_run_id, workflow_id, p_agent_id, _get_checkpoint_operation_name(p_operation), p_milestone_id, p_task_id, p_messages);
}

void AIAgentNextSession::_store_active_agent_run_progress_message(int p_index, const Dictionary &p_message) {
	if (active_agent_run_id.is_empty()) {
		return;
	}
	run_tracker.store_progress_message(active_agent_run_id, p_index, p_message);
}

void AIAgentNextSession::_store_active_agent_run_result(const AIAgentRuntimeResult &p_result) {
	if (active_agent_run_id.is_empty()) {
		return;
	}
	run_tracker.store_result(active_agent_run_id, p_result);
}

void AIAgentNextSession::_fail_workflow(const String &p_event_type, const String &p_milestone_id, const String &p_task_id, const String &p_agent_id, const String &p_error) {
	AINextWorkflowCheckpoint failed_checkpoint = checkpoint;
	failed_checkpoint.status = "failed";
	_clear_workflow();
	checkpoint = failed_checkpoint;
	project_state->set_session_state(AI_NEXT_SESSION_FAILED);
	event_log->record_event(p_event_type, p_milestone_id, p_task_id, p_agent_id, p_error);
	_emit_project_state_changed();
}

bool AIAgentNextSession::_begin_agent_run(PendingOperation p_operation, const String &p_agent_id, const Vector<AIAgentMessage> &p_messages, const String &p_milestone_id, const String &p_task_id, const String &p_existing_agent_run_id) {
	if (pending_operation != PENDING_OPERATION_NONE) {
		return false;
	}

	Ref<AIAgentBase> agent = _get_agent(p_agent_id);
	if (agent.is_null()) {
		return false;
	}

	const String agent_run_id = p_existing_agent_run_id.is_empty() ? _make_run_id("agent_run") : p_existing_agent_run_id;
	Vector<AIAgentMessage> run_messages = _get_or_create_agent_run_messages(agent_run_id, p_messages);
	active_workflow_run_id = _make_run_id("workflow_run");
	active_agent_run_id = agent_run_id;
	pending_operation = p_operation;
	pending_agent_id = p_agent_id;
	pending_milestone_id = p_milestone_id;
	pending_task_id = p_task_id;

	checkpoint.status = "running";
	checkpoint.operation = _get_checkpoint_operation_name(p_operation);
	checkpoint.workflow_run_id = active_workflow_run_id;
	checkpoint.agent_run_id = active_agent_run_id;
	checkpoint.agent_id = p_agent_id;
	checkpoint.milestone_id = p_milestone_id;
	checkpoint.task_id = p_task_id;
	checkpoint.single_task_run = single_task_run;
	checkpoint.feedback_text = pending_feedback_text;
	checkpoint.feedback_previous_task_count = pending_feedback_previous_task_count;
	checkpoint.feedback_attachments = pending_feedback_attachments.duplicate(true);
	checkpoint.selected_task_id = selected_task_id;
	checkpoint.active_task_batch = active_task_batch.duplicate(true);
	checkpoint.active_task_batch_index = active_task_batch_index;

	if (!p_task_id.is_empty() && project_state.is_valid()) {
		Dictionary patch;
		patch["run_id"] = active_agent_run_id;
		String error;
		project_state->update_task(p_task_id, patch, error);
	}
	_mark_active_agent_run_started(active_agent_run_id, p_agent_id, p_operation, p_milestone_id, p_task_id, run_messages);
	_save_current_workflow();

	AINextWorkflowContextOptions context_options;
	context_options.operation = _get_checkpoint_operation_name(p_operation);
	context_options.milestone_id = p_milestone_id;
	context_options.task_id = p_task_id;
	Array context_documents = AINextWorkflowContextBuilder::build_context(project_state, event_log.is_valid() ? event_log->get_events() : Array(), context_options);
	context_documents.append_array(_collect_initial_context());
	if (!agent->start(run_messages, context_documents)) {
		AINextAgentRunState *run_state = _find_agent_run(active_agent_run_id);
		if (run_state) {
			run_state->status = "failed";
			run_state->updated_at = Time::get_singleton()->get_unix_time_from_system();
		}
		checkpoint.status = "failed";
		_save_current_workflow();
		_clear_pending_agent_run();
		return false;
	}
	return true;
}

void AIAgentNextSession::_on_agent_runtime_message_added(int p_index, const Dictionary &p_message, const String &p_agent_id) {
	if (!_is_workflow_active() || p_agent_id != pending_agent_id || checkpoint.status != "running") {
		return;
	}

	if (_runtime_message_should_checkpoint(p_message)) {
		_store_active_agent_run_progress_message(p_index, p_message);
	}

	Dictionary progress_message;
	progress_message["agent_id"] = p_agent_id;
	progress_message["milestone_id"] = pending_milestone_id;
	progress_message["task_id"] = pending_task_id;
	progress_message["runtime_index"] = p_index;
	progress_message["role"] = String(p_message.get("role", String()));
	progress_message["content"] = _summarize_runtime_message(p_message);
	progress_message["message"] = p_message;

	runtime_to_progress_indices[p_index] = runtime_messages.size();
	runtime_messages.push_back(progress_message);
	if (_runtime_message_records_completed_next_write(p_message)) {
		_save_current_workflow();
		_emit_project_state_changed(false);
	}
	_queue_agent_progress_changed();
}

void AIAgentNextSession::_on_agent_runtime_message_updated(int p_index, const Dictionary &p_message, const String &p_agent_id) {
	if (!_is_workflow_active() || p_agent_id != pending_agent_id || checkpoint.status != "running") {
		return;
	}

	if (_runtime_message_should_checkpoint(p_message)) {
		_store_active_agent_run_progress_message(p_index, p_message);
	}

	const int *progress_index = runtime_to_progress_indices.getptr(p_index);
	if (!progress_index || *progress_index < 0 || *progress_index >= runtime_messages.size()) {
		_on_agent_runtime_message_added(p_index, p_message, p_agent_id);
		return;
	}

	Dictionary progress_message = runtime_messages[*progress_index];
	progress_message["agent_id"] = p_agent_id;
	progress_message["milestone_id"] = pending_milestone_id;
	progress_message["task_id"] = pending_task_id;
	progress_message["runtime_index"] = p_index;
	progress_message["role"] = String(p_message.get("role", String()));
	progress_message["content"] = _summarize_runtime_message(p_message);
	progress_message["message"] = p_message;
	runtime_messages[*progress_index] = progress_message;
	if (_runtime_message_records_completed_next_write(p_message)) {
		_save_current_workflow();
		_emit_project_state_changed(false);
	}
	_queue_agent_progress_changed();
}

void AIAgentNextSession::_on_agent_runtime_finished(const String &p_agent_id) {
	if (!_is_workflow_active() || pending_operation == PENDING_OPERATION_NONE || pending_agent_id != p_agent_id) {
		return;
	}
	if (checkpoint.status != "running" || checkpoint.workflow_run_id != active_workflow_run_id || checkpoint.agent_run_id != active_agent_run_id) {
		print_line(vformat("[AI Agent][NEXT] Ignored stale runtime result. workflow=%s agent=%s", workflow_id, p_agent_id));
		return;
	}

	Ref<AIAgentBase> agent = _get_agent(p_agent_id);
	if (agent.is_null()) {
		_fail_workflow("operation_failed", pending_milestone_id, pending_task_id, p_agent_id, "NEXT agent is not configured.");
		return;
	}

	const PendingOperation finished_operation = pending_operation;
	const String finished_milestone_id = pending_milestone_id;
	const String finished_task_id = pending_task_id;
	const String finished_agent_id = pending_agent_id;
	const int previous_task_count = pending_feedback_previous_task_count;
	AIAgentRuntimeResult result = agent->get_runtime_runner()->get_last_result();
	_store_active_agent_run_result(result);
	_clear_pending_agent_run();

	switch (finished_operation) {
		case PENDING_OPERATION_GENERATE_PLAN:
			_finish_generate_plan(result);
			break;
		case PENDING_OPERATION_RUN_TASK:
			_finish_active_task(result, finished_milestone_id, finished_task_id, finished_agent_id);
			break;
		case PENDING_OPERATION_REVIEW:
			_finish_review_active_milestone(result, finished_milestone_id);
			break;
		case PENDING_OPERATION_FEEDBACK_TASKS:
			_finish_feedback_tasks(result, finished_milestone_id, previous_task_count);
			break;
		case PENDING_OPERATION_NONE:
			break;
	}
}

void AIAgentNextSession::_finish_generate_plan(const AIAgentRuntimeResult &p_result) {
	if (!p_result.success) {
		_fail_workflow("planning_failed", String(), String(), _planning_agent_id(), p_result.error);
		return;
	}

	if (project_state->get_milestone_count() <= 0) {
		_fail_workflow("planning_failed", String(), String(), _planning_agent_id(), "Planning completed without writing NEXT milestones.");
		return;
	}

	_clear_workflow();
	_sync_selected_task_to_active_milestone();
	project_state->set_session_state(AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL);
	event_log->record_event("planning_completed", project_state->get_active_milestone_id(), String(), _planning_agent_id(), "NEXT planning completed.");
	_emit_project_state_changed();
}

void AIAgentNextSession::_finish_review_active_milestone(const AIAgentRuntimeResult &p_result, const String &p_milestone_id) {
	if (!p_result.success) {
		_fail_workflow("review_failed", p_milestone_id, String(), _review_agent_id(), p_result.error);
		return;
	}

	const String findings = _get_last_response_summary(p_result);
	Dictionary review_metadata;
	review_metadata["findings"] = findings;
	event_log->record_event("review_completed", p_milestone_id, String(), _review_agent_id(), findings, review_metadata);
	_clear_workflow();
	project_state->set_session_state(AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL);
	_emit_project_state_changed();
}

void AIAgentNextSession::_finish_feedback_tasks(const AIAgentRuntimeResult &p_result, const String &p_milestone_id, int p_previous_task_count) {
	if (!p_result.success) {
		_fail_workflow("feedback_planning_failed", p_milestone_id, String(), _planning_agent_id(), p_result.error);
		return;
	}

	if (project_state->get_task_count(p_milestone_id) <= p_previous_task_count) {
		_fail_workflow("feedback_planning_failed", p_milestone_id, String(), _planning_agent_id(), "Feedback planning completed without appending tasks.");
		return;
	}

	_clear_workflow();
	project_state->set_session_state(AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL);
	event_log->record_event("feedback_tasks_generated", p_milestone_id, String(), _planning_agent_id(), "NEXT feedback tasks generated.");
	_emit_project_state_changed();
}

void AIAgentNextSession::_continue_active_milestone_run() {
	if (!workflow_active) {
		return;
	}

	const String milestone_id = workflow_milestone_id.is_empty() ? project_state->get_active_milestone_id() : workflow_milestone_id;
	if (milestone_id.is_empty()) {
		_fail_workflow("milestone_run_failed", String(), String(), "next_session", "No active NEXT milestone.");
		return;
	}

	if (milestone_run_guard++ >= 100) {
		_fail_workflow("milestone_run_failed", milestone_id, String(), "next_session", "NEXT milestone run exceeded the scheduler guard.");
		return;
	}

	Array ready_tasks = project_state->get_ready_tasks(milestone_id);
	if (ready_tasks.is_empty()) {
		_complete_active_milestone_run(false);
		return;
	}

	active_task_batch = _select_task_batch(ready_tasks);
	if (active_task_batch.is_empty()) {
		active_task_batch.push_back(ready_tasks[0]);
	}
	active_task_batch_index = 0;

	Dictionary batch_metadata;
	batch_metadata["batch_size"] = active_task_batch.size();
	batch_metadata["task_ids"] = _task_ids_from_batch(active_task_batch);
	event_log->record_event("task_batch_started", milestone_id, String(), "next_session", "NEXT task batch started.", batch_metadata);

	_start_next_task_from_batch();
}

void AIAgentNextSession::_start_next_task_from_batch() {
	if (!workflow_active) {
		return;
	}

	const String milestone_id = workflow_milestone_id.is_empty() ? project_state->get_active_milestone_id() : workflow_milestone_id;
	while (active_task_batch_index < active_task_batch.size()) {
		if (Variant(active_task_batch[active_task_batch_index]).get_type() != Variant::DICTIONARY) {
			active_task_batch_index++;
			continue;
		}

		Dictionary task = active_task_batch[active_task_batch_index];
		const String task_id = String(task.get("id", String()));
		const String agent_id = String(task.get("assigned_agent_id", String()));
		Ref<AIAgentBase> agent = _get_agent(agent_id);
		if (agent.is_null()) {
			project_state->mark_task_failed(task_id, "Unknown NEXT agent: " + agent_id);
			event_log->record_event("task_failed", milestone_id, task_id, agent_id, "Unknown NEXT agent.");
			_complete_active_milestone_run(true);
			return;
		}

		project_state->set_task_status(task_id, AI_NEXT_TASK_IN_PROGRESS);
		selected_task_id = task_id;
		event_log->record_event("task_started", milestone_id, task_id, agent_id, String(task.get("title", String())));
		_emit_project_state_changed();

	Vector<AIAgentMessage> messages;
		messages.push_back(_make_task_run_message(task));
		if (!_begin_agent_run(PENDING_OPERATION_RUN_TASK, agent_id, messages, milestone_id, task_id)) {
			project_state->mark_task_failed(task_id, "Failed to start NEXT agent runtime.");
			event_log->record_event("task_failed", milestone_id, task_id, agent_id, "Failed to start NEXT agent runtime.");
			_complete_active_milestone_run(true);
		}
		return;
	}

	active_task_batch = Array();
	active_task_batch_index = 0;
	_continue_active_milestone_run();
}

void AIAgentNextSession::_finish_active_task(const AIAgentRuntimeResult &p_result, const String &p_milestone_id, const String &p_task_id, const String &p_agent_id) {
	if (!p_result.success) {
		project_state->mark_task_failed(p_task_id, p_result.error);
		event_log->record_event("task_failed", p_milestone_id, p_task_id, p_agent_id, p_result.error);
		if (single_task_run) {
			_complete_single_task_run(true, p_milestone_id, p_task_id);
		} else {
			_complete_active_milestone_run(true);
		}
		return;
	}

	project_state->mark_task_completed(p_task_id, _get_last_response_summary(p_result), Array());
	event_log->record_event("task_completed", p_milestone_id, p_task_id, p_agent_id, _get_last_response_summary(p_result));
	if (single_task_run) {
		_complete_single_task_run(false, p_milestone_id, p_task_id);
		return;
	}
	active_task_batch_index++;
	_start_next_task_from_batch();
}

void AIAgentNextSession::_complete_active_milestone_run(bool p_failed) {
	const String milestone_id = workflow_milestone_id.is_empty() ? project_state->get_active_milestone_id() : workflow_milestone_id;
	_clear_workflow();
	project_state->set_session_state(p_failed ? AI_NEXT_SESSION_FAILED : AI_NEXT_SESSION_WAITING_PLAYTEST);
	event_log->record_event(p_failed ? "milestone_run_failed" : "milestone_run_completed", milestone_id, String(), "next_session", p_failed ? "NEXT milestone run failed." : "NEXT milestone run completed.");
	_emit_project_state_changed();
}

void AIAgentNextSession::_complete_single_task_run(bool p_failed, const String &p_milestone_id, const String &p_task_id) {
	_clear_workflow();
	if (p_failed) {
		project_state->set_session_state(AI_NEXT_SESSION_FAILED);
		event_log->record_event("task_run_failed", p_milestone_id, p_task_id, "next_session", "NEXT task run failed.");
	} else {
		project_state->set_session_state(project_state->can_run_milestone(p_milestone_id) ? AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL : AI_NEXT_SESSION_WAITING_PLAYTEST);
		event_log->record_event("task_run_completed", p_milestone_id, p_task_id, "next_session", "NEXT task run completed.");
	}
	_emit_project_state_changed();
}

void AIAgentNextSession::_sync_selected_task_to_active_milestone() {
	selected_task_id.clear();
	if (project_state.is_null()) {
		return;
	}

	const String milestone_id = project_state->get_active_milestone_id();
	Dictionary milestone = project_state->get_milestone(milestone_id);
	Array tasks = milestone.get("tasks", Array());
	if (tasks.is_empty()) {
		return;
	}
	for (int i = 0; i < tasks.size(); i++) {
		if (Variant(tasks[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary task = tasks[i];
		const String task_id = String(task.get("id", String()));
		if (!task_id.is_empty()) {
			selected_task_id = task_id;
			return;
		}
	}
}

String AIAgentNextSession::_find_next_unlocked_milestone_id(const String &p_after_milestone_id) const {
	if (project_state.is_null()) {
		return String();
	}

	Array milestones = project_state->get_milestones_as_array();
	bool after_current = p_after_milestone_id.is_empty();
	for (int i = 0; i < milestones.size(); i++) {
		if (Variant(milestones[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary milestone = milestones[i];
		const String milestone_id = String(milestone.get("id", String()));
		if (after_current && String(milestone.get("status", String())) != "locked") {
			return milestone_id;
		}
		if (milestone_id == p_after_milestone_id) {
			after_current = true;
		}
	}
	return String();
}

void AIAgentNextSession::_set_idle_state_for_active_milestone() {
	if (project_state.is_null()) {
		return;
	}

	Dictionary milestone = project_state->get_milestone(project_state->get_active_milestone_id());
	const String status = String(milestone.get("status", String()));
	if (status == "waiting_playtest" || status == "ready_to_lock") {
		project_state->set_session_state(AI_NEXT_SESSION_WAITING_PLAYTEST);
	} else if (status == "locked") {
		project_state->set_session_state(AI_NEXT_SESSION_READY_TO_LOCK);
	} else {
		project_state->set_session_state(AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL);
	}
}

String AIAgentNextSession::_get_checkpoint_operation_name(PendingOperation p_operation) const {
	switch (p_operation) {
		case PENDING_OPERATION_GENERATE_PLAN:
			return "generate_plan";
		case PENDING_OPERATION_RUN_TASK:
			return single_task_run ? String("run_task") : String("run_milestone");
		case PENDING_OPERATION_REVIEW:
			return "review";
		case PENDING_OPERATION_FEEDBACK_TASKS:
			return "feedback_tasks";
		case PENDING_OPERATION_NONE:
		default:
			return "none";
	}
}

Ref<AINextProjectState> AIAgentNextSession::get_project_state() const {
	return project_state;
}

Ref<AINextProjectStore> AIAgentNextSession::get_project_store() const {
	return project_store;
}

Ref<AINextEventLog> AIAgentNextSession::get_event_log() const {
	return event_log;
}

String AIAgentNextSession::get_workflow_id() const {
	return workflow_id;
}

String AIAgentNextSession::get_workflow_title() const {
	return workflow_title;
}

Array AIAgentNextSession::list_workflows() const {
	return workflow_store.is_valid() ? workflow_store->list_workflows() : Array();
}

bool AIAgentNextSession::has_agent(const String &p_agent_id) const {
	return _get_agent(p_agent_id).is_valid();
}

Ref<AIAgentBase> AIAgentNextSession::get_agent_for_test(const String &p_agent_id) const {
	return _get_agent(p_agent_id);
}

bool AIAgentNextSession::is_workflow_active() const {
	return _is_workflow_active();
}

String AIAgentNextSession::get_active_operation_name() const {
	if (!_is_workflow_active()) {
		return String();
	}

	switch (pending_operation) {
		case PENDING_OPERATION_GENERATE_PLAN:
			return "Planning milestones";
		case PENDING_OPERATION_RUN_TASK: {
			Dictionary task = project_state->get_task(pending_task_id);
			const String title = String(task.get("title", String())).strip_edges();
			return title.is_empty() ? "Running task" : "Running task: " + title;
		}
		case PENDING_OPERATION_REVIEW:
			return "Reviewing milestone";
		case PENDING_OPERATION_FEEDBACK_TASKS:
			return "Planning feedback tasks";
		case PENDING_OPERATION_NONE:
			break;
	}

	if (workflow_active) {
		return "Scheduling NEXT tasks";
	}
	return String();
}

Array AIAgentNextSession::get_runtime_messages() const {
	return runtime_messages.duplicate(true);
}

Array AIAgentNextSession::get_recent_runtime_messages(int p_limit) const {
	Array messages;
	if (p_limit <= 0) {
		return messages;
	}

	const int start = MAX(0, runtime_messages.size() - p_limit);
	for (int i = start; i < runtime_messages.size(); i++) {
		if (Variant(runtime_messages[i]).get_type() == Variant::DICTIONARY) {
			Dictionary message = runtime_messages[i];
			messages.push_back(message.duplicate(true));
		} else {
			messages.push_back(runtime_messages[i]);
		}
	}
	return messages;
}

Array AIAgentNextSession::get_task_session_messages(const String &p_task_id) const {
	Array messages;
	if (project_state.is_null() || !project_state->has_task(p_task_id)) {
		return messages;
	}

	Dictionary task = project_state->get_task(p_task_id);
	const String run_id = String(task.get("run_id", String()));
	const AINextAgentRunState *run_state = _find_agent_run(run_id);
	if (!run_state) {
		run_state = _find_latest_task_agent_run(p_task_id);
	}
	if (!run_state) {
		return messages;
	}

	for (int i = 0; i < run_state->messages.size(); i++) {
		messages.push_back(run_state->messages[i].to_dict());
	}
	return messages;
}

String AIAgentNextSession::get_selected_task_id() const {
	return selected_task_id;
}

bool AIAgentNextSession::can_run_active_milestone() const {
	if (project_state.is_null() || _is_workflow_active()) {
		return false;
	}
	return project_state->can_run_milestone(project_state->get_active_milestone_id());
}

bool AIAgentNextSession::can_run_task(const String &p_task_id) const {
	if (project_state.is_null() || _is_workflow_active()) {
		return false;
	}
	Dictionary task = project_state->get_task(p_task_id);
	return String(task.get("status", String())) == "ready";
}

bool AIAgentNextSession::can_continue_task_session(const String &p_task_id) const {
	if (project_state.is_null() || _is_workflow_active() || !project_state->has_task(p_task_id)) {
		return false;
	}
	const String milestone_id = project_state->get_task_milestone_id(p_task_id);
	Dictionary milestone = project_state->get_milestone(milestone_id);
	if (milestone.is_empty() || String(milestone.get("status", String())) == "locked") {
		return false;
	}
	Dictionary task = project_state->get_task(p_task_id);
	if (_get_agent(String(task.get("assigned_agent_id", String()))).is_null()) {
		return false;
	}
	const String run_id = String(task.get("run_id", String()));
	const AINextAgentRunState *run_state = _find_agent_run(run_id);
	if (!run_state) {
		run_state = _find_latest_task_agent_run(p_task_id);
	}
	return run_state && !run_state->messages.is_empty();
}

bool AIAgentNextSession::can_review_active_milestone() const {
	if (project_state.is_null() || _is_workflow_active()) {
		return false;
	}
	Dictionary milestone = project_state->get_milestone(project_state->get_active_milestone_id());
	const String status = String(milestone.get("status", String()));
	return status == "waiting_playtest" || status == "ready_to_lock";
}

bool AIAgentNextSession::can_lock_active_milestone() const {
	if (project_state.is_null() || _is_workflow_active()) {
		return false;
	}
	String error;
	return project_state->can_lock_milestone(project_state->get_active_milestone_id(), error);
}

bool AIAgentNextSession::_can_edit_plan_target(const String &p_milestone_id) const {
	if (project_state.is_null() || _is_workflow_active()) {
		return false;
	}

	if (p_milestone_id.is_empty()) {
		return true;
	}

	Dictionary milestone = project_state->get_milestone(p_milestone_id);
	if (milestone.is_empty()) {
		return false;
	}
	return String(milestone.get("status", String())) != "locked";
}

bool AIAgentNextSession::_finish_user_plan_edit(const String &p_action, const String &p_milestone_id, const String &p_task_id, const String &p_message) {
	if (project_state.is_null() || event_log.is_null()) {
		return false;
	}

	if (!p_task_id.is_empty() && !project_state->has_task(p_task_id)) {
		selected_task_id.clear();
	}
	if (!project_state->has_task(selected_task_id)) {
		_sync_selected_task_to_active_milestone();
	}

	const AINextSessionState state = project_state->get_session_state();
	if (state == AI_NEXT_SESSION_IDLE || state == AI_NEXT_SESSION_BRIEFING || state == AI_NEXT_SESSION_PLANNING) {
		project_state->set_session_state(AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL);
	}

	Dictionary edit_metadata;
	edit_metadata["action"] = p_action;
	event_log->record_event("plan_edited", p_milestone_id, p_task_id, "user", p_message, edit_metadata);
	_emit_project_state_changed();
	return true;
}

bool AIAgentNextSession::can_edit_plan() const {
	return project_state.is_valid() && !_is_workflow_active();
}

void AIAgentNextSession::set_model_profile_id(const String &p_model_profile_id) {
	for (const KeyValue<String, Ref<AIAgentBase>> &E : agents) {
		if (E.value.is_valid()) {
			E.value->set_model_profile_id(p_model_profile_id);
		}
	}
}

void AIAgentNextSession::set_agent_model_profile_id(const String &p_agent_id, const String &p_model_profile_id) {
	Ref<AIAgentBase> agent = _get_agent(p_agent_id);
	if (agent.is_valid() && !p_model_profile_id.is_empty()) {
		agent->set_model_profile_id(p_model_profile_id);
	}
}

void AIAgentNextSession::submit_brief(const String &p_brief) {
	project_state->set_brief(p_brief);
	const String title = p_brief.strip_edges();
	if (!title.is_empty() && (workflow_title.is_empty() || workflow_title == "New NEXT Workflow")) {
		workflow_title = title.substr(0, 80);
	}
	project_state->set_session_state(AI_NEXT_SESSION_BRIEFING);
	event_log->record_event("brief_submitted", String(), String(), "user", "NEXT brief submitted.");
	_emit_workflow_session_changed();
	_emit_project_state_changed();
}

void AIAgentNextSession::start_new_workflow() {
	if (_is_workflow_active()) {
		cancel_current_operation();
	}
	_save_current_workflow();
	_start_empty_workflow(true);
}

bool AIAgentNextSession::load_workflow(const String &p_workflow_id) {
	if (p_workflow_id.strip_edges().is_empty() || workflow_store.is_null()) {
		return false;
	}
	if (p_workflow_id == workflow_id) {
		return true;
	}
	if (_is_workflow_active()) {
		cancel_current_operation();
	}
	_save_current_workflow();

	AINextWorkflowSnapshot snapshot;
	if (!workflow_store->load_workflow(p_workflow_id, snapshot)) {
		return false;
	}
	_apply_workflow_snapshot(snapshot, true);
	_save_current_workflow();
	_emit_workflow_session_changed();
	_emit_project_state_changed(false);
	return true;
}

bool AIAgentNextSession::delete_workflow(const String &p_workflow_id) {
	if (p_workflow_id.strip_edges().is_empty() || workflow_store.is_null()) {
		return false;
	}
	if (_is_workflow_active()) {
		cancel_current_operation();
	}

	const bool deleting_current = p_workflow_id == workflow_id;
	if (!workflow_store->delete_workflow(p_workflow_id)) {
		return false;
	}

	if (deleting_current) {
		String latest_workflow_id;
		AINextWorkflowSnapshot snapshot;
		if (workflow_store->get_most_recent_workflow_id(latest_workflow_id) && workflow_store->load_workflow(latest_workflow_id, snapshot)) {
			_apply_workflow_snapshot(snapshot, true);
			_save_current_workflow();
			_emit_workflow_session_changed();
			_emit_project_state_changed(false);
		} else {
			_start_empty_workflow(true);
		}
	}
	return true;
}

bool AIAgentNextSession::can_continue_workflow() const {
	return !_is_workflow_active() && checkpoint.is_resumable();
}

bool AIAgentNextSession::continue_workflow() {
	if (!can_continue_workflow()) {
		return false;
	}

	if (checkpoint.operation == "run_task" || checkpoint.operation == "run_milestone") {
		const String task_id = checkpoint.task_id;
		const String milestone_id = checkpoint.milestone_id.is_empty() && project_state.is_valid() ? project_state->get_task_milestone_id(task_id) : checkpoint.milestone_id;
		Dictionary task = project_state->get_task(task_id);
		const String agent_id = checkpoint.agent_id.is_empty() ? String(task.get("assigned_agent_id", String())) : checkpoint.agent_id;
		if (task_id.is_empty() || milestone_id.is_empty() || task.is_empty() || _get_agent(agent_id).is_null()) {
			checkpoint.status = "failed";
			_save_current_workflow();
			return false;
		}

		workflow_active = true;
		single_task_run = checkpoint.operation == "run_task" ? true : checkpoint.single_task_run;
		workflow_milestone_id = milestone_id;
		selected_task_id = task_id;
		active_task_batch = checkpoint.active_task_batch.duplicate(true);
		active_task_batch_index = checkpoint.active_task_batch_index;
		_clear_runtime_messages();
		project_state->set_active_milestone_id(milestone_id);
		project_state->set_session_state(AI_NEXT_SESSION_EXECUTING);
		project_state->set_task_status(task_id, AI_NEXT_TASK_IN_PROGRESS);
		event_log->record_event("workflow_resumed", milestone_id, task_id, agent_id, "NEXT workflow resumed.");
		_emit_project_state_changed();

		Vector<AIAgentMessage> messages;
		messages.push_back(_make_task_run_message(task));
		if (!_begin_agent_run(PENDING_OPERATION_RUN_TASK, agent_id, messages, milestone_id, task_id, checkpoint.agent_run_id)) {
			project_state->mark_task_failed(task_id, "Failed to resume NEXT agent runtime.");
			event_log->record_event("task_failed", milestone_id, task_id, agent_id, "Failed to resume NEXT agent runtime.");
			_complete_single_task_run(true, milestone_id, task_id);
			return false;
		}
		return true;
	}

	if (checkpoint.operation == "generate_plan") {
		workflow_active = true;
		_clear_runtime_messages();
		project_state->set_session_state(AI_NEXT_SESSION_PLANNING);
		event_log->record_event("workflow_resumed", String(), String(), _planning_agent_id(), "NEXT planning resumed.");
		_emit_project_state_changed();

		AIAgentMessage user_message;
		user_message.role = AI_AGENT_ROLE_USER;
		user_message.created_at = Time::get_singleton()->get_unix_time_from_system();
		user_message.content = "Create a NEXT milestone plan for this brief:\n\n" + project_state->get_brief().strip_edges();
		Vector<AIAgentMessage> messages;
		messages.push_back(user_message);
		if (!_begin_agent_run(PENDING_OPERATION_GENERATE_PLAN, _planning_agent_id(), messages, String(), String(), checkpoint.agent_run_id)) {
			_fail_workflow("planning_failed", String(), String(), _planning_agent_id(), "Failed to resume NEXT planning agent runtime.");
			return false;
		}
		return true;
	}

	if (checkpoint.operation == "review") {
		workflow_active = true;
		workflow_milestone_id = checkpoint.milestone_id;
		_clear_runtime_messages();
		project_state->set_session_state(AI_NEXT_SESSION_FEEDBACK_PLANNING);
		event_log->record_event("workflow_resumed", workflow_milestone_id, String(), _review_agent_id(), "NEXT review resumed.");
		_emit_project_state_changed();

		AIAgentMessage user_message;
		user_message.role = AI_AGENT_ROLE_USER;
		user_message.created_at = Time::get_singleton()->get_unix_time_from_system();
		user_message.content = vformat("Review NEXT milestone `%s`. Inspect project context and pending changes. Return findings only; do not edit files.", workflow_milestone_id);
		Vector<AIAgentMessage> messages;
		messages.push_back(user_message);
		if (!_begin_agent_run(PENDING_OPERATION_REVIEW, _review_agent_id(), messages, workflow_milestone_id, String(), checkpoint.agent_run_id)) {
			_fail_workflow("review_failed", workflow_milestone_id, String(), _review_agent_id(), "Failed to resume NEXT review agent runtime.");
			return false;
		}
		return true;
	}

	if (checkpoint.operation == "feedback_tasks") {
		workflow_active = true;
		workflow_milestone_id = checkpoint.milestone_id;
		pending_feedback_text = checkpoint.feedback_text;
		pending_feedback_previous_task_count = checkpoint.feedback_previous_task_count;
		pending_feedback_attachments = checkpoint.feedback_attachments.duplicate(true);
		_clear_runtime_messages();
		project_state->set_session_state(AI_NEXT_SESSION_FEEDBACK_PLANNING);
		event_log->record_event("workflow_resumed", workflow_milestone_id, String(), _planning_agent_id(), "NEXT feedback planning resumed.");
		_emit_project_state_changed();

		AIAgentMessage user_message;
		user_message.role = AI_AGENT_ROLE_USER;
		user_message.created_at = Time::get_singleton()->get_unix_time_from_system();
		user_message.content = vformat("Turn this playtest feedback into additional NEXT tasks for milestone `%s` by calling ai_next.manage_project with append_tasks:\n\n%s", workflow_milestone_id, pending_feedback_text);
		_set_message_attachments(user_message, pending_feedback_attachments);
		Vector<AIAgentMessage> messages;
		messages.push_back(user_message);
		if (!_begin_agent_run(PENDING_OPERATION_FEEDBACK_TASKS, _planning_agent_id(), messages, workflow_milestone_id, String(), checkpoint.agent_run_id)) {
			_fail_workflow("feedback_planning_failed", workflow_milestone_id, String(), _planning_agent_id(), "Failed to resume NEXT planning agent runtime.");
			return false;
		}
		return true;
	}

	return false;
}

bool AIAgentNextSession::select_milestone(const String &p_milestone_id) {
	if (project_state.is_null() || _is_workflow_active() || !project_state->has_milestone(p_milestone_id)) {
		return false;
	}

	project_state->set_active_milestone_id(p_milestone_id);
	_sync_selected_task_to_active_milestone();
	_set_idle_state_for_active_milestone();
	event_log->record_event("milestone_selected", p_milestone_id, String(), "user", "NEXT milestone selected.");
	_emit_project_state_changed();
	return true;
}

bool AIAgentNextSession::select_task(const String &p_task_id) {
	if (project_state.is_null() || !project_state->has_task(p_task_id)) {
		return false;
	}

	const String task_milestone_id = project_state->get_task_milestone_id(p_task_id);
	if (task_milestone_id.is_empty()) {
		return false;
	}
	if (project_state->get_active_milestone_id() != task_milestone_id && !_is_workflow_active()) {
		project_state->set_active_milestone_id(task_milestone_id);
		_set_idle_state_for_active_milestone();
	}
	selected_task_id = p_task_id;
	_emit_project_state_changed();
	return true;
}

String AIAgentNextSession::create_user_milestone(const String &p_title, const String &p_description) {
	if (!_can_edit_plan_target()) {
		return String();
	}

	const String milestone_id = project_state->create_milestone(p_title.strip_edges(), p_description);
	if (milestone_id.is_empty()) {
		return String();
	}
	_finish_user_plan_edit("create_milestone", milestone_id, String(), "NEXT milestone created by user.");
	return milestone_id;
}

bool AIAgentNextSession::edit_user_milestone(const String &p_milestone_id, const String &p_title, const String &p_description) {
	if (!_can_edit_plan_target(p_milestone_id)) {
		return false;
	}

	Dictionary patch;
	patch["title"] = p_title.strip_edges();
	patch["description"] = p_description;
	String error;
	if (!project_state->update_milestone(p_milestone_id, patch, error)) {
		event_log->record_event("plan_edit_failed", p_milestone_id, String(), "user", error);
		return false;
	}
	return _finish_user_plan_edit("edit_milestone", p_milestone_id, String(), "NEXT milestone edited by user.");
}

bool AIAgentNextSession::delete_user_milestone(const String &p_milestone_id) {
	if (!_can_edit_plan_target(p_milestone_id)) {
		return false;
	}

	String error;
	if (!project_state->delete_milestone(p_milestone_id, error)) {
		event_log->record_event("plan_edit_failed", p_milestone_id, String(), "user", error);
		return false;
	}
	return _finish_user_plan_edit("delete_milestone", p_milestone_id, String(), "NEXT milestone deleted by user.");
}

bool AIAgentNextSession::move_user_milestone(const String &p_milestone_id, int p_to_index) {
	if (!_can_edit_plan_target(p_milestone_id)) {
		return false;
	}

	String error;
	if (!project_state->move_milestone(p_milestone_id, p_to_index, error)) {
		event_log->record_event("plan_edit_failed", p_milestone_id, String(), "user", error);
		return false;
	}
	return _finish_user_plan_edit("move_milestone", p_milestone_id, String(), "NEXT milestone reordered by user.");
}

bool AIAgentNextSession::merge_user_milestones(const String &p_target_milestone_id, const String &p_source_milestone_id) {
	if (!_can_edit_plan_target(p_target_milestone_id) || !_can_edit_plan_target(p_source_milestone_id)) {
		return false;
	}

	String error;
	if (!project_state->merge_milestones(p_target_milestone_id, p_source_milestone_id, error)) {
		event_log->record_event("plan_edit_failed", p_source_milestone_id, String(), "user", error);
		return false;
	}
	return _finish_user_plan_edit("merge_milestones", p_target_milestone_id, String(), "NEXT milestones merged by user.");
}

String AIAgentNextSession::create_user_task(const String &p_milestone_id, const String &p_title, const String &p_assigned_agent_id, const Array &p_depends_on, const String &p_description) {
	return create_user_task(p_milestone_id, p_title, p_assigned_agent_id, p_depends_on, p_description, Array());
}

String AIAgentNextSession::create_user_task(const String &p_milestone_id, const String &p_title, const String &p_assigned_agent_id, const Array &p_depends_on, const String &p_description, const Array &p_attachments) {
	if (!_can_edit_plan_target(p_milestone_id)) {
		return String();
	}

	const String task_id = project_state->add_task(p_milestone_id, p_title.strip_edges(), p_assigned_agent_id.strip_edges(), p_depends_on, p_description);
	if (task_id.is_empty()) {
		event_log->record_event("plan_edit_failed", p_milestone_id, String(), "user", project_state->get_last_error());
		return String();
	}
	if (!p_attachments.is_empty()) {
		Dictionary patch;
		patch["attachments"] = p_attachments.duplicate(true);
		String error;
		if (!project_state->update_task(task_id, patch, error)) {
			event_log->record_event("plan_edit_failed", p_milestone_id, task_id, "user", error);
			return String();
		}
	}
	selected_task_id = task_id;
	_finish_user_plan_edit("create_task", p_milestone_id, task_id, "NEXT task created by user.");
	return task_id;
}

bool AIAgentNextSession::edit_user_task(const String &p_task_id, const String &p_title, const String &p_description, const String &p_assigned_agent_id) {
	if (project_state.is_null()) {
		return false;
	}
	Dictionary task = project_state->get_task(p_task_id);
	return edit_user_task(p_task_id, p_title, p_description, p_assigned_agent_id, task.get("attachments", Array()));
}

bool AIAgentNextSession::edit_user_task(const String &p_task_id, const String &p_title, const String &p_description, const String &p_assigned_agent_id, const Array &p_attachments) {
	if (project_state.is_null() || !_can_edit_plan_target(project_state->get_task_milestone_id(p_task_id))) {
		return false;
	}

	Dictionary patch;
	patch["title"] = p_title.strip_edges();
	patch["description"] = p_description;
	patch["assigned_agent_id"] = p_assigned_agent_id.strip_edges();
	patch["attachments"] = p_attachments.duplicate(true);
	String error;
	if (!project_state->update_task(p_task_id, patch, error)) {
		event_log->record_event("plan_edit_failed", project_state->get_task_milestone_id(p_task_id), p_task_id, "user", error);
		return false;
	}
	selected_task_id = p_task_id;
	return _finish_user_plan_edit("edit_task", project_state->get_task_milestone_id(p_task_id), p_task_id, "NEXT task edited by user.");
}

bool AIAgentNextSession::delete_user_task(const String &p_task_id) {
	if (project_state.is_null() || !_can_edit_plan_target(project_state->get_task_milestone_id(p_task_id))) {
		return false;
	}

	const String milestone_id = project_state->get_task_milestone_id(p_task_id);
	String error;
	if (!project_state->delete_task(p_task_id, error)) {
		event_log->record_event("plan_edit_failed", milestone_id, p_task_id, "user", error);
		return false;
	}
	if (selected_task_id == p_task_id) {
		selected_task_id.clear();
	}
	return _finish_user_plan_edit("delete_task", milestone_id, p_task_id, "NEXT task deleted by user.");
}

bool AIAgentNextSession::move_user_task(const String &p_task_id, const String &p_target_milestone_id, int p_to_index) {
	if (project_state.is_null()) {
		return false;
	}
	const String source_milestone_id = project_state->get_task_milestone_id(p_task_id);
	if (!_can_edit_plan_target(source_milestone_id) || !_can_edit_plan_target(p_target_milestone_id)) {
		return false;
	}

	String error;
	if (!project_state->move_task(p_task_id, p_target_milestone_id, p_to_index, error)) {
		event_log->record_event("plan_edit_failed", source_milestone_id, p_task_id, "user", error);
		return false;
	}
	selected_task_id = p_task_id;
	return _finish_user_plan_edit("move_task", p_target_milestone_id, p_task_id, "NEXT task moved by user.");
}

bool AIAgentNextSession::set_user_task_dependencies(const String &p_task_id, const Array &p_depends_on) {
	if (project_state.is_null() || !_can_edit_plan_target(project_state->get_task_milestone_id(p_task_id))) {
		return false;
	}

	const String milestone_id = project_state->get_task_milestone_id(p_task_id);
	String error;
	if (!project_state->set_task_dependencies(p_task_id, p_depends_on, error)) {
		event_log->record_event("plan_edit_failed", milestone_id, p_task_id, "user", error);
		return false;
	}
	selected_task_id = p_task_id;
	return _finish_user_plan_edit("edit_task_dependencies", milestone_id, p_task_id, "NEXT task dependencies edited by user.");
}

void AIAgentNextSession::generate_plan() {
	if (_is_workflow_active()) {
		event_log->record_event("operation_ignored", project_state->get_active_milestone_id(), String(), "next_session", "NEXT operation is already running.");
		return;
	}

	workflow_active = true;
	_clear_runtime_messages();
	project_state->set_session_state(AI_NEXT_SESSION_PLANNING);
	event_log->record_event("planning_started", String(), String(), _planning_agent_id(), "NEXT planning started.");
	_emit_project_state_changed();

	const String brief = project_state->get_brief().strip_edges();
	if (brief.is_empty()) {
		_fail_workflow("planning_failed", String(), String(), _planning_agent_id(), "NEXT brief is empty.");
		return;
	}

	AIAgentMessage user_message;
	user_message.role = AI_AGENT_ROLE_USER;
	user_message.created_at = Time::get_singleton()->get_unix_time_from_system();
	user_message.content = "Create a NEXT milestone plan for this brief:\n\n" + brief;

	Vector<AIAgentMessage> messages;
	messages.push_back(user_message);
	if (!_begin_agent_run(PENDING_OPERATION_GENERATE_PLAN, _planning_agent_id(), messages)) {
		_fail_workflow("planning_failed", String(), String(), _planning_agent_id(), "Failed to start NEXT planning agent runtime.");
	}
}

void AIAgentNextSession::approve_plan() {
	project_state->set_session_state(AI_NEXT_SESSION_EXECUTING);
	event_log->record_event("plan_approved", project_state->get_active_milestone_id(), String(), "user", "NEXT plan approved.");
	_emit_project_state_changed();
}

void AIAgentNextSession::run_active_milestone() {
	if (_is_workflow_active()) {
		event_log->record_event("operation_ignored", project_state->get_active_milestone_id(), String(), "next_session", "NEXT operation is already running.");
		return;
	}

	workflow_active = true;
	milestone_run_guard = 0;
	active_task_batch = Array();
	active_task_batch_index = 0;
	single_task_run = false;
	workflow_milestone_id = project_state->get_active_milestone_id();
	_clear_runtime_messages();
	project_state->set_session_state(AI_NEXT_SESSION_EXECUTING);
	event_log->record_event("milestone_run_started", project_state->get_active_milestone_id(), String(), "next_session", "NEXT active milestone run requested.");
	_emit_project_state_changed();

	const String milestone_id = workflow_milestone_id;
	if (milestone_id.is_empty()) {
		_fail_workflow("milestone_run_failed", String(), String(), "next_session", "No active NEXT milestone.");
		return;
	}

	_continue_active_milestone_run();
}

bool AIAgentNextSession::run_task(const String &p_task_id) {
	if (_is_workflow_active()) {
		event_log->record_event("operation_ignored", project_state->get_active_milestone_id(), p_task_id, "next_session", "NEXT operation is already running.");
		return false;
	}
	if (!can_run_task(p_task_id)) {
		event_log->record_event("task_run_ignored", project_state->get_active_milestone_id(), p_task_id, "next_session", "NEXT task is not ready to run.");
		return false;
	}

	const String milestone_id = project_state->get_task_milestone_id(p_task_id);
	Dictionary task = project_state->get_task(p_task_id);
	const String agent_id = String(task.get("assigned_agent_id", String()));
	Ref<AIAgentBase> agent = _get_agent(agent_id);
	if (milestone_id.is_empty() || agent.is_null()) {
		event_log->record_event("task_run_ignored", milestone_id, p_task_id, agent_id, "NEXT task agent is not configured.");
		return false;
	}

	workflow_active = true;
	single_task_run = true;
	workflow_milestone_id = milestone_id;
	active_task_batch = Array();
	active_task_batch_index = 0;
	selected_task_id = p_task_id;
	_clear_runtime_messages();
	project_state->set_active_milestone_id(milestone_id);
	project_state->set_session_state(AI_NEXT_SESSION_EXECUTING);
	project_state->set_task_status(p_task_id, AI_NEXT_TASK_IN_PROGRESS);
	event_log->record_event("task_started", milestone_id, p_task_id, agent_id, String(task.get("title", String())));
	_emit_project_state_changed();

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_task_run_message(task));
	if (!_begin_agent_run(PENDING_OPERATION_RUN_TASK, agent_id, messages, milestone_id, p_task_id)) {
		project_state->mark_task_failed(p_task_id, "Failed to start NEXT agent runtime.");
		event_log->record_event("task_failed", milestone_id, p_task_id, agent_id, "Failed to start NEXT agent runtime.");
		_complete_single_task_run(true, milestone_id, p_task_id);
		return false;
	}
	return true;
}

bool AIAgentNextSession::send_task_session_message(const String &p_task_id, const String &p_message) {
	return send_task_session_message(p_task_id, p_message, Array());
}

bool AIAgentNextSession::send_task_session_message(const String &p_task_id, const String &p_message, const Array &p_attachments) {
	const String message_text = p_message.strip_edges();
	if (message_text.is_empty()) {
		return false;
	}
	if (_is_workflow_active()) {
		event_log->record_event("operation_ignored", project_state->get_active_milestone_id(), p_task_id, "next_session", "NEXT operation is already running.");
		return false;
	}
	if (!can_continue_task_session(p_task_id)) {
		event_log->record_event("task_session_ignored", project_state.is_valid() ? project_state->get_active_milestone_id() : String(), p_task_id, "next_session", "NEXT task session cannot continue.");
		return false;
	}

	const String milestone_id = project_state->get_task_milestone_id(p_task_id);
	Dictionary task = project_state->get_task(p_task_id);
	const String agent_id = String(task.get("assigned_agent_id", String()));
	AINextAgentRunState *run_state = _find_agent_run(String(task.get("run_id", String())));
	if (!run_state) {
		run_state = _find_latest_task_agent_run(p_task_id);
	}
	if (!run_state) {
		event_log->record_event("task_session_ignored", milestone_id, p_task_id, "next_session", "NEXT task session has no previous run to continue.");
		return false;
	}

	Vector<AIAgentMessage> messages;
	String existing_run_id;
	existing_run_id = run_state->run_id;
	messages = run_state->messages;

	AIAgentMessage user_message;
	user_message.role = AI_AGENT_ROLE_USER;
	user_message.created_at = Time::get_singleton()->get_unix_time_from_system();
	user_message.content = p_message;
	_set_message_attachments(user_message, p_attachments);
	messages.push_back(user_message);

	if (run_state) {
		run_state->messages = messages;
		run_state->updated_at = Time::get_singleton()->get_unix_time_from_system();
	}

	workflow_active = true;
	single_task_run = true;
	workflow_milestone_id = milestone_id;
	active_task_batch = Array();
	active_task_batch_index = 0;
	selected_task_id = p_task_id;
	_clear_runtime_messages();
	project_state->set_active_milestone_id(milestone_id);
	project_state->set_session_state(AI_NEXT_SESSION_EXECUTING);
	project_state->set_task_status(p_task_id, AI_NEXT_TASK_IN_PROGRESS);
	event_log->record_event("task_session_message_sent", milestone_id, p_task_id, "user", message_text);
	event_log->record_event("task_started", milestone_id, p_task_id, agent_id, String(task.get("title", String())));
	_emit_project_state_changed();

	if (!_begin_agent_run(PENDING_OPERATION_RUN_TASK, agent_id, messages, milestone_id, p_task_id, existing_run_id)) {
		project_state->mark_task_failed(p_task_id, "Failed to continue NEXT task session.");
		event_log->record_event("task_failed", milestone_id, p_task_id, agent_id, "Failed to continue NEXT task session.");
		_complete_single_task_run(true, milestone_id, p_task_id);
		return false;
	}
	return true;
}

void AIAgentNextSession::review_active_milestone() {
	if (_is_workflow_active()) {
		event_log->record_event("operation_ignored", project_state->get_active_milestone_id(), String(), "next_session", "NEXT operation is already running.");
		return;
	}

	workflow_active = true;
	workflow_milestone_id = project_state->get_active_milestone_id();
	_clear_runtime_messages();
	project_state->set_session_state(AI_NEXT_SESSION_FEEDBACK_PLANNING);
	const String milestone_id = workflow_milestone_id;
	event_log->record_event("review_started", milestone_id, String(), _review_agent_id(), "NEXT review started.");
	_emit_project_state_changed();

	if (milestone_id.is_empty()) {
		_fail_workflow("review_failed", String(), String(), _review_agent_id(), "No active NEXT milestone.");
		return;
	}

	AIAgentMessage user_message;
	user_message.role = AI_AGENT_ROLE_USER;
	user_message.created_at = Time::get_singleton()->get_unix_time_from_system();
	user_message.content = vformat("Review NEXT milestone `%s`. Inspect project context and pending changes. Return findings only; do not edit files.", milestone_id);

	Vector<AIAgentMessage> messages;
	messages.push_back(user_message);
	if (!_begin_agent_run(PENDING_OPERATION_REVIEW, _review_agent_id(), messages, milestone_id)) {
		_fail_workflow("review_failed", milestone_id, String(), _review_agent_id(), "Failed to start NEXT review agent runtime.");
	}
}

void AIAgentNextSession::generate_feedback_tasks(const String &p_feedback) {
	generate_feedback_tasks(p_feedback, Array());
}

void AIAgentNextSession::generate_feedback_tasks(const String &p_feedback, const Array &p_attachments) {
	if (_is_workflow_active()) {
		event_log->record_event("operation_ignored", project_state->get_active_milestone_id(), String(), "next_session", "NEXT operation is already running.");
		return;
	}

	workflow_active = true;
	workflow_milestone_id = project_state->get_active_milestone_id();
	_clear_runtime_messages();
	project_state->set_session_state(AI_NEXT_SESSION_FEEDBACK_PLANNING);
	Dictionary feedback_metadata;
	feedback_metadata["feedback"] = p_feedback;
	event_log->record_event("feedback_submitted", project_state->get_active_milestone_id(), String(), "user", "NEXT playtest feedback submitted.", feedback_metadata);
	_emit_project_state_changed();

	const String milestone_id = workflow_milestone_id;
	const String feedback = p_feedback.strip_edges();
	if (milestone_id.is_empty() || feedback.is_empty()) {
		_fail_workflow("feedback_planning_failed", milestone_id, String(), _planning_agent_id(), "NEXT feedback or active milestone is empty.");
		return;
	}

	const int previous_task_count = project_state->get_task_count(milestone_id);
	pending_feedback_previous_task_count = previous_task_count;
	pending_feedback_text = feedback;
	pending_feedback_attachments = p_attachments.duplicate(true);
	AIAgentMessage user_message;
	user_message.role = AI_AGENT_ROLE_USER;
	user_message.created_at = Time::get_singleton()->get_unix_time_from_system();
	user_message.content = vformat("Turn this playtest feedback into additional NEXT tasks for milestone `%s` by calling ai_next.manage_project with append_tasks:\n\n%s", milestone_id, feedback);
	_set_message_attachments(user_message, pending_feedback_attachments);

	Vector<AIAgentMessage> messages;
	messages.push_back(user_message);
	if (!_begin_agent_run(PENDING_OPERATION_FEEDBACK_TASKS, _planning_agent_id(), messages, milestone_id)) {
		_fail_workflow("feedback_planning_failed", milestone_id, String(), _planning_agent_id(), "Failed to start NEXT planning agent runtime.");
	}
}

void AIAgentNextSession::accept_and_lock_active_milestone() {
	String error;
	const String milestone_id = project_state->get_active_milestone_id();
	if (!project_state->lock_milestone(milestone_id, error)) {
		project_state->set_session_state(AI_NEXT_SESSION_FAILED);
		event_log->record_event("milestone_lock_failed", milestone_id, String(), "next_session", error);
	} else {
		const String next_milestone_id = _find_next_unlocked_milestone_id(milestone_id);
		if (!next_milestone_id.is_empty()) {
			project_state->set_active_milestone_id(next_milestone_id);
			_sync_selected_task_to_active_milestone();
			_set_idle_state_for_active_milestone();
		} else {
			project_state->set_session_state(AI_NEXT_SESSION_READY_TO_LOCK);
		}
		event_log->record_event("milestone_locked", milestone_id, String(), "user", "NEXT milestone locked.");
	}
	_emit_project_state_changed();
}

void AIAgentNextSession::cancel_current_operation() {
	if (!_is_workflow_active() && !checkpoint.is_resumable()) {
		return;
	}

	if (checkpoint.operation.is_empty() || checkpoint.operation == "none") {
		checkpoint.operation = _get_checkpoint_operation_name(pending_operation);
		checkpoint.agent_id = pending_agent_id;
		checkpoint.milestone_id = pending_milestone_id;
		checkpoint.task_id = pending_task_id;
		checkpoint.single_task_run = single_task_run;
		checkpoint.selected_task_id = selected_task_id;
		checkpoint.active_task_batch = active_task_batch.duplicate(true);
		checkpoint.active_task_batch_index = active_task_batch_index;
		checkpoint.feedback_text = pending_feedback_text;
		checkpoint.feedback_previous_task_count = pending_feedback_previous_task_count;
		checkpoint.feedback_attachments = pending_feedback_attachments.duplicate(true);
	}
	checkpoint.status = "user_terminated";
	if (checkpoint.agent_run_id.is_empty()) {
		checkpoint.agent_run_id = active_agent_run_id;
	}
	if (checkpoint.workflow_run_id.is_empty()) {
		checkpoint.workflow_run_id = active_workflow_run_id;
	}

	if (!checkpoint.task_id.is_empty()) {
		project_state->reset_interrupted_task(checkpoint.task_id);
	}
	AINextAgentRunState *run_state = _find_agent_run(checkpoint.agent_run_id);
	if (run_state) {
		run_state->status = "user_terminated";
		run_state->updated_at = Time::get_singleton()->get_unix_time_from_system();
	}

	workflow_active = false;
	_clear_pending_agent_run();
	workflow_milestone_id.clear();
	single_task_run = false;
	milestone_run_guard = 0;
	active_task_batch = Array();
	active_task_batch_index = 0;
	_clear_runtime_messages();
	project_state->set_session_state(AI_NEXT_SESSION_IDLE);
	event_log->record_event("operation_cancelled", checkpoint.milestone_id, checkpoint.task_id, "user", "NEXT operation cancelled.");
	_emit_project_state_changed();
}

void AIAgentNextSession::set_workflow_project_scope_for_test(const String &p_project_scope_key) {
	if (_is_workflow_active()) {
		cancel_current_operation();
	}
	if (workflow_store.is_null()) {
		workflow_store.instantiate();
	}
	workflow_store->set_project_scope(p_project_scope_key);
	_load_initial_workflow();
}

Ref<AINextWorkflowStore> AIAgentNextSession::get_workflow_store_for_test() const {
	return workflow_store;
}

Dictionary AIAgentNextSession::get_workflow_checkpoint_for_test() const {
	return checkpoint.to_dict();
}

Error AIAgentNextSession::save_workflow_for_test() {
	if (workflow_store.is_null() || workflow_id.is_empty()) {
		return ERR_UNCONFIGURED;
	}
	workflow_updated_at = Time::get_singleton()->get_unix_time_from_system();
	return workflow_store->save_workflow(_build_workflow_snapshot());
}
