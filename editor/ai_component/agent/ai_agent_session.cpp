/**************************************************************************/
/*  ai_agent_session.cpp                                                   */
/**************************************************************************/

#include "ai_agent_session.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/templates/local_vector.h"

#include "editor/ai_component/tools/editor/ai_get_editor_context_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_add_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_create_scene_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_delete_node_tool.h"
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
#include "editor/ai_component/providers/ai_mcp_settings.h"
#include "editor/ai_component/providers/ai_mcp_stdio_client.h"
#include "editor/ai_component/tools/ai_mcp_tool.h"
#include "editor/ai_component/tools/project/ai_list_project_tool.h"
#include "editor/ai_component/tools/project/ai_create_folder_tool.h"
#include "editor/ai_component/tools/project/ai_read_file_tool.h"
#include "editor/ai_component/tools/project/ai_search_project_tool.h"

void AIAgentSession::_bind_methods() {
	ClassDB::bind_method(D_METHOD("send_user_message", "message"), &AIAgentSession::send_user_message);
	ClassDB::bind_method(D_METHOD("cancel_request"), &AIAgentSession::cancel_request);
	ClassDB::bind_method(D_METHOD("start_new_session"), &AIAgentSession::start_new_session);
	ClassDB::bind_method(D_METHOD("load_session", "session_id"), &AIAgentSession::load_session);
	ClassDB::bind_method(D_METHOD("delete_session", "session_id"), &AIAgentSession::delete_session);
	ClassDB::bind_method(D_METHOD("approve_pending_tool"), &AIAgentSession::approve_pending_tool);
	ClassDB::bind_method(D_METHOD("reject_pending_tool"), &AIAgentSession::reject_pending_tool);
	ClassDB::bind_method(D_METHOD("get_pending_tool_approval"), &AIAgentSession::get_pending_tool_approval);
	ClassDB::bind_method(D_METHOD("get_messages_as_array"), &AIAgentSession::get_messages_as_array);
	ClassDB::bind_method(D_METHOD("set_agent_profile_id", "profile_id"), &AIAgentSession::set_agent_profile_id);
	ClassDB::bind_method(D_METHOD("get_agent_profile_id"), &AIAgentSession::get_agent_profile_id);
	ClassDB::bind_method(D_METHOD("get_token_usage"), &AIAgentSession::get_token_usage);
	ClassDB::bind_method(D_METHOD("is_tool_runtime_available"), &AIAgentSession::is_tool_runtime_available);

	ADD_SIGNAL(MethodInfo("message_added", PropertyInfo(Variant::DICTIONARY, "message")));
	ADD_SIGNAL(MethodInfo("message_updated", PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::DICTIONARY, "message")));
	ADD_SIGNAL(MethodInfo("message_removed", PropertyInfo(Variant::INT, "index")));
	ADD_SIGNAL(MethodInfo("state_changed", PropertyInfo(Variant::INT, "state")));
	ADD_SIGNAL(MethodInfo("token_usage_changed", PropertyInfo(Variant::DICTIONARY, "usage")));
	ADD_SIGNAL(MethodInfo("request_failed", PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("tool_approval_requested", PropertyInfo(Variant::DICTIONARY, "approval")));
}

AIAgentSession::AIAgentSession() {
	store.instantiate();
	runtime.instantiate();
	runtime_runner.instantiate();
	runtime_client.instantiate();
	tool_registry.instantiate();
	project_tree_context.instantiate();
	editor_context.instantiate();
	agent_profile = AIAgentProfile::get_plan_profile();

	runtime->set_client(runtime_client);
	_configure_tool_runtime();
	store->set_project_scope(_get_project_scope_key());

	runtime_runner->connect("runtime_finished", callable_mp(this, &AIAgentSession::_on_runtime_finished), CONNECT_DEFERRED);
	runtime_runner->connect("runtime_message_added", callable_mp(this, &AIAgentSession::_on_runtime_message_added), CONNECT_DEFERRED);
	runtime_runner->connect("runtime_message_updated", callable_mp(this, &AIAgentSession::_on_runtime_message_updated), CONNECT_DEFERRED);

	_load_initial_session();
}

void AIAgentSession::configure_provider(const AIProviderConfig &p_config) {
	if (runtime_client.is_valid()) {
		runtime_client->set_config(p_config);
	}
}

void AIAgentSession::set_agent_profile_id(const String &p_profile_id) {
	if (p_profile_id == "write") {
		agent_profile = AIAgentProfile::get_write_profile();
	} else if (p_profile_id == "review") {
		agent_profile = AIAgentProfile::get_review_profile();
	} else if (p_profile_id == "build") {
		agent_profile = AIAgentProfile::get_build_profile();
	} else {
		agent_profile = AIAgentProfile::get_plan_profile();
	}

	_apply_dynamic_tool_permissions();
	if (runtime.is_valid()) {
		runtime->set_profile(agent_profile);
	}
	print_line(vformat("[AI Agent][Session] Agent profile set: %s", agent_profile.id));
}

String AIAgentSession::get_agent_profile_id() const {
	return agent_profile.id;
}

Ref<AIAgentRuntime> AIAgentSession::get_agent_runtime() const {
	return runtime;
}

Ref<AIAgentRuntimeRunner> AIAgentSession::get_agent_runtime_runner() const {
	return runtime_runner;
}

Ref<AIToolRegistry> AIAgentSession::get_tool_registry() const {
	return tool_registry;
}

bool AIAgentSession::is_tool_runtime_available() const {
	return runtime.is_valid() && runtime_client.is_valid() && runtime->get_client().is_valid() && tool_registry.is_valid();
}

void AIAgentSession::reload_tool_runtime() {
	if (_is_busy()) {
		pending_tool_runtime_reload = true;
		print_line("[AI Agent][Session] Tool runtime reload skipped because the session is busy.");
		return;
	}
	pending_tool_runtime_reload = false;
	_configure_tool_runtime();
}

void AIAgentSession::send_user_message(const String &p_message) {
	String stripped = p_message.strip_edges();
	if (stripped.is_empty() || _is_busy()) {
		print_line(vformat("[AI Agent][Session] Ignored send request. empty=%s state=%d", stripped.is_empty() ? "yes" : "no", (int)state));
		return;
	}

	print_line(vformat("[AI Agent][Session] User message accepted. chars=%d existing_messages=%d", stripped.length(), messages.size()));
	if (messages.is_empty()) {
		title = stripped.substr(0, 80);
		print_line(vformat("[AI Agent][Session] Session title initialized: %s", title));
	}

	AIAgentMessage user_message;
	user_message.role = AI_AGENT_ROLE_USER;
	user_message.content = stripped;
	user_message.created_at = Time::get_singleton()->get_unix_time_from_system();
	messages.push_back(user_message);
	emit_signal(SNAME("message_added"), user_message.to_dict());
	_save();
	print_line(vformat("[AI Agent][Session] User message saved. session=%s messages=%d", session_id, messages.size()));

	AIAgentMessage assistant_message;
	assistant_message.role = AI_AGENT_ROLE_ASSISTANT;
	assistant_message.created_at = Time::get_singleton()->get_unix_time_from_system();
	messages.push_back(assistant_message);
	active_assistant_index = messages.size() - 1;
	emit_signal(SNAME("message_added"), assistant_message.to_dict());
	print_line(vformat("[AI Agent][Session] Assistant placeholder added. index=%d", active_assistant_index));

	_start_runtime_turn();
}

void AIAgentSession::cancel_request() {
	if (state == AI_AGENT_STATE_STREAMING || state == AI_AGENT_STATE_PREPARING_CONTEXT) {
		print_line(vformat("[AI Agent][Session] Cancelling active request. state=%d", (int)state));
		if (runtime_runner.is_valid() && runtime_runner->is_running() && active_assistant_index >= 0 && active_assistant_index < messages.size() && messages[active_assistant_index].content.is_empty()) {
			_remove_message_at(active_assistant_index);
			active_assistant_index = -1;
		}
		_set_state(AI_AGENT_STATE_CANCELLED);
		_save();
		if (pending_tool_runtime_reload) {
			print_line("[AI Agent][Session] Applying deferred tool runtime reload after cancellation.");
			reload_tool_runtime();
		}
	} else if (state == AI_AGENT_STATE_WAITING_TOOL_APPROVAL) {
		reject_pending_tool();
	}
}

void AIAgentSession::start_new_session() {
	if (state == AI_AGENT_STATE_WAITING_TOOL_APPROVAL) {
		pending_tool_approval.clear();
	}
	session_id = OS::get_singleton()->get_unique_id() + "_" + itos(Time::get_singleton()->get_unix_time_from_system()) + "_" + itos(Math::rand());
	title = "New Chat";
	messages.clear();
	active_assistant_index = -1;
	runtime_base_message_count = 0;
	runtime_progress_message_count = 0;
	runtime_to_local_message_indices.clear();
	pending_tool_approval.clear();
	_recalculate_token_usage();
	_set_state(AI_AGENT_STATE_IDLE);
	print_line(vformat("[AI Agent][Session] Started new session. session=%s", session_id));
}

bool AIAgentSession::load_session(const String &p_session_id) {
	if (state == AI_AGENT_STATE_WAITING_TOOL_APPROVAL) {
		pending_tool_approval.clear();
	}
	if (state == AI_AGENT_STATE_STREAMING || state == AI_AGENT_STATE_PREPARING_CONTEXT) {
		print_line("[AI Agent][Session] Loading another session while request is active; cancelling current request.");
		cancel_request();
	}

	String loaded_title;
	Vector<AIAgentMessage> loaded_messages;
	if (!store->load_conversation(p_session_id, loaded_title, loaded_messages)) {
		print_line(vformat("[AI Agent][Session] Failed to load session: %s", p_session_id));
		return false;
	}

	session_id = p_session_id;
	title = loaded_title.is_empty() ? String("New Chat") : loaded_title;
	messages = loaded_messages;
	active_assistant_index = -1;
	runtime_base_message_count = messages.size();
	runtime_progress_message_count = 0;
	runtime_to_local_message_indices.clear();
	pending_tool_approval.clear();
	_recalculate_token_usage();
	_set_state(AI_AGENT_STATE_IDLE);
	print_line(vformat("[AI Agent][Session] Loaded session. session=%s title=%s messages=%d", session_id, title, messages.size()));
	return true;
}

bool AIAgentSession::delete_session(const String &p_session_id) {
	if (_is_busy()) {
		print_line("[AI Agent][Session] Refused to delete session while request is active.");
		return false;
	}

	if (p_session_id.is_empty() || store.is_null()) {
		return false;
	}

	const bool deleting_current = p_session_id == session_id;
	if (!store->delete_conversation(p_session_id)) {
		print_line(vformat("[AI Agent][Session] Failed to delete session: %s", p_session_id));
		return false;
	}

	print_line(vformat("[AI Agent][Session] Deleted session: %s", p_session_id));
	if (deleting_current) {
		_load_initial_session();
	}
	return true;
}

bool AIAgentSession::approve_pending_tool() {
	if (pending_tool_approval.is_empty()) {
		print_line("[AI Agent][Session] No pending tool approval to approve.");
		return false;
	}

	AIToolCall call = AIToolCall::from_dict(pending_tool_approval);
	Ref<AITool> tool = tool_registry->get_tool(call.tool_name);
	if (tool.is_null()) {
		AIToolResult result;
		result.error = "Tool is not registered.";
		call.status = AI_TOOL_CALL_STATUS_FAILED;
		call.updated_at = Time::get_singleton()->get_unix_time_from_system();
		_update_tool_call_status(call);
		_append_tool_result_message(call, result);
		pending_tool_approval.clear();
		_start_runtime_turn();
		return false;
	}

	print_line(vformat("[AI Agent][Session] Approved pending tool. name=%s", call.tool_name));
	AIToolResult result = tool->execute(call.arguments);
	call.status = result.is_error() ? AI_TOOL_CALL_STATUS_FAILED : AI_TOOL_CALL_STATUS_COMPLETED;
	call.updated_at = Time::get_singleton()->get_unix_time_from_system();
	_update_tool_call_status(call);
	_append_tool_result_message(call, result);
	pending_tool_approval.clear();
	_start_runtime_turn();
	return !result.is_error();
}

bool AIAgentSession::reject_pending_tool() {
	if (pending_tool_approval.is_empty()) {
		print_line("[AI Agent][Session] No pending tool approval to reject.");
		return false;
	}

	AIToolCall call = AIToolCall::from_dict(pending_tool_approval);
	call.status = AI_TOOL_CALL_STATUS_DENIED;
	call.updated_at = Time::get_singleton()->get_unix_time_from_system();
	_update_tool_call_status(call);
	AIAgentMessage tool_message;
	tool_message.role = AI_AGENT_ROLE_TOOL;
	tool_message.created_at = Time::get_singleton()->get_unix_time_from_system();
	tool_message.content = "Tool call denied for `" + call.tool_name + "`: user rejected the approval request.";
	tool_message.metadata["tool_call_id"] = call.id;
	tool_message.metadata["tool_name"] = call.tool_name;
	tool_message.metadata["status"] = AIToolCall::status_to_string(AI_TOOL_CALL_STATUS_DENIED);
	messages.push_back(tool_message);
	emit_signal(SNAME("message_added"), tool_message.to_dict());
	pending_tool_approval.clear();
	_recalculate_token_usage();
	_save();
	print_line(vformat("[AI Agent][Session] Rejected pending tool. name=%s", call.tool_name));
	_start_runtime_turn();
	return true;
}

Dictionary AIAgentSession::get_pending_tool_approval() const {
	return pending_tool_approval;
}

Array AIAgentSession::get_messages_as_array() const {
	Array array;
	for (int i = 0; i < messages.size(); i++) {
		array.push_back(messages[i].to_dict());
	}
	return array;
}

String AIAgentSession::get_session_id() const {
	return session_id;
}

String AIAgentSession::get_title() const {
	return title;
}

AIAgentState AIAgentSession::get_state() const {
	return state;
}

Dictionary AIAgentSession::get_token_usage() const {
	return token_usage.duplicate(true);
}

Array AIAgentSession::list_sessions() const {
	return store->list_conversations();
}

String AIAgentSession::_get_project_scope_key() const {
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (!project_settings) {
		return "global";
	}

	String resource_path = project_settings->get_resource_path();
	if (resource_path.is_empty()) {
		return "global";
	}
	return resource_path.md5_text();
}

void AIAgentSession::_load_initial_session() {
	String latest_session_id;
	if (store.is_valid() && store->get_most_recent_conversation_id(latest_session_id) && load_session(latest_session_id)) {
		print_line(vformat("[AI Agent][Session] Loaded most recent project session. session=%s", latest_session_id));
		return;
	}

	start_new_session();
}

void AIAgentSession::_set_state(AIAgentState p_state) {
	state = p_state;
	print_line(vformat("[AI Agent][Session] State changed: %d", (int)state));
	emit_signal(SNAME("state_changed"), (int)state);
}

Array AIAgentSession::_collect_context() {
	Array context;
	context.append_array(editor_context->collect_context());
	context.append_array(project_tree_context->collect_context());
	return context;
}

void AIAgentSession::_save() {
	store->save_conversation(session_id, title, messages);
}

void AIAgentSession::_recalculate_token_usage() {
	Dictionary usage;
	int message_count = 0;
	int estimated_input_tokens = 0;
	int estimated_input_chars = 0;
	for (int i = 0; i < messages.size(); i++) {
		const AIAgentMessage &message = messages[i];
		if (message.metadata.is_empty()) {
			continue;
		}

		if (message.metadata.has("usage") && Variant(message.metadata["usage"]).get_type() == Variant::DICTIONARY) {
			Dictionary message_usage = message.metadata["usage"];
			usage["prompt_tokens"] = (int)usage.get("prompt_tokens", 0) + (int)message_usage.get("prompt_tokens", 0);
			usage["completion_tokens"] = (int)usage.get("completion_tokens", 0) + (int)message_usage.get("completion_tokens", 0);
			usage["total_tokens"] = (int)usage.get("total_tokens", 0) + (int)message_usage.get("total_tokens", 0);
			message_count++;
		}

		if (message.metadata.has("estimated_context_usage") && Variant(message.metadata["estimated_context_usage"]).get_type() == Variant::DICTIONARY) {
			Dictionary estimate = message.metadata["estimated_context_usage"];
			estimated_input_tokens += (int)estimate.get("estimated_input_tokens", 0);
			estimated_input_chars += (int)estimate.get("estimated_input_chars", 0);
		}
	}

	usage["message_count"] = message_count;
	usage["estimated_input_tokens"] = estimated_input_tokens;
	usage["estimated_input_chars"] = estimated_input_chars;
	if (!usage.has("prompt_tokens")) {
		usage["prompt_tokens"] = 0;
	}
	if (!usage.has("completion_tokens")) {
		usage["completion_tokens"] = 0;
	}
	if (!usage.has("total_tokens")) {
		usage["total_tokens"] = 0;
	}
	token_usage = usage;
	emit_signal(SNAME("token_usage_changed"), token_usage);
	print_line(vformat("[AI Agent][Session] Token usage recalculated. prompt=%d completion=%d total=%d estimated_input=%d messages=%d",
			(int)token_usage.get("prompt_tokens", 0),
			(int)token_usage.get("completion_tokens", 0),
			(int)token_usage.get("total_tokens", 0),
			(int)token_usage.get("estimated_input_tokens", 0),
			(int)token_usage.get("message_count", 0)));
}

void AIAgentSession::_remove_message_at(int p_index) {
	if (p_index < 0 || p_index >= messages.size()) {
		return;
	}

	messages.remove_at(p_index);
	emit_signal(SNAME("message_removed"), p_index);
	LocalVector<int> stale_runtime_indices;
	for (const KeyValue<int, int> &E : runtime_to_local_message_indices) {
		if (E.value == p_index) {
			stale_runtime_indices.push_back(E.key);
		}
	}
	for (uint32_t i = 0; i < stale_runtime_indices.size(); i++) {
		runtime_to_local_message_indices.erase(stale_runtime_indices[i]);
	}
	for (KeyValue<int, int> &E : runtime_to_local_message_indices) {
		if (E.value > p_index) {
			E.value--;
		}
	}
	_recalculate_token_usage();
}

void AIAgentSession::_append_tool_result_message(const AIToolCall &p_call, const AIToolResult &p_tool_result) {
	AIAgentMessage tool_message;
	tool_message.role = AI_AGENT_ROLE_TOOL;
	tool_message.created_at = Time::get_singleton()->get_unix_time_from_system();
	tool_message.metadata = p_tool_result.metadata;
	tool_message.metadata["tool_call_id"] = p_call.id;
	tool_message.metadata["tool_name"] = p_call.tool_name;
	tool_message.metadata["truncated"] = p_tool_result.truncated;
	if (p_tool_result.is_error()) {
		tool_message.content = "Tool call failed for `" + p_call.tool_name + "`: " + p_tool_result.error;
		tool_message.metadata["status"] = AIToolCall::status_to_string(AI_TOOL_CALL_STATUS_FAILED);
	} else {
		tool_message.content = p_tool_result.content;
		tool_message.metadata["status"] = AIToolCall::status_to_string(AI_TOOL_CALL_STATUS_COMPLETED);
	}
	messages.push_back(tool_message);
	emit_signal(SNAME("message_added"), tool_message.to_dict());
	_recalculate_token_usage();
	_save();
}

bool AIAgentSession::_update_tool_call_status(const AIToolCall &p_call) {
	for (int i = messages.size() - 1; i >= 0; i--) {
		AIAgentMessage message = messages[i];
		if (!message.metadata.has("tool_calls") || Variant(message.metadata["tool_calls"]).get_type() != Variant::ARRAY) {
			continue;
		}

		Array tool_calls = message.metadata["tool_calls"];
		for (int j = 0; j < tool_calls.size(); j++) {
			if (Variant(tool_calls[j]).get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary call_dict = tool_calls[j];
			if (String(call_dict.get("id", "")) != p_call.id) {
				continue;
			}

			tool_calls[j] = p_call.to_dict();
			message.metadata["tool_calls"] = tool_calls;
			messages.write[i] = message;
			emit_signal(SNAME("message_updated"), i, message.to_dict());
			return true;
		}
	}
	return false;
}

bool AIAgentSession::_start_runtime_turn() {
	if (!is_tool_runtime_available()) {
		print_line("[AI Agent][Session] Failed before runtime start: tool runtime is not available.");
		_on_provider_request_failed("AI runtime is not configured.");
		return false;
	}

	_set_state(AI_AGENT_STATE_PREPARING_CONTEXT);
	print_line("[AI Agent][Session] Collecting editor/project context...");
	Array context = _collect_context();
	print_line(vformat("[AI Agent][Session] Context collected. documents=%d", context.size()));

	Vector<AIAgentMessage> request_messages = messages;
	if (active_assistant_index >= 0 && active_assistant_index < request_messages.size() && request_messages[active_assistant_index].content.is_empty()) {
		request_messages.remove_at(active_assistant_index);
		print_line("[AI Agent][Session] Removed assistant placeholder from provider request messages.");
	}

	_set_state(AI_AGENT_STATE_STREAMING);
	print_line(vformat("[AI Agent][Session] Starting function-calling runtime. request_messages=%d", request_messages.size()));
	runtime_base_message_count = request_messages.size();
	runtime_progress_message_count = 0;
	runtime_to_local_message_indices.clear();
	runtime->set_session_id(session_id);
	if (!runtime_runner->start(request_messages, context)) {
		print_line("[AI Agent][Session] Failed to start function-calling runtime.");
		_on_provider_request_failed("Failed to start AI runtime.");
		return false;
	}
	return true;
}

bool AIAgentSession::_is_busy() const {
	return state == AI_AGENT_STATE_STREAMING || state == AI_AGENT_STATE_PREPARING_CONTEXT || state == AI_AGENT_STATE_WAITING_TOOL_APPROVAL;
}

void AIAgentSession::_configure_tool_runtime() {
	if (tool_registry.is_null()) {
		tool_registry.instantiate();
	} else {
		tool_registry->clear();
	}

	Ref<AIListProjectTool> list_project;
	list_project.instantiate();
	tool_registry->register_tool(list_project);
	print_line("[AI Agent][Session] Registered tool: project.list_tree");

	Ref<AIReadFileTool> read_file;
	read_file.instantiate();
	tool_registry->register_tool(read_file);
	print_line("[AI Agent][Session] Registered tool: project.read_file");

	Ref<AICreateFolderTool> create_folder;
	create_folder.instantiate();
	tool_registry->register_tool(create_folder);
	print_line("[AI Agent][Session] Registered tool: project.create_folder");

	Ref<AISearchProjectTool> search_project;
	search_project.instantiate();
	tool_registry->register_tool(search_project);
	print_line("[AI Agent][Session] Registered tool: project.search_text");

	Ref<AIGetEditorContextTool> editor_context_tool;
	editor_context_tool.instantiate();
	tool_registry->register_tool(editor_context_tool);
	print_line("[AI Agent][Session] Registered tool: editor.get_context");

	Ref<AISceneCreateSceneTool> create_scene_tool;
	create_scene_tool.instantiate();
	tool_registry->register_tool(create_scene_tool);
	print_line("[AI Agent][Session] Registered tool: scene.create_scene");

	Ref<AISceneAddNodeTool> add_node_tool;
	add_node_tool.instantiate();
	tool_registry->register_tool(add_node_tool);
	print_line("[AI Agent][Session] Registered tool: scene.add_node");

	Ref<AISceneDeleteNodeTool> delete_node_tool;
	delete_node_tool.instantiate();
	tool_registry->register_tool(delete_node_tool);
	print_line("[AI Agent][Session] Registered tool: scene.delete_node");

	Ref<AISceneListPropertiesTool> list_properties_tool;
	list_properties_tool.instantiate();
	tool_registry->register_tool(list_properties_tool);
	print_line("[AI Agent][Session] Registered tool: scene.list_properties");

	Ref<AISceneRenameNodeTool> rename_node_tool;
	rename_node_tool.instantiate();
	tool_registry->register_tool(rename_node_tool);
	print_line("[AI Agent][Session] Registered tool: scene.rename_node");

	Ref<AISceneMoveNodeTool> move_node_tool;
	move_node_tool.instantiate();
	tool_registry->register_tool(move_node_tool);
	print_line("[AI Agent][Session] Registered tool: scene.move_node");

	Ref<AISceneSetPropertyTool> set_property_tool;
	set_property_tool.instantiate();
	tool_registry->register_tool(set_property_tool);
	print_line("[AI Agent][Session] Registered tool: scene.set_property");

	Ref<AISceneSaveCurrentSceneTool> save_current_scene_tool;
	save_current_scene_tool.instantiate();
	tool_registry->register_tool(save_current_scene_tool);
	print_line("[AI Agent][Session] Registered tool: scene.save_current_scene");

	Ref<AISceneOpenSceneTool> open_scene_tool;
	open_scene_tool.instantiate();
	tool_registry->register_tool(open_scene_tool);
	print_line("[AI Agent][Session] Registered tool: scene.open_scene");

	Ref<AIScriptInspectTool> script_inspect_tool;
	script_inspect_tool.instantiate();
	tool_registry->register_tool(script_inspect_tool);
	print_line("[AI Agent][Session] Registered tool: script.inspect");

	Ref<AIScriptCreateTool> script_create_tool;
	script_create_tool.instantiate();
	tool_registry->register_tool(script_create_tool);
	print_line("[AI Agent][Session] Registered tool: script.create");

	Ref<AIScriptWriteTool> script_write_tool;
	script_write_tool.instantiate();
	tool_registry->register_tool(script_write_tool);
	print_line("[AI Agent][Session] Registered tool: script.write");

	Ref<AIScriptPatchFunctionTool> script_patch_function_tool;
	script_patch_function_tool.instantiate();
	tool_registry->register_tool(script_patch_function_tool);
	print_line("[AI Agent][Session] Registered tool: script.patch_function");

	Ref<AIScriptBindToNodeTool> script_bind_to_node_tool;
	script_bind_to_node_tool.instantiate();
	tool_registry->register_tool(script_bind_to_node_tool);
	print_line("[AI Agent][Session] Registered tool: script.bind_to_node");

	Ref<AIScriptUnbindFromNodeTool> script_unbind_from_node_tool;
	script_unbind_from_node_tool.instantiate();
	tool_registry->register_tool(script_unbind_from_node_tool);
	print_line("[AI Agent][Session] Registered tool: script.unbind_from_node");

	Ref<AIScriptDeleteTool> script_delete_tool;
	script_delete_tool.instantiate();
	tool_registry->register_tool(script_delete_tool);
	print_line("[AI Agent][Session] Registered tool: script.delete");

	Ref<AIShaderApplyToNodeTool> shader_apply_to_node_tool;
	shader_apply_to_node_tool.instantiate();
	tool_registry->register_tool(shader_apply_to_node_tool);
	print_line("[AI Agent][Session] Registered tool: shader.apply_to_node");

	_register_mcp_tools();
	_apply_dynamic_tool_permissions();

	runtime->set_tool_registry(tool_registry);
	runtime->set_profile(agent_profile);
	runtime_runner->set_runtime(runtime);
}

void AIAgentSession::_register_mcp_tools() {
	Vector<AIMCPServerConfig> servers = AIMCPSettings::get_servers(true);
	for (int i = 0; i < servers.size(); i++) {
		const AIMCPServerConfig &server = servers[i];
		Ref<AIMCPStdioClient> client;
		client.instantiate();
		client->set_server_config(server);
		client->set_timeout_msec(3000);

		Vector<AIMCPToolDescriptor> tools;
		String error;
		if (!client->list_tools(tools, error)) {
			print_line(vformat("[AI Agent][MCP] Failed to discover tools. server=%s error=%s", server.display_name, error));
			continue;
		}

		for (int j = 0; j < tools.size(); j++) {
			Ref<AIMCPTool> tool;
			tool.instantiate();
			tool->setup(server, tools[j]);
			if (tool_registry->register_tool(tool)) {
				print_line(vformat("[AI Agent][Session] Registered MCP tool: %s server=%s source_tool=%s", tool->get_name(), server.display_name, tools[j].name));
			}
		}
	}
}

void AIAgentSession::_apply_dynamic_tool_permissions() {
	if (tool_registry.is_null()) {
		return;
	}

	Vector<String> tool_names = tool_registry->get_tool_names();
	for (int i = 0; i < tool_names.size(); i++) {
		const String &tool_name = tool_names[i];
		if (tool_name.begins_with("mcp_")) {
			agent_profile.allowed_tools.erase(tool_name);
			agent_profile.ask_tools.insert(tool_name);
		}
	}
}

void AIAgentSession::_apply_runtime_result(const AIAgentRuntimeResult &p_result) {
	if (!p_result.success) {
		print_line(vformat("[AI Agent][Session] Runtime result failed: %s", p_result.error));
		_on_provider_request_failed(p_result.error.is_empty() ? String("Agent runtime failed.") : p_result.error);
		return;
	}

	if (active_assistant_index >= 0 && active_assistant_index < messages.size() && messages[active_assistant_index].content.is_empty()) {
		_remove_message_at(active_assistant_index);
		print_line("[AI Agent][Session] Removed assistant placeholder before applying runtime messages.");
	}

	const int base_message_count = runtime_base_message_count;
	print_line(vformat("[AI Agent][Session] Applying runtime result. base_messages=%d progress_messages=%d mapped_messages=%d result_messages=%d tool_calls=%d", base_message_count, runtime_progress_message_count, runtime_to_local_message_indices.size(), p_result.messages.size(), p_result.tool_calls.size()));
	for (int i = base_message_count; i < p_result.messages.size(); i++) {
		if (runtime_to_local_message_indices.has(i)) {
			continue;
		}

		messages.push_back(p_result.messages[i]);
		emit_signal(SNAME("message_added"), p_result.messages[i].to_dict());
	}

	active_assistant_index = -1;
	runtime_base_message_count = messages.size();
	runtime_progress_message_count = 0;
	runtime_to_local_message_indices.clear();
	_recalculate_token_usage();
	if (!p_result.pending_approval.is_empty()) {
		pending_tool_approval = p_result.pending_approval.duplicate(true);
		_set_state(AI_AGENT_STATE_WAITING_TOOL_APPROVAL);
		emit_signal(SNAME("tool_approval_requested"), pending_tool_approval);
		_save();
		print_line(vformat("[AI Agent][Session] Runtime result is waiting for tool approval. tool=%s", String(pending_tool_approval.get("tool_name", ""))));
		return;
	}

	_set_state(AI_AGENT_STATE_IDLE);
	_save();
	if (pending_tool_runtime_reload) {
		print_line("[AI Agent][Session] Applying deferred tool runtime reload.");
		reload_tool_runtime();
	}
	print_line(vformat("[AI Agent][Session] Runtime result applied and saved. session=%s messages=%d", session_id, messages.size()));
}

void AIAgentSession::_on_provider_request_failed(const String &p_message) {
	print_line(vformat("[AI Agent][Session] Provider/runtime request failed: %s", p_message));
	if (active_assistant_index >= 0 && active_assistant_index < messages.size() && messages[active_assistant_index].content.is_empty()) {
		_remove_message_at(active_assistant_index);
		active_assistant_index = -1;
		print_line("[AI Agent][Session] Removed empty assistant placeholder after failure.");
	}

	AIAgentMessage error_message;
	error_message.role = AI_AGENT_ROLE_ERROR;
	error_message.content = p_message;
	error_message.created_at = Time::get_singleton()->get_unix_time_from_system();
	messages.push_back(error_message);
	emit_signal(SNAME("message_added"), error_message.to_dict());
	emit_signal(SNAME("request_failed"), p_message);
	_recalculate_token_usage();
	_set_state(AI_AGENT_STATE_FAILED);
	runtime_base_message_count = messages.size();
	runtime_progress_message_count = 0;
	runtime_to_local_message_indices.clear();
	_save();
	if (pending_tool_runtime_reload) {
		print_line("[AI Agent][Session] Applying deferred tool runtime reload after failure.");
		reload_tool_runtime();
	}
}

void AIAgentSession::_on_runtime_finished() {
	if (state == AI_AGENT_STATE_CANCELLED) {
		print_line("[AI Agent][Session] Runtime finished after cancellation; result ignored.");
		return;
	}
	print_line("[AI Agent][Session] Runtime runner finished; applying result.");
	_apply_runtime_result(runtime_runner->get_last_result());
}

void AIAgentSession::_on_runtime_message_added(int p_index, const Dictionary &p_message) {
	if (state == AI_AGENT_STATE_CANCELLED) {
		return;
	}
	if (state != AI_AGENT_STATE_STREAMING && state != AI_AGENT_STATE_PREPARING_CONTEXT) {
		print_line(vformat("[AI Agent][Session] Ignored runtime progress add outside active run. runtime_index=%d state=%d", p_index, (int)state));
		return;
	}

	if (p_index < runtime_base_message_count) {
		print_line(vformat("[AI Agent][Session] Ignored runtime progress add for base history. runtime_index=%d base_messages=%d", p_index, runtime_base_message_count));
		return;
	}

	AIAgentMessage message = AIAgentMessage::from_dict(p_message);
	if (active_assistant_index >= 0 && active_assistant_index < messages.size() && messages[active_assistant_index].content.is_empty() && message.role == AI_AGENT_ROLE_ASSISTANT) {
		const int replaced_index = active_assistant_index;
		messages.write[active_assistant_index] = message;
		emit_signal(SNAME("message_updated"), active_assistant_index, message.to_dict());
		active_assistant_index = -1;
		runtime_progress_message_count++;
		runtime_to_local_message_indices[p_index] = replaced_index;
		print_line(vformat("[AI Agent][Session] Runtime progress updated assistant placeholder. runtime_index=%d local_index=%d role=%s", p_index, replaced_index, AIAgentMessage::role_to_string(message.role)));
		return;
	}

	messages.push_back(message);
	emit_signal(SNAME("message_added"), message.to_dict());
	runtime_to_local_message_indices[p_index] = messages.size() - 1;
	runtime_progress_message_count++;
	print_line(vformat("[AI Agent][Session] Runtime progress message added. runtime_index=%d local_index=%d role=%s", p_index, messages.size() - 1, AIAgentMessage::role_to_string(message.role)));
}

void AIAgentSession::_on_runtime_message_updated(int p_index, const Dictionary &p_message) {
	if (state == AI_AGENT_STATE_CANCELLED) {
		return;
	}
	if (state != AI_AGENT_STATE_STREAMING && state != AI_AGENT_STATE_PREPARING_CONTEXT) {
		print_line(vformat("[AI Agent][Session] Ignored runtime progress update outside active run. runtime_index=%d state=%d", p_index, (int)state));
		return;
	}

	const int *mapped_index = runtime_to_local_message_indices.getptr(p_index);
	if (!mapped_index) {
		print_line(vformat("[AI Agent][Session] Ignored runtime progress update without local mapping. runtime_index=%d mapped_messages=%d messages=%d", p_index, runtime_to_local_message_indices.size(), messages.size()));
		return;
	}

	const int local_index = *mapped_index;
	if (local_index < 0 || local_index >= messages.size()) {
		print_line(vformat("[AI Agent][Session] Ignored runtime progress update outside local range. runtime_index=%d local_index=%d messages=%d", p_index, local_index, messages.size()));
		return;
	}

	AIAgentMessage message = AIAgentMessage::from_dict(p_message);
	messages.write[local_index] = message;
	emit_signal(SNAME("message_updated"), local_index, message.to_dict());
	print_line(vformat("[AI Agent][Session] Runtime progress message updated. runtime_index=%d local_index=%d role=%s", p_index, local_index, AIAgentMessage::role_to_string(message.role)));
}

void AIAgentSession::replace_messages_for_test(const Vector<AIAgentMessage> &p_messages, int p_active_assistant_index) {
	messages = p_messages;
	active_assistant_index = p_active_assistant_index;
	runtime_base_message_count = active_assistant_index >= 0 ? active_assistant_index : messages.size();
	runtime_progress_message_count = 0;
	runtime_to_local_message_indices.clear();
	_recalculate_token_usage();
	_set_state(active_assistant_index >= 0 ? AI_AGENT_STATE_STREAMING : AI_AGENT_STATE_IDLE);
}

void AIAgentSession::apply_runtime_result_for_test(const AIAgentRuntimeResult &p_result) {
	_apply_runtime_result(p_result);
}

void AIAgentSession::add_runtime_message_for_test(int p_index, const AIAgentMessage &p_message) {
	_on_runtime_message_added(p_index, p_message.to_dict());
}

void AIAgentSession::update_runtime_message_for_test(int p_index, const AIAgentMessage &p_message) {
	_on_runtime_message_updated(p_index, p_message.to_dict());
}

Error AIAgentSession::save_for_test() {
	_save();
	return OK;
}

void AIAgentSession::set_conversation_project_scope_for_test(const String &p_project_scope_key) {
	store->set_project_scope(p_project_scope_key);
	_load_initial_session();
}

Ref<AIConversationStore> AIAgentSession::get_conversation_store_for_test() const {
	return store;
}
