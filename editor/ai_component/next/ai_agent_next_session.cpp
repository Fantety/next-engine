/**************************************************************************/
/*  ai_agent_next_session.cpp                                             */
/**************************************************************************/

#include "ai_agent_next_session.h"

#include "core/object/class_db.h"
#include "editor/ai_component/agent/ai_agent_message.h"
#include "editor/ai_component/next/ai_next_manage_project_tool.h"
#include "editor/ai_component/next/ai_next_prompts.h"
#include "editor/ai_component/tools/editor/ai_get_editor_context_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_add_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_create_scene_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_delete_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_instantiate_scene_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_list_properties_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_move_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_open_scene_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_rename_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_save_current_scene_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_set_property_tool.h"
#include "editor/ai_component/tools/editor/ai_script_bind_to_node_tool.h"
#include "editor/ai_component/tools/editor/ai_script_create_tool.h"
#include "editor/ai_component/tools/editor/ai_script_delete_tool.h"
#include "editor/ai_component/tools/editor/ai_script_inspect_tool.h"
#include "editor/ai_component/tools/editor/ai_script_patch_function_tool.h"
#include "editor/ai_component/tools/editor/ai_script_unbind_from_node_tool.h"
#include "editor/ai_component/tools/editor/ai_script_write_tool.h"
#include "editor/ai_component/tools/editor/ai_shader_apply_to_node_tool.h"
#include "editor/ai_component/tools/editor/ai_shader_create_tool.h"
#include "editor/ai_component/tools/editor/ai_shader_delete_tool.h"
#include "editor/ai_component/tools/editor/ai_shader_edit_tool.h"
#include "editor/ai_component/tools/project/ai_create_folder_tool.h"
#include "editor/ai_component/tools/project/ai_list_project_tool.h"
#include "editor/ai_component/tools/project/ai_read_file_tool.h"
#include "editor/ai_component/tools/project/ai_search_project_tool.h"

namespace {

template <typename T>
Ref<AITool> _make_tool() {
	Ref<T> tool;
	tool.instantiate();
	return tool;
}

void _push_tool(Vector<Ref<AITool>> &r_tools, const Ref<AITool> &p_tool) {
	if (p_tool.is_valid()) {
		r_tools.push_back(p_tool);
	}
}

String _get_last_response_summary(const AIAgentRuntimeResult &p_result) {
	for (int i = p_result.messages.size() - 1; i >= 0; i--) {
		if (p_result.messages[i].role == AI_AGENT_ROLE_ASSISTANT && !p_result.messages[i].content.strip_edges().is_empty()) {
			return p_result.messages[i].content.strip_edges();
		}
	}
	return "Task completed.";
}

} // namespace

void AIAgentNextSession::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_project_state"), &AIAgentNextSession::get_project_state);
	ClassDB::bind_method(D_METHOD("get_project_store"), &AIAgentNextSession::get_project_store);
	ClassDB::bind_method(D_METHOD("get_event_log"), &AIAgentNextSession::get_event_log);
	ClassDB::bind_method(D_METHOD("has_agent", "agent_id"), &AIAgentNextSession::has_agent);
	ClassDB::bind_method(D_METHOD("set_model_profile_id", "model_profile_id"), &AIAgentNextSession::set_model_profile_id);
	ClassDB::bind_method(D_METHOD("submit_brief", "brief"), &AIAgentNextSession::submit_brief);
	ClassDB::bind_method(D_METHOD("generate_plan"), &AIAgentNextSession::generate_plan);
	ClassDB::bind_method(D_METHOD("approve_plan"), &AIAgentNextSession::approve_plan);
	ClassDB::bind_method(D_METHOD("run_active_milestone"), &AIAgentNextSession::run_active_milestone);
	ClassDB::bind_method(D_METHOD("generate_feedback_tasks", "feedback"), &AIAgentNextSession::generate_feedback_tasks);
	ClassDB::bind_method(D_METHOD("accept_and_lock_active_milestone"), &AIAgentNextSession::accept_and_lock_active_milestone);
	ClassDB::bind_method(D_METHOD("cancel_current_operation"), &AIAgentNextSession::cancel_current_operation);

	ADD_SIGNAL(MethodInfo("state_changed", PropertyInfo(Variant::STRING, "state")));
	ADD_SIGNAL(MethodInfo("project_state_changed"));
}

AIAgentNextSession::AIAgentNextSession() {
	project_state.instantiate();
	project_store.instantiate();
	event_log.instantiate();

	planning_agent.instantiate();
	script_agent.instantiate();
	scene_agent.instantiate();
	shader_agent.instantiate();
	review_agent.instantiate();

	Vector<Ref<AITool>> planning_tools;
	_register_next_tools(planning_agent);
	_register_shared_read_tools(planning_agent);
	_configure_agent(planning_agent, "planning_agent", AINextPrompts::get_planning_prompt(), planning_tools);

	Vector<Ref<AITool>> script_tools;
	_register_shared_read_tools(script_agent);
	_register_specialist_write_tools(script_agent, "script_agent");
	_configure_agent(script_agent, "script_agent", AINextPrompts::get_script_prompt(), script_tools);

	Vector<Ref<AITool>> scene_tools;
	_register_shared_read_tools(scene_agent);
	_register_specialist_write_tools(scene_agent, "scene_agent");
	_configure_agent(scene_agent, "scene_agent", AINextPrompts::get_scene_prompt(), scene_tools);

	Vector<Ref<AITool>> shader_tools;
	_register_shared_read_tools(shader_agent);
	_register_specialist_write_tools(shader_agent, "shader_agent");
	_configure_agent(shader_agent, "shader_agent", AINextPrompts::get_shader_prompt(), shader_tools);

	Vector<Ref<AITool>> review_tools;
	_register_shared_read_tools(review_agent);
	_configure_agent(review_agent, "review_agent", AINextPrompts::get_review_prompt(), review_tools);
	review_agent->set_agent_profile_id("review");
}

void AIAgentNextSession::_configure_agent(const Ref<AIAgentBase> &p_agent, const String &p_agent_id, const String &p_prompt, const Vector<Ref<AITool>> &p_tools) {
	ERR_FAIL_COND(p_agent.is_null());
	p_agent->set_session_id("next:" + p_agent_id);
	p_agent->set_system_prompt(p_prompt);
	if (p_agent_id == "review_agent") {
		p_agent->set_agent_profile_id("review");
	} else if (p_agent_id == "planning_agent") {
		p_agent->set_agent_profile_id("plan");
	} else {
		p_agent->set_agent_profile_id("write");
	}
	for (const Ref<AITool> &tool : p_tools) {
		p_agent->add_tool(tool, AI_TOOL_PERMISSION_ALLOW);
	}
}

void AIAgentNextSession::_register_next_tools(const Ref<AIAgentBase> &p_agent) {
	ERR_FAIL_COND(p_agent.is_null());
	Ref<AINextManageProjectTool> manage_project_tool;
	manage_project_tool.instantiate();
	manage_project_tool->set_project_state(project_state);
	p_agent->add_tool(manage_project_tool, AI_TOOL_PERMISSION_ALLOW);
}

void AIAgentNextSession::_register_shared_read_tools(const Ref<AIAgentBase> &p_agent) {
	ERR_FAIL_COND(p_agent.is_null());
	p_agent->add_tool(_make_tool<AIListProjectTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AIReadFileTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AISearchProjectTool>(), AI_TOOL_PERMISSION_ALLOW);
	p_agent->add_tool(_make_tool<AIGetEditorContextTool>(), AI_TOOL_PERMISSION_ALLOW);
}

void AIAgentNextSession::_register_specialist_write_tools(const Ref<AIAgentBase> &p_agent, const String &p_agent_id) {
	ERR_FAIL_COND(p_agent.is_null());
	if (p_agent_id == "script_agent") {
		p_agent->add_tool(_make_tool<AICreateFolderTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AIScriptInspectTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AIScriptCreateTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AIScriptWriteTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AIScriptPatchFunctionTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AIScriptBindToNodeTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AIScriptUnbindFromNodeTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AIScriptDeleteTool>(), AI_TOOL_PERMISSION_ASK);
		return;
	}
	if (p_agent_id == "scene_agent") {
		p_agent->add_tool(_make_tool<AISceneListPropertiesTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AISceneCreateSceneTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AISceneAddNodeTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AISceneInstantiateSceneTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AISceneRenameNodeTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AISceneMoveNodeTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AISceneSetPropertyTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AISceneSaveCurrentSceneTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AISceneOpenSceneTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AISceneDeleteNodeTool>(), AI_TOOL_PERMISSION_ASK);
		return;
	}
	if (p_agent_id == "shader_agent") {
		p_agent->add_tool(_make_tool<AICreateFolderTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AIShaderCreateTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AIShaderEditTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AIShaderApplyToNodeTool>(), AI_TOOL_PERMISSION_ALLOW);
		p_agent->add_tool(_make_tool<AIShaderDeleteTool>(), AI_TOOL_PERMISSION_ASK);
	}
}

Ref<AIAgentBase> AIAgentNextSession::_get_agent(const String &p_agent_id) const {
	if (p_agent_id == "planning_agent") {
		return planning_agent;
	}
	if (p_agent_id == "script_agent") {
		return script_agent;
	}
	if (p_agent_id == "scene_agent") {
		return scene_agent;
	}
	if (p_agent_id == "shader_agent") {
		return shader_agent;
	}
	if (p_agent_id == "review_agent") {
		return review_agent;
	}
	return Ref<AIAgentBase>();
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

bool AIAgentNextSession::has_agent(const String &p_agent_id) const {
	return _get_agent(p_agent_id).is_valid();
}

Ref<AIAgentBase> AIAgentNextSession::get_agent_for_test(const String &p_agent_id) const {
	return _get_agent(p_agent_id);
}

void AIAgentNextSession::set_model_profile_id(const String &p_model_profile_id) {
	planning_agent->set_model_profile_id(p_model_profile_id);
	script_agent->set_model_profile_id(p_model_profile_id);
	scene_agent->set_model_profile_id(p_model_profile_id);
	shader_agent->set_model_profile_id(p_model_profile_id);
	review_agent->set_model_profile_id(p_model_profile_id);
}

void AIAgentNextSession::submit_brief(const String &p_brief) {
	project_state->set_brief(p_brief);
	project_state->set_session_state(AI_NEXT_SESSION_BRIEFING);
	event_log->record_event("brief_submitted", String(), String(), "user", "NEXT brief submitted.");
	emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
	emit_signal(SNAME("project_state_changed"));
}

void AIAgentNextSession::generate_plan() {
	project_state->set_session_state(AI_NEXT_SESSION_PLANNING);
	event_log->record_event("planning_started", String(), String(), "planning_agent", "NEXT planning started.");
	emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
	emit_signal(SNAME("project_state_changed"));

	const String brief = project_state->get_brief().strip_edges();
	if (brief.is_empty()) {
		project_state->set_session_state(AI_NEXT_SESSION_FAILED);
		event_log->record_event("planning_failed", String(), String(), "planning_agent", "NEXT brief is empty.");
		emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
		emit_signal(SNAME("project_state_changed"));
		return;
	}

	AIAgentMessage user_message;
	user_message.role = AI_AGENT_ROLE_USER;
	user_message.content = "Create a NEXT milestone plan for this brief:\n\n" + brief;

	Vector<AIAgentMessage> messages;
	messages.push_back(user_message);
	AIAgentRuntimeResult result = planning_agent->run(messages);
	if (!result.success) {
		project_state->set_session_state(AI_NEXT_SESSION_FAILED);
		event_log->record_event("planning_failed", String(), String(), "planning_agent", result.error);
		emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
		emit_signal(SNAME("project_state_changed"));
		return;
	}

	if (project_state->get_milestone_count() <= 0) {
		project_state->set_session_state(AI_NEXT_SESSION_FAILED);
		event_log->record_event("planning_failed", String(), String(), "planning_agent", "Planning completed without writing NEXT milestones.");
		emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
		emit_signal(SNAME("project_state_changed"));
		return;
	}

	project_state->set_session_state(AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL);
	event_log->record_event("planning_completed", project_state->get_active_milestone_id(), String(), "planning_agent", "NEXT planning completed.");
	emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
	emit_signal(SNAME("project_state_changed"));
}

void AIAgentNextSession::approve_plan() {
	project_state->set_session_state(AI_NEXT_SESSION_EXECUTING);
	event_log->record_event("plan_approved", project_state->get_active_milestone_id(), String(), "user", "NEXT plan approved.");
	emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
	emit_signal(SNAME("project_state_changed"));
}

void AIAgentNextSession::run_active_milestone() {
	project_state->set_session_state(AI_NEXT_SESSION_EXECUTING);
	event_log->record_event("milestone_run_started", project_state->get_active_milestone_id(), String(), "next_session", "NEXT active milestone run requested.");
	emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
	emit_signal(SNAME("project_state_changed"));

	const String milestone_id = project_state->get_active_milestone_id();
	if (milestone_id.is_empty()) {
		project_state->set_session_state(AI_NEXT_SESSION_FAILED);
		event_log->record_event("milestone_run_failed", String(), String(), "next_session", "No active NEXT milestone.");
		emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
		emit_signal(SNAME("project_state_changed"));
		return;
	}

	bool failed = false;
	int guard = 0;
	while (!failed && guard++ < 100) {
		Array ready_tasks = project_state->get_ready_tasks(milestone_id);
		if (ready_tasks.is_empty()) {
			break;
		}

		for (int i = 0; i < ready_tasks.size(); i++) {
			if (Variant(ready_tasks[i]).get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary task = ready_tasks[i];
			const String task_id = String(task.get("id", String()));
			const String agent_id = String(task.get("assigned_agent_id", String()));
			Ref<AIAgentBase> agent = _get_agent(agent_id);
			if (agent.is_null()) {
				project_state->mark_task_failed(task_id, "Unknown NEXT agent: " + agent_id);
				event_log->record_event("task_failed", milestone_id, task_id, agent_id, "Unknown NEXT agent.");
				failed = true;
				break;
			}

			project_state->set_task_status(task_id, AI_NEXT_TASK_IN_PROGRESS);
			event_log->record_event("task_started", milestone_id, task_id, agent_id, String(task.get("title", String())));

			AIAgentMessage user_message;
			user_message.role = AI_AGENT_ROLE_USER;
			user_message.content = vformat("Run NEXT task `%s`.\n\nDescription:\n%s\n\nReturn a concise summary and list produced paths if any.",
					String(task.get("title", String())),
					String(task.get("description", String())));

			Vector<AIAgentMessage> messages;
			messages.push_back(user_message);
			AIAgentRuntimeResult result = agent->run(messages);
			if (!result.success) {
				project_state->mark_task_failed(task_id, result.error);
				event_log->record_event("task_failed", milestone_id, task_id, agent_id, result.error);
				failed = true;
				break;
			}

			project_state->mark_task_completed(task_id, _get_last_response_summary(result), Array());
			event_log->record_event("task_completed", milestone_id, task_id, agent_id, _get_last_response_summary(result));
		}
	}

	project_state->set_session_state(failed ? AI_NEXT_SESSION_FAILED : AI_NEXT_SESSION_WAITING_PLAYTEST);
	event_log->record_event(failed ? "milestone_run_failed" : "milestone_run_completed", milestone_id, String(), "next_session", failed ? "NEXT milestone run failed." : "NEXT milestone run completed.");
	emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
	emit_signal(SNAME("project_state_changed"));
}

void AIAgentNextSession::generate_feedback_tasks(const String &p_feedback) {
	project_state->set_session_state(AI_NEXT_SESSION_FEEDBACK_PLANNING);
	Dictionary feedback_metadata;
	feedback_metadata["feedback"] = p_feedback;
	event_log->record_event("feedback_submitted", project_state->get_active_milestone_id(), String(), "user", "NEXT playtest feedback submitted.", feedback_metadata);
	emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
	emit_signal(SNAME("project_state_changed"));

	const String milestone_id = project_state->get_active_milestone_id();
	const String feedback = p_feedback.strip_edges();
	if (milestone_id.is_empty() || feedback.is_empty()) {
		project_state->set_session_state(AI_NEXT_SESSION_FAILED);
		event_log->record_event("feedback_planning_failed", milestone_id, String(), "planning_agent", "NEXT feedback or active milestone is empty.");
		emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
		emit_signal(SNAME("project_state_changed"));
		return;
	}

	const int previous_task_count = project_state->get_task_count(milestone_id);
	AIAgentMessage user_message;
	user_message.role = AI_AGENT_ROLE_USER;
	user_message.content = vformat("Turn this playtest feedback into additional NEXT tasks for milestone `%s` by calling ai_next.manage_project with append_tasks:\n\n%s", milestone_id, feedback);

	Vector<AIAgentMessage> messages;
	messages.push_back(user_message);
	AIAgentRuntimeResult result = planning_agent->run(messages);
	if (!result.success) {
		project_state->set_session_state(AI_NEXT_SESSION_FAILED);
		event_log->record_event("feedback_planning_failed", milestone_id, String(), "planning_agent", result.error);
		emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
		emit_signal(SNAME("project_state_changed"));
		return;
	}

	if (project_state->get_task_count(milestone_id) <= previous_task_count) {
		project_state->set_session_state(AI_NEXT_SESSION_FAILED);
		event_log->record_event("feedback_planning_failed", milestone_id, String(), "planning_agent", "Feedback planning completed without appending tasks.");
		emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
		emit_signal(SNAME("project_state_changed"));
		return;
	}

	project_state->set_session_state(AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL);
	event_log->record_event("feedback_tasks_generated", milestone_id, String(), "planning_agent", "NEXT feedback tasks generated.");
	emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
	emit_signal(SNAME("project_state_changed"));
}

void AIAgentNextSession::accept_and_lock_active_milestone() {
	String error;
	const String milestone_id = project_state->get_active_milestone_id();
	if (!project_state->lock_milestone(milestone_id, error)) {
		project_state->set_session_state(AI_NEXT_SESSION_FAILED);
		event_log->record_event("milestone_lock_failed", milestone_id, String(), "next_session", error);
	} else {
		project_state->set_session_state(AI_NEXT_SESSION_READY_TO_LOCK);
		event_log->record_event("milestone_locked", milestone_id, String(), "user", "NEXT milestone locked.");
	}
	emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
	emit_signal(SNAME("project_state_changed"));
}

void AIAgentNextSession::cancel_current_operation() {
	project_state->set_session_state(AI_NEXT_SESSION_IDLE);
	event_log->record_event("operation_cancelled", project_state->get_active_milestone_id(), String(), "user", "NEXT operation cancelled.");
	emit_signal(SNAME("state_changed"), project_state->get_session_state_name());
	emit_signal(SNAME("project_state_changed"));
}
