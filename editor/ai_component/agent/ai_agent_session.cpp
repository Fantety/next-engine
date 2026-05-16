/**************************************************************************/
/*  ai_agent_session.cpp                                                   */
/**************************************************************************/

#include "ai_agent_session.h"

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
	ClassDB::bind_method(D_METHOD("get_messages_as_array"), &AIAgentSession::get_messages_as_array);
	ClassDB::bind_method(D_METHOD("set_agent_profile_id", "profile_id"), &AIAgentSession::set_agent_profile_id);
	ClassDB::bind_method(D_METHOD("get_agent_profile_id"), &AIAgentSession::get_agent_profile_id);
	ClassDB::bind_method(D_METHOD("is_tool_runtime_available"), &AIAgentSession::is_tool_runtime_available);

	ADD_SIGNAL(MethodInfo("message_added", PropertyInfo(Variant::DICTIONARY, "message")));
	ADD_SIGNAL(MethodInfo("message_updated", PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::DICTIONARY, "message")));
	ADD_SIGNAL(MethodInfo("state_changed", PropertyInfo(Variant::INT, "state")));
	ADD_SIGNAL(MethodInfo("request_failed", PropertyInfo(Variant::STRING, "message")));
}

AIAgentSession::AIAgentSession() {
	store.instantiate();
	runner.instantiate();
	runtime.instantiate();
	runtime_runner.instantiate();
	tool_registry.instantiate();
	project_tree_context.instantiate();
	editor_context.instantiate();
	agent_profile = AIAgentProfile::get_plan_profile();

	provider = memnew(AIOpenAICompatibleProvider);
	add_child(provider);
	runner->set_provider(provider);
	_configure_tool_runtime();

	provider->connect("response_started", callable_mp(this, &AIAgentSession::_on_provider_response_started), CONNECT_DEFERRED);
	provider->connect("response_delta", callable_mp(this, &AIAgentSession::_on_provider_response_delta), CONNECT_DEFERRED);
	provider->connect("response_finished", callable_mp(this, &AIAgentSession::_on_provider_response_finished), CONNECT_DEFERRED);
	provider->connect("request_failed", callable_mp(this, &AIAgentSession::_on_provider_request_failed), CONNECT_DEFERRED);

	start_new_session();
}

void AIAgentSession::configure_provider(const AIProviderConfig &p_config) {
	provider->set_config(p_config);
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
	return provider != nullptr && provider->get_features().supports_tools && runtime.is_valid() && tool_registry.is_valid();
}

void AIAgentSession::send_user_message(const String &p_message) {
	String stripped = p_message.strip_edges();
	if (stripped.is_empty() || state == AI_AGENT_STATE_STREAMING || state == AI_AGENT_STATE_PREPARING_CONTEXT) {
		return;
	}

	if (messages.is_empty()) {
		title = stripped.substr(0, 80);
	}

	AIAgentMessage user_message;
	user_message.role = AI_AGENT_ROLE_USER;
	user_message.content = stripped;
	user_message.created_at = Time::get_singleton()->get_unix_time_from_system();
	messages.push_back(user_message);
	emit_signal(SNAME("message_added"), user_message.to_dict());
	_save();

	AIAgentMessage assistant_message;
	assistant_message.role = AI_AGENT_ROLE_ASSISTANT;
	assistant_message.created_at = Time::get_singleton()->get_unix_time_from_system();
	messages.push_back(assistant_message);
	active_assistant_index = messages.size() - 1;
	emit_signal(SNAME("message_added"), assistant_message.to_dict());

	_set_state(AI_AGENT_STATE_PREPARING_CONTEXT);
	Array context = _collect_context();
	_set_state(AI_AGENT_STATE_STREAMING);
	if (!runner->start(messages, context)) {
		_on_provider_request_failed("Failed to start AI request.");
	}
}

void AIAgentSession::cancel_request() {
	if (state == AI_AGENT_STATE_STREAMING || state == AI_AGENT_STATE_PREPARING_CONTEXT) {
		runner->cancel();
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
}

bool AIAgentSession::load_session(const String &p_session_id) {
	if (state == AI_AGENT_STATE_STREAMING || state == AI_AGENT_STATE_PREPARING_CONTEXT) {
		cancel_request();
	}

	String loaded_title;
	Vector<AIAgentMessage> loaded_messages;
	if (!store->load_conversation(p_session_id, loaded_title, loaded_messages)) {
		return false;
	}

	session_id = p_session_id;
	title = loaded_title.is_empty() ? String("New Chat") : loaded_title;
	messages = loaded_messages;
	active_assistant_index = -1;
	_set_state(AI_AGENT_STATE_IDLE);
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

void AIAgentSession::_set_state(AIAgentState p_state) {
	state = p_state;
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

void AIAgentSession::_configure_tool_runtime() {
	Ref<AIListProjectTool> list_project;
	list_project.instantiate();
	tool_registry->register_tool(list_project);

	Ref<AIReadFileTool> read_file;
	read_file.instantiate();
	tool_registry->register_tool(read_file);

	Ref<AISearchProjectTool> search_project;
	search_project.instantiate();
	tool_registry->register_tool(search_project);

	Ref<AIGetEditorContextTool> editor_context_tool;
	editor_context_tool.instantiate();
	tool_registry->register_tool(editor_context_tool);

	runtime->set_tool_registry(tool_registry);
	runtime->set_profile(agent_profile);
	runtime_runner->set_runtime(runtime);
}

void AIAgentSession::_on_provider_response_started() {
}

void AIAgentSession::_on_provider_response_delta(const String &p_delta) {
	if (active_assistant_index < 0 || active_assistant_index >= messages.size()) {
		return;
	}
	messages.write[active_assistant_index].content += p_delta;
	emit_signal(SNAME("message_updated"), active_assistant_index, messages[active_assistant_index].to_dict());
}

void AIAgentSession::_on_provider_response_finished(const String &p_finish_reason) {
	if (p_finish_reason == "cancelled") {
		_set_state(AI_AGENT_STATE_CANCELLED);
	} else {
		_set_state(AI_AGENT_STATE_IDLE);
	}
	active_assistant_index = -1;
	_save();
}

void AIAgentSession::_on_provider_request_failed(const String &p_message) {
	if (active_assistant_index >= 0 && active_assistant_index < messages.size() && messages[active_assistant_index].content.is_empty()) {
		messages.remove_at(active_assistant_index);
		active_assistant_index = -1;
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
