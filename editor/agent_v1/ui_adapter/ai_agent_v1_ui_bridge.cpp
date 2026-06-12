/**************************************************************************/
/*  ai_agent_v1_ui_bridge.cpp                                             */
/**************************************************************************/

#include "ai_agent_v1_ui_bridge.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

void AIAgentV1UIBridge::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_session_service", "service"), &AIAgentV1UIBridge::set_session_service);
	ClassDB::bind_method(D_METHOD("get_session_service"), &AIAgentV1UIBridge::get_session_service);
	ClassDB::bind_method(D_METHOD("set_config_service", "service"), &AIAgentV1UIBridge::set_config_service);
	ClassDB::bind_method(D_METHOD("get_config_service"), &AIAgentV1UIBridge::get_config_service);
	ClassDB::bind_method(D_METHOD("set_runtime_registry", "registry"), &AIAgentV1UIBridge::set_runtime_registry);
	ClassDB::bind_method(D_METHOD("get_runtime_registry"), &AIAgentV1UIBridge::get_runtime_registry);
	ClassDB::bind_method(D_METHOD("set_tool_registry", "registry"), &AIAgentV1UIBridge::set_tool_registry);
	ClassDB::bind_method(D_METHOD("get_tool_registry"), &AIAgentV1UIBridge::get_tool_registry);
	ClassDB::bind_method(D_METHOD("set_permission_service", "service"), &AIAgentV1UIBridge::set_permission_service);
	ClassDB::bind_method(D_METHOD("get_permission_service"), &AIAgentV1UIBridge::get_permission_service);
	ClassDB::bind_method(D_METHOD("set_mcp_service", "service"), &AIAgentV1UIBridge::set_mcp_service);
	ClassDB::bind_method(D_METHOD("get_mcp_service"), &AIAgentV1UIBridge::get_mcp_service);
	ClassDB::bind_method(D_METHOD("set_skill_service", "service"), &AIAgentV1UIBridge::set_skill_service);
	ClassDB::bind_method(D_METHOD("get_skill_service"), &AIAgentV1UIBridge::get_skill_service);
	ClassDB::bind_method(D_METHOD("set_agent_service", "service"), &AIAgentV1UIBridge::set_agent_service);
	ClassDB::bind_method(D_METHOD("get_agent_service"), &AIAgentV1UIBridge::get_agent_service);
	ClassDB::bind_method(D_METHOD("get_conversation_adapter"), &AIAgentV1UIBridge::get_conversation_adapter);
	ClassDB::bind_method(D_METHOD("get_config_adapter"), &AIAgentV1UIBridge::get_config_adapter);

	ClassDB::bind_method(D_METHOD("get_settings_snapshot"), &AIAgentV1UIBridge::get_settings_snapshot);
	ClassDB::bind_method(D_METHOD("list_models"), &AIAgentV1UIBridge::list_models);
	ClassDB::bind_method(D_METHOD("list_model_provider_presets"), &AIAgentV1UIBridge::list_model_provider_presets);
	ClassDB::bind_method(D_METHOD("list_model_profiles", "enabled_only"), &AIAgentV1UIBridge::list_model_profiles, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_model_profile", "profile_id"), &AIAgentV1UIBridge::get_model_profile);
	ClassDB::bind_method(D_METHOD("add_model_profile", "profile", "scope"), &AIAgentV1UIBridge::add_model_profile, DEFVAL("project"));
	ClassDB::bind_method(D_METHOD("update_model_profile", "profile_id", "profile", "scope"), &AIAgentV1UIBridge::update_model_profile, DEFVAL("project"));
	ClassDB::bind_method(D_METHOD("remove_model_profile", "profile_id", "scope"), &AIAgentV1UIBridge::remove_model_profile, DEFVAL("project"));
	ClassDB::bind_method(D_METHOD("list_agents"), &AIAgentV1UIBridge::list_agents);
	ClassDB::bind_method(D_METHOD("patch_settings", "patch", "scope"), &AIAgentV1UIBridge::patch_settings, DEFVAL("project"));
	ClassDB::bind_method(D_METHOD("refresh_mcp_status"), &AIAgentV1UIBridge::refresh_mcp_status);
	ClassDB::bind_method(D_METHOD("list_change_sets", "status"), &AIAgentV1UIBridge::list_change_sets, DEFVAL("pending"));
	ClassDB::bind_method(D_METHOD("get_change_set", "change_set_id"), &AIAgentV1UIBridge::get_change_set);
	ClassDB::bind_method(D_METHOD("get_pending_change_set_count"), &AIAgentV1UIBridge::get_pending_change_set_count);
	ClassDB::bind_method(D_METHOD("keep_change_set", "change_set_id"), &AIAgentV1UIBridge::keep_change_set);
	ClassDB::bind_method(D_METHOD("revert_change_set", "change_set_id"), &AIAgentV1UIBridge::revert_change_set);

	ClassDB::bind_method(D_METHOD("create_session", "options"), &AIAgentV1UIBridge::create_session, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("list_sessions"), &AIAgentV1UIBridge::list_sessions);
	ClassDB::bind_method(D_METHOD("restore_active_session"), &AIAgentV1UIBridge::restore_active_session);
	ClassDB::bind_method(D_METHOD("set_active_session", "session_id"), &AIAgentV1UIBridge::set_active_session);
	ClassDB::bind_method(D_METHOD("archive_session", "session_id"), &AIAgentV1UIBridge::archive_session);
	ClassDB::bind_method(D_METHOD("delete_session", "session_id"), &AIAgentV1UIBridge::delete_session);
	ClassDB::bind_method(D_METHOD("get_active_session_id"), &AIAgentV1UIBridge::get_active_session_id);
	ClassDB::bind_method(D_METHOD("get_active_session"), &AIAgentV1UIBridge::get_active_session);
	ClassDB::bind_method(D_METHOD("get_messages", "session_id"), &AIAgentV1UIBridge::get_messages, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("send_message", "text", "model_id", "agent_id", "attachments", "resume"), &AIAgentV1UIBridge::send_message, DEFVAL(String()), DEFVAL(String()), DEFVAL(Array()), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("cancel_active_run", "reason"), &AIAgentV1UIBridge::cancel_active_run, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_run_state", "session_id"), &AIAgentV1UIBridge::get_run_state, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("reply_permission", "request_id", "allowed", "reason", "options"), &AIAgentV1UIBridge::reply_permission, DEFVAL(String()), DEFVAL(Dictionary()));

	ADD_SIGNAL(MethodInfo("sessions_changed", PropertyInfo(Variant::ARRAY, "sessions")));
	ADD_SIGNAL(MethodInfo("active_session_changed", PropertyInfo(Variant::DICTIONARY, "session")));
	ADD_SIGNAL(MethodInfo("messages_changed", PropertyInfo(Variant::STRING, "session_id"), PropertyInfo(Variant::ARRAY, "messages")));
	ADD_SIGNAL(MethodInfo("run_state_changed", PropertyInfo(Variant::DICTIONARY, "state")));
	ADD_SIGNAL(MethodInfo("permission_requested", PropertyInfo(Variant::DICTIONARY, "request")));
	ADD_SIGNAL(MethodInfo("permission_resolved", PropertyInfo(Variant::DICTIONARY, "reply")));
	ADD_SIGNAL(MethodInfo("config_changed", PropertyInfo(Variant::STRING, "scope"), PropertyInfo(Variant::DICTIONARY, "config")));
	ADD_SIGNAL(MethodInfo("models_changed", PropertyInfo(Variant::ARRAY, "models")));
	ADD_SIGNAL(MethodInfo("mcp_status_changed", PropertyInfo(Variant::ARRAY, "statuses"), PropertyInfo(Variant::DICTIONARY, "summary")));
	ADD_SIGNAL(MethodInfo("skill_status_changed", PropertyInfo(Variant::ARRAY, "statuses"), PropertyInfo(Variant::DICTIONARY, "summary")));
	ADD_SIGNAL(MethodInfo("rules_changed", PropertyInfo(Variant::ARRAY, "rules")));
	ADD_SIGNAL(MethodInfo("marquee_changed", PropertyInfo(Variant::ARRAY, "marquees"), PropertyInfo(Variant::STRING, "active_id")));
	ADD_SIGNAL(MethodInfo("change_sets_changed"));
	ADD_SIGNAL(MethodInfo("error_reported", PropertyInfo(Variant::DICTIONARY, "error")));
}

Ref<AIAgentV1UIBridge> AIAgentV1UIBridge::get_singleton() {
	if (singleton.is_null()) {
		singleton.instantiate();
	}
	return singleton;
}

void AIAgentV1UIBridge::clear_singleton_for_test() {
	singleton.unref();
	AIChangeSetStore::clear_singleton_for_test();
}

AIAgentV1UIBridge::AIAgentV1UIBridge() {
	_sync_adapters();
}

void AIAgentV1UIBridge::_ensure_backend_services() {
	if (session_service.is_null()) {
		session_service.instantiate();
	}
	if (config_service.is_null()) {
		config_service = session_service->get_config_service();
		if (config_service.is_null()) {
			config_service.instantiate();
		}
	}
	if (runtime_registry.is_null()) {
		runtime_registry = session_service->get_runtime_registry();
		if (runtime_registry.is_null()) {
			runtime_registry.instantiate();
		}
	}
	if (tool_registry.is_null()) {
		tool_registry = session_service->get_tool_registry();
		if (tool_registry.is_null()) {
			tool_registry.instantiate();
			tool_registry->register_builtin_tools();
		}
	}
	if (permission_service.is_null()) {
		permission_service = session_service->get_permission_service();
		if (permission_service.is_null()) {
			permission_service.instantiate();
		}
	}
	if (skill_service.is_null()) {
		skill_service = session_service->get_skill_service();
		if (skill_service.is_null()) {
			skill_service.instantiate();
		}
	}
	if (agent_service.is_null()) {
		agent_service = session_service->get_agent_service();
		if (agent_service.is_null()) {
			agent_service.instantiate();
		}
	}
	if (mcp_service.is_null()) {
		mcp_service.instantiate();
	}
	if (change_set_store.is_null()) {
		change_set_store = AIChangeSetStore::get_singleton();
	}
}

void AIAgentV1UIBridge::_wire_backend_services() {
	if (session_service.is_valid()) {
		session_service->set_config_service(config_service);
		session_service->set_runtime_registry(runtime_registry);
		session_service->set_tool_registry(tool_registry);
		session_service->set_permission_service(permission_service);
		session_service->set_skill_service(skill_service);
		session_service->set_agent_service(agent_service);
	}
	if (agent_service.is_valid()) {
		agent_service->set_config_service(config_service);
		if (session_service.is_valid()) {
			agent_service->set_session_store(session_service->get_session_store());
		}
	}
	if (skill_service.is_valid()) {
		skill_service->set_tool_registry(tool_registry);
	}
	if (mcp_service.is_valid()) {
		mcp_service->set_tool_registry(tool_registry);
	}
}

void AIAgentV1UIBridge::_ensure_adapters() {
	if (conversation_adapter.is_null()) {
		conversation_adapter.instantiate();
	}
	if (config_adapter.is_null()) {
		config_adapter.instantiate();
	}
}

void AIAgentV1UIBridge::_wire_adapter_signals() {
	const Callable sessions_changed = callable_mp(this, &AIAgentV1UIBridge::_sessions_changed);
	if (!conversation_adapter->is_connected(SNAME("sessions_changed"), sessions_changed)) {
		conversation_adapter->connect(SNAME("sessions_changed"), sessions_changed);
	}

	const Callable active_session_changed = callable_mp(this, &AIAgentV1UIBridge::_active_session_changed);
	if (!conversation_adapter->is_connected(SNAME("active_session_changed"), active_session_changed)) {
		conversation_adapter->connect(SNAME("active_session_changed"), active_session_changed);
	}

	const Callable messages_changed = callable_mp(this, &AIAgentV1UIBridge::_messages_changed);
	if (!conversation_adapter->is_connected(SNAME("messages_changed"), messages_changed)) {
		conversation_adapter->connect(SNAME("messages_changed"), messages_changed);
	}

	const Callable run_state_changed = callable_mp(this, &AIAgentV1UIBridge::_run_state_changed);
	if (!conversation_adapter->is_connected(SNAME("run_state_changed"), run_state_changed)) {
		conversation_adapter->connect(SNAME("run_state_changed"), run_state_changed);
	}

	const Callable permission_requested = callable_mp(this, &AIAgentV1UIBridge::_permission_requested);
	if (!conversation_adapter->is_connected(SNAME("permission_requested"), permission_requested)) {
		conversation_adapter->connect(SNAME("permission_requested"), permission_requested);
	}

	const Callable permission_resolved = callable_mp(this, &AIAgentV1UIBridge::_permission_resolved);
	if (!conversation_adapter->is_connected(SNAME("permission_resolved"), permission_resolved)) {
		conversation_adapter->connect(SNAME("permission_resolved"), permission_resolved);
	}

	const Callable config_changed = callable_mp(this, &AIAgentV1UIBridge::_config_changed);
	if (!config_adapter->is_connected(SNAME("config_changed"), config_changed)) {
		config_adapter->connect(SNAME("config_changed"), config_changed);
	}

	const Callable models_changed = callable_mp(this, &AIAgentV1UIBridge::_models_changed);
	if (!config_adapter->is_connected(SNAME("models_changed"), models_changed)) {
		config_adapter->connect(SNAME("models_changed"), models_changed);
	}

	const Callable mcp_status_changed = callable_mp(this, &AIAgentV1UIBridge::_mcp_status_changed);
	if (!config_adapter->is_connected(SNAME("mcp_status_changed"), mcp_status_changed)) {
		config_adapter->connect(SNAME("mcp_status_changed"), mcp_status_changed);
	}

	const Callable skill_status_changed = callable_mp(this, &AIAgentV1UIBridge::_skill_status_changed);
	if (!config_adapter->is_connected(SNAME("skill_status_changed"), skill_status_changed)) {
		config_adapter->connect(SNAME("skill_status_changed"), skill_status_changed);
	}

	const Callable rules_changed = callable_mp(this, &AIAgentV1UIBridge::_rules_changed);
	if (!config_adapter->is_connected(SNAME("rules_changed"), rules_changed)) {
		config_adapter->connect(SNAME("rules_changed"), rules_changed);
	}

	const Callable marquee_changed = callable_mp(this, &AIAgentV1UIBridge::_marquee_changed);
	if (!config_adapter->is_connected(SNAME("marquee_changed"), marquee_changed)) {
		config_adapter->connect(SNAME("marquee_changed"), marquee_changed);
	}

	if (change_set_store.is_valid()) {
		const Callable change_sets_changed = callable_mp(this, &AIAgentV1UIBridge::_change_sets_changed);
		if (!change_set_store->is_connected(SNAME("changed"), change_sets_changed)) {
			change_set_store->connect(SNAME("changed"), change_sets_changed, CONNECT_DEFERRED);
		}
	}

	const Callable error_reported = callable_mp(this, &AIAgentV1UIBridge::_error_reported);
	if (!conversation_adapter->is_connected(SNAME("error_reported"), error_reported)) {
		conversation_adapter->connect(SNAME("error_reported"), error_reported);
	}
	if (!config_adapter->is_connected(SNAME("error_reported"), error_reported)) {
		config_adapter->connect(SNAME("error_reported"), error_reported);
	}
}

void AIAgentV1UIBridge::_sync_adapters() {
	_ensure_backend_services();
	_wire_backend_services();
	_ensure_adapters();

	conversation_adapter->set_session_service(session_service);
	config_adapter->set_config_service(config_service);
	config_adapter->set_mcp_service(mcp_service);
	config_adapter->set_skill_service(skill_service);
	config_adapter->set_agent_service(agent_service);
	_wire_adapter_signals();
}

void AIAgentV1UIBridge::_sessions_changed(const Array &p_sessions) {
	emit_signal(SNAME("sessions_changed"), p_sessions.duplicate(true));
}

void AIAgentV1UIBridge::_active_session_changed(const Dictionary &p_session) {
	emit_signal(SNAME("active_session_changed"), p_session.duplicate(true));
}

void AIAgentV1UIBridge::_messages_changed(const String &p_session_id, const Array &p_messages) {
	emit_signal(SNAME("messages_changed"), p_session_id, p_messages.duplicate(true));
}

void AIAgentV1UIBridge::_run_state_changed(const Dictionary &p_state) {
	emit_signal(SNAME("run_state_changed"), p_state.duplicate(true));
}

void AIAgentV1UIBridge::_permission_requested(const Dictionary &p_request) {
	emit_signal(SNAME("permission_requested"), p_request.duplicate(true));
}

void AIAgentV1UIBridge::_permission_resolved(const Dictionary &p_reply) {
	emit_signal(SNAME("permission_resolved"), p_reply.duplicate(true));
}

void AIAgentV1UIBridge::_config_changed(const String &p_scope, const Dictionary &p_config) {
	emit_signal(SNAME("config_changed"), p_scope, p_config.duplicate(true));
}

void AIAgentV1UIBridge::_models_changed(const Array &p_models) {
	emit_signal(SNAME("models_changed"), p_models.duplicate(true));
}

void AIAgentV1UIBridge::_mcp_status_changed(const Array &p_statuses, const Dictionary &p_summary) {
	emit_signal(SNAME("mcp_status_changed"), p_statuses.duplicate(true), p_summary.duplicate(true));
}

void AIAgentV1UIBridge::_skill_status_changed(const Array &p_statuses, const Dictionary &p_summary) {
	emit_signal(SNAME("skill_status_changed"), p_statuses.duplicate(true), p_summary.duplicate(true));
}

void AIAgentV1UIBridge::_rules_changed(const Array &p_rules) {
	emit_signal(SNAME("rules_changed"), p_rules.duplicate(true));
}

void AIAgentV1UIBridge::_marquee_changed(const Array &p_marquees, const String &p_active_id) {
	emit_signal(SNAME("marquee_changed"), p_marquees.duplicate(true), p_active_id);
}

void AIAgentV1UIBridge::_change_sets_changed() {
	emit_signal(SNAME("change_sets_changed"));
}

void AIAgentV1UIBridge::_error_reported(const Dictionary &p_error) {
	emit_signal(SNAME("error_reported"), p_error.duplicate(true));
}

void AIAgentV1UIBridge::set_session_service(const Ref<AISessionService> &p_service) {
	session_service = p_service;
	if (session_service.is_valid()) {
		if (config_service.is_null()) {
			config_service = session_service->get_config_service();
		}
		if (runtime_registry.is_null()) {
			runtime_registry = session_service->get_runtime_registry();
		}
		if (tool_registry.is_null()) {
			tool_registry = session_service->get_tool_registry();
		}
		if (permission_service.is_null()) {
			permission_service = session_service->get_permission_service();
		}
		if (skill_service.is_null()) {
			skill_service = session_service->get_skill_service();
		}
		if (agent_service.is_null()) {
			agent_service = session_service->get_agent_service();
		}
	}
	_sync_adapters();
}

Ref<AISessionService> AIAgentV1UIBridge::get_session_service() const {
	return session_service;
}

void AIAgentV1UIBridge::set_config_service(const Ref<AIConfigService> &p_service) {
	config_service = p_service;
	_sync_adapters();
}

Ref<AIConfigService> AIAgentV1UIBridge::get_config_service() const {
	return config_service;
}

void AIAgentV1UIBridge::set_runtime_registry(const Ref<AILLMRuntimeRegistry> &p_registry) {
	runtime_registry = p_registry;
	_sync_adapters();
}

Ref<AILLMRuntimeRegistry> AIAgentV1UIBridge::get_runtime_registry() const {
	return runtime_registry;
}

void AIAgentV1UIBridge::set_tool_registry(const Ref<AIV1ToolRegistry> &p_registry) {
	tool_registry = p_registry;
	_sync_adapters();
}

Ref<AIV1ToolRegistry> AIAgentV1UIBridge::get_tool_registry() const {
	return tool_registry;
}

void AIAgentV1UIBridge::set_permission_service(const Ref<AIPermissionService> &p_service) {
	permission_service = p_service;
	_sync_adapters();
}

Ref<AIPermissionService> AIAgentV1UIBridge::get_permission_service() const {
	return permission_service;
}

void AIAgentV1UIBridge::set_mcp_service(const Ref<AIV1MCPService> &p_service) {
	mcp_service = p_service;
	_sync_adapters();
}

Ref<AIV1MCPService> AIAgentV1UIBridge::get_mcp_service() const {
	return mcp_service;
}

void AIAgentV1UIBridge::set_skill_service(const Ref<AIV1SkillService> &p_service) {
	skill_service = p_service;
	_sync_adapters();
}

Ref<AIV1SkillService> AIAgentV1UIBridge::get_skill_service() const {
	return skill_service;
}

void AIAgentV1UIBridge::set_agent_service(const Ref<AIAgentService> &p_service) {
	agent_service = p_service;
	_sync_adapters();
}

Ref<AIAgentService> AIAgentV1UIBridge::get_agent_service() const {
	return agent_service;
}

Ref<AIAgentV1UIAdapter> AIAgentV1UIBridge::get_conversation_adapter() const {
	return conversation_adapter;
}

Ref<AIAgentV1UIConfigAdapter> AIAgentV1UIBridge::get_config_adapter() const {
	return config_adapter;
}

Dictionary AIAgentV1UIBridge::get_settings_snapshot() {
	_sync_adapters();
	return config_adapter->get_settings_snapshot();
}

Array AIAgentV1UIBridge::list_models() {
	_sync_adapters();
	return config_adapter->list_models();
}

Array AIAgentV1UIBridge::list_model_provider_presets() {
	_sync_adapters();
	return config_adapter->list_model_provider_presets();
}

Array AIAgentV1UIBridge::list_model_profiles(bool p_enabled_only) {
	_sync_adapters();
	return config_adapter->list_model_profiles(p_enabled_only);
}

Dictionary AIAgentV1UIBridge::get_model_profile(const String &p_profile_id) {
	_sync_adapters();
	return config_adapter->get_model_profile(p_profile_id);
}

Dictionary AIAgentV1UIBridge::add_model_profile(const Dictionary &p_profile, const String &p_scope) {
	_sync_adapters();
	return config_adapter->add_model_profile(p_profile, p_scope);
}

Dictionary AIAgentV1UIBridge::update_model_profile(const String &p_profile_id, const Dictionary &p_profile, const String &p_scope) {
	_sync_adapters();
	return config_adapter->update_model_profile(p_profile_id, p_profile, p_scope);
}

Dictionary AIAgentV1UIBridge::remove_model_profile(const String &p_profile_id, const String &p_scope) {
	_sync_adapters();
	return config_adapter->remove_model_profile(p_profile_id, p_scope);
}

Array AIAgentV1UIBridge::list_agents() {
	_sync_adapters();
	return config_adapter->list_agents();
}

Dictionary AIAgentV1UIBridge::patch_settings(const Dictionary &p_patch, const String &p_scope) {
	_sync_adapters();
	return config_adapter->patch_settings(p_patch, p_scope);
}

Dictionary AIAgentV1UIBridge::refresh_mcp_status() {
	_sync_adapters();
	Dictionary result;
	if (mcp_service.is_null() || config_service.is_null()) {
		result["success"] = false;
		Dictionary error;
		error["kind"] = "unavailable";
		error["message"] = "MCP service is not available.";
		result["error"] = error;
		return result;
	}

	const Dictionary config = config_service->get_config();
	const Dictionary import_result = mcp_service->import_config(config);
	if (!bool(import_result.get("success", false))) {
		return import_result;
	}

	result = mcp_service->refresh();
	const Array statuses = mcp_service->get_statuses();
	const Dictionary summary = mcp_service->get_status_summary();
	result["statuses"] = statuses.duplicate(true);
	result["summary"] = summary.duplicate(true);
	_mcp_status_changed(statuses, summary);
	return result;
}

Array AIAgentV1UIBridge::list_change_sets(const String &p_status) {
	_sync_adapters();
	if (change_set_store.is_null()) {
		return Array();
	}
	return change_set_store->list_change_sets(p_status);
}

Dictionary AIAgentV1UIBridge::get_change_set(const String &p_change_set_id) {
	_sync_adapters();
	if (change_set_store.is_null()) {
		return Dictionary();
	}
	return change_set_store->get_change_set(p_change_set_id);
}

int AIAgentV1UIBridge::get_pending_change_set_count() {
	_sync_adapters();
	if (change_set_store.is_null()) {
		return 0;
	}
	return change_set_store->get_pending_count();
}

Dictionary AIAgentV1UIBridge::keep_change_set(const String &p_change_set_id) {
	_sync_adapters();
	Dictionary result;
	if (change_set_store.is_null()) {
		result["success"] = false;
		Dictionary error;
		error["kind"] = "unavailable";
		error["message"] = "AI change review store is not available.";
		result["error"] = error;
		return result;
	}

	String error;
	const bool ok = change_set_store->keep_change_set(p_change_set_id, error);
	result["success"] = ok;
	if (!ok) {
		Dictionary error_dict;
		error_dict["kind"] = "unavailable";
		error_dict["message"] = error;
		result["error"] = error_dict;
	}
	return result;
}

Dictionary AIAgentV1UIBridge::revert_change_set(const String &p_change_set_id) {
	_sync_adapters();
	Dictionary result;
	if (change_set_store.is_null()) {
		result["success"] = false;
		Dictionary error;
		error["kind"] = "unavailable";
		error["message"] = "AI change review store is not available.";
		result["error"] = error;
		return result;
	}

	String error;
	const bool ok = change_set_store->revert_change_set(p_change_set_id, error);
	result["success"] = ok;
	if (!ok) {
		Dictionary error_dict;
		error_dict["kind"] = "unavailable";
		error_dict["message"] = error;
		result["error"] = error_dict;
	}
	return result;
}

Dictionary AIAgentV1UIBridge::create_session(const Dictionary &p_options) {
	_sync_adapters();
	return conversation_adapter->create_session(p_options);
}

Array AIAgentV1UIBridge::list_sessions() {
	_sync_adapters();
	return conversation_adapter->list_sessions();
}

bool AIAgentV1UIBridge::restore_active_session() {
	_sync_adapters();
	return conversation_adapter->restore_active_session();
}

bool AIAgentV1UIBridge::set_active_session(const String &p_session_id) {
	_sync_adapters();
	return conversation_adapter->set_active_session(p_session_id);
}

Dictionary AIAgentV1UIBridge::archive_session(const String &p_session_id) {
	_sync_adapters();
	return conversation_adapter->archive_session(p_session_id);
}

Dictionary AIAgentV1UIBridge::delete_session(const String &p_session_id) {
	_sync_adapters();
	return conversation_adapter->delete_session(p_session_id);
}

String AIAgentV1UIBridge::get_active_session_id() const {
	return conversation_adapter.is_valid() ? conversation_adapter->get_active_session_id() : String();
}

Dictionary AIAgentV1UIBridge::get_active_session() {
	_sync_adapters();
	return conversation_adapter->get_active_session();
}

Array AIAgentV1UIBridge::get_messages(const String &p_session_id) {
	_sync_adapters();
	return conversation_adapter->get_messages(p_session_id);
}

Dictionary AIAgentV1UIBridge::send_message(const String &p_text, const String &p_model_id, const String &p_agent_id, const Array &p_attachments, bool p_resume) {
	_sync_adapters();
	return conversation_adapter->send_message(p_text, p_model_id, p_agent_id, p_attachments, p_resume);
}

Dictionary AIAgentV1UIBridge::cancel_active_run(const String &p_reason) {
	_sync_adapters();
	return conversation_adapter->cancel_active_run(p_reason);
}

Dictionary AIAgentV1UIBridge::get_run_state(const String &p_session_id) const {
	if (conversation_adapter.is_null()) {
		return Dictionary();
	}
	return conversation_adapter->get_run_state(p_session_id);
}

Dictionary AIAgentV1UIBridge::reply_permission(const String &p_request_id, bool p_allowed, const String &p_reason, const Dictionary &p_options) {
	_sync_adapters();
	return conversation_adapter->reply_permission(p_request_id, p_allowed, p_reason, p_options);
}
