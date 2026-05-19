/**************************************************************************/
/*  ai_agent_session.cpp                                                   */
/**************************************************************************/

#include "ai_agent_session.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/os/time.h"

#include "editor/ai_component/tools/editor/ai_get_editor_context_tool.h"
#include "editor/ai_component/tools/project/ai_list_project_tool.h"
#include "editor/ai_component/tools/project/ai_read_file_tool.h"
#include "editor/ai_component/tools/project/ai_search_project_tool.h"

void AIAgentSession::_bind_methods() {
	ClassDB::bind_method(D_METHOD("send_user_message", "message"), &AIAgentSession::send_user_message);
	ClassDB::bind_method(D_METHOD("cancel_request"), &AIAgentSession::cancel_request);
	ClassDB::bind_method(D_METHOD("start_new_session"), &AIAgentSession::start_new_session);
	ClassDB::bind_method(D_METHOD("load_session", "session_id"), &AIAgentSession::load_session);
	ClassDB::bind_method(D_METHOD("delete_session", "session_id"), &AIAgentSession::delete_session);
	ClassDB::bind_method(D_METHOD("get_messages_as_array"), &AIAgentSession::get_messages_as_array);
	ClassDB::bind_method(D_METHOD("set_agent_profile_id", "profile_id"), &AIAgentSession::set_agent_profile_id);
	ClassDB::bind_method(D_METHOD("get_agent_profile_id"), &AIAgentSession::get_agent_profile_id);
	ClassDB::bind_method(D_METHOD("is_tool_runtime_available"), &AIAgentSession::is_tool_runtime_available);

	ADD_SIGNAL(MethodInfo("message_added", PropertyInfo(Variant::DICTIONARY, "message")));
	ADD_SIGNAL(MethodInfo("message_updated", PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::DICTIONARY, "message")));
	ADD_SIGNAL(MethodInfo("message_removed", PropertyInfo(Variant::INT, "index")));
	ADD_SIGNAL(MethodInfo("state_changed", PropertyInfo(Variant::INT, "state")));
	ADD_SIGNAL(MethodInfo("request_failed", PropertyInfo(Variant::STRING, "message")));
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

	_load_initial_session();
}

void AIAgentSession::configure_provider(const AIProviderConfig &p_config) {
	if (runtime_client.is_valid()) {
		runtime_client->set_config(p_config);
	}
}

void AIAgentSession::set_agent_profile_id(const String &p_profile_id) {
	if (p_profile_id == "build") {
		agent_profile = AIAgentProfile::get_build_profile();
	} else {
		agent_profile = AIAgentProfile::get_plan_profile();
	}

	if (runtime.is_valid()) {
		runtime->set_profile(agent_profile);
	}
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

void AIAgentSession::send_user_message(const String &p_message) {
	String stripped = p_message.strip_edges();
	if (stripped.is_empty() || state == AI_AGENT_STATE_STREAMING || state == AI_AGENT_STATE_PREPARING_CONTEXT) {
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
	if (is_tool_runtime_available()) {
		print_line(vformat("[AI Agent][Session] Starting function-calling runtime. request_messages=%d", request_messages.size()));
		if (!runtime_runner->start(request_messages, context)) {
			print_line("[AI Agent][Session] Failed to start function-calling runtime.");
			_on_provider_request_failed("Failed to start AI runtime.");
		}
	} else {
		print_line("[AI Agent][Session] Failed before runtime start: tool runtime is not available.");
		_on_provider_request_failed("AI runtime is not configured.");
	}
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
	}
}

void AIAgentSession::start_new_session() {
	session_id = OS::get_singleton()->get_unique_id() + "_" + itos(Time::get_singleton()->get_unix_time_from_system()) + "_" + itos(Math::rand());
	title = "New Chat";
	messages.clear();
	active_assistant_index = -1;
	_set_state(AI_AGENT_STATE_IDLE);
	print_line(vformat("[AI Agent][Session] Started new session. session=%s", session_id));
}

bool AIAgentSession::load_session(const String &p_session_id) {
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
	_set_state(AI_AGENT_STATE_IDLE);
	print_line(vformat("[AI Agent][Session] Loaded session. session=%s title=%s messages=%d", session_id, title, messages.size()));
	return true;
}

bool AIAgentSession::delete_session(const String &p_session_id) {
	if (state == AI_AGENT_STATE_STREAMING || state == AI_AGENT_STATE_PREPARING_CONTEXT) {
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

void AIAgentSession::_remove_message_at(int p_index) {
	if (p_index < 0 || p_index >= messages.size()) {
		return;
	}

	messages.remove_at(p_index);
	emit_signal(SNAME("message_removed"), p_index);
}

void AIAgentSession::_configure_tool_runtime() {
	Ref<AIListProjectTool> list_project;
	list_project.instantiate();
	tool_registry->register_tool(list_project);
	print_line("[AI Agent][Session] Registered tool: project.list_tree");

	Ref<AIReadFileTool> read_file;
	read_file.instantiate();
	tool_registry->register_tool(read_file);
	print_line("[AI Agent][Session] Registered tool: project.read_file");

	Ref<AISearchProjectTool> search_project;
	search_project.instantiate();
	tool_registry->register_tool(search_project);
	print_line("[AI Agent][Session] Registered tool: project.search_text");

	Ref<AIGetEditorContextTool> editor_context_tool;
	editor_context_tool.instantiate();
	tool_registry->register_tool(editor_context_tool);
	print_line("[AI Agent][Session] Registered tool: editor.get_context");

	runtime->set_tool_registry(tool_registry);
	runtime->set_profile(agent_profile);
	runtime_runner->set_runtime(runtime);
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

	const int base_message_count = messages.size();
	print_line(vformat("[AI Agent][Session] Applying runtime result. base_messages=%d result_messages=%d tool_calls=%d", base_message_count, p_result.messages.size(), p_result.tool_calls.size()));
	for (int i = base_message_count; i < p_result.messages.size(); i++) {
		messages.push_back(p_result.messages[i]);
		emit_signal(SNAME("message_added"), p_result.messages[i].to_dict());
	}

	active_assistant_index = -1;
	_set_state(AI_AGENT_STATE_IDLE);
	_save();
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
	_set_state(AI_AGENT_STATE_FAILED);
	_save();
}

void AIAgentSession::_on_runtime_finished() {
	if (state == AI_AGENT_STATE_CANCELLED) {
		print_line("[AI Agent][Session] Runtime finished after cancellation; result ignored.");
		return;
	}
	print_line("[AI Agent][Session] Runtime runner finished; applying result.");
	_apply_runtime_result(runtime_runner->get_last_result());
}

void AIAgentSession::replace_messages_for_test(const Vector<AIAgentMessage> &p_messages, int p_active_assistant_index) {
	messages = p_messages;
	active_assistant_index = p_active_assistant_index;
	_set_state(active_assistant_index >= 0 ? AI_AGENT_STATE_STREAMING : AI_AGENT_STATE_IDLE);
}

void AIAgentSession::apply_runtime_result_for_test(const AIAgentRuntimeResult &p_result) {
	_apply_runtime_result(p_result);
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
