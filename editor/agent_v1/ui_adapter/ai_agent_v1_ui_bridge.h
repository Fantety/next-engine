/**************************************************************************/
/*  ai_agent_v1_ui_bridge.h                                               */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/runtime/ai_llm_runtime_registry.h"
#include "editor/agent_v1/session/service/ai_session_service.h"
#include "editor/agent_v1/tools/editor/ai_change_set_store.h"
#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_adapter.h"
#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_config_adapter.h"

#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class AIAgentV1UIBridge : public RefCounted {
	GDCLASS(AIAgentV1UIBridge, RefCounted);

	static inline Ref<AIAgentV1UIBridge> singleton;

	Ref<AISessionService> session_service;
	Ref<AIConfigService> config_service;
	Ref<AILLMRuntimeRegistry> runtime_registry;
	Ref<AIV1ToolRegistry> tool_registry;
	Ref<AIPermissionService> permission_service;
	Ref<AIV1MCPService> mcp_service;
	Ref<AIV1SkillService> skill_service;
	Ref<AIAgentService> agent_service;
	Ref<AIChangeSetStore> change_set_store;

	Ref<AIAgentV1UIAdapter> conversation_adapter;
	Ref<AIAgentV1UIConfigAdapter> config_adapter;

	static String _default_project_storage_root();
	static String _current_project_scope_id();
	static String _current_project_scope_directory();
	void _ensure_backend_services();
	void _wire_backend_services();
	void _ensure_adapters();
	void _wire_adapter_signals();
	void _sync_project_scope();
	void _sync_adapters();

	void _sessions_changed(const Array &p_sessions);
	void _active_session_changed(const Dictionary &p_session);
	void _messages_changed(const String &p_session_id, const Array &p_messages);
	void _run_state_changed(const Dictionary &p_state);
	void _permission_requested(const Dictionary &p_request);
	void _permission_resolved(const Dictionary &p_reply);
	void _config_changed(const String &p_scope, const Dictionary &p_config);
	void _models_changed(const Array &p_models);
	void _mcp_status_changed(const Array &p_statuses, const Dictionary &p_summary);
	void _skill_status_changed(const Array &p_statuses, const Dictionary &p_summary);
	void _rules_changed(const Array &p_rules);
	void _marquee_changed(const Array &p_marquees, const String &p_active_id);
	void _change_sets_changed();
	void _error_reported(const Dictionary &p_error);

protected:
	static void _bind_methods();

public:
	static Ref<AIAgentV1UIBridge> get_singleton();
	static void clear_singleton_for_test();

	AIAgentV1UIBridge();

	void set_session_service(const Ref<AISessionService> &p_service);
	Ref<AISessionService> get_session_service() const;
	void set_config_service(const Ref<AIConfigService> &p_service);
	Ref<AIConfigService> get_config_service() const;
	void set_runtime_registry(const Ref<AILLMRuntimeRegistry> &p_registry);
	Ref<AILLMRuntimeRegistry> get_runtime_registry() const;
	void set_tool_registry(const Ref<AIV1ToolRegistry> &p_registry);
	Ref<AIV1ToolRegistry> get_tool_registry() const;
	void set_permission_service(const Ref<AIPermissionService> &p_service);
	Ref<AIPermissionService> get_permission_service() const;
	void set_mcp_service(const Ref<AIV1MCPService> &p_service);
	Ref<AIV1MCPService> get_mcp_service() const;
	void set_skill_service(const Ref<AIV1SkillService> &p_service);
	Ref<AIV1SkillService> get_skill_service() const;
	void set_agent_service(const Ref<AIAgentService> &p_service);
	Ref<AIAgentService> get_agent_service() const;

	Ref<AIAgentV1UIAdapter> get_conversation_adapter() const;
	Ref<AIAgentV1UIConfigAdapter> get_config_adapter() const;

	Dictionary get_settings_snapshot();
	Array list_models();
	Array list_model_provider_presets();
	Array list_model_profiles(bool p_enabled_only = true);
	Dictionary get_model_profile(const String &p_profile_id);
	Dictionary add_model_profile(const Dictionary &p_profile, const String &p_scope = "project");
	Dictionary update_model_profile(const String &p_profile_id, const Dictionary &p_profile, const String &p_scope = "project");
	Dictionary remove_model_profile(const String &p_profile_id, const String &p_scope = "project");
	Array list_agents();
	Dictionary patch_settings(const Dictionary &p_patch, const String &p_scope = "project");
	Dictionary refresh_mcp_status();
	Array list_change_sets(const String &p_status = "pending");
	Dictionary get_change_set(const String &p_change_set_id);
	int get_pending_change_set_count();
	Dictionary keep_change_set(const String &p_change_set_id);
	Dictionary revert_change_set(const String &p_change_set_id);

	Dictionary create_session(const Dictionary &p_options = Dictionary());
	Array list_sessions();
	bool restore_active_session();
	bool set_active_session(const String &p_session_id);
	Dictionary archive_session(const String &p_session_id);
	Dictionary delete_session(const String &p_session_id);
	String get_active_session_id() const;
	Dictionary get_active_session();
	Array get_messages(const String &p_session_id = String());
	Dictionary send_message(const String &p_text, const String &p_model_id = String(), const String &p_agent_id = String(), const Array &p_attachments = Array(), bool p_resume = true);
	Dictionary cancel_active_run(const String &p_reason = String());
	Dictionary get_run_state(const String &p_session_id = String()) const;
	Dictionary reply_permission(const String &p_request_id, bool p_allowed, const String &p_reason = String(), const Dictionary &p_options = Dictionary());
};
