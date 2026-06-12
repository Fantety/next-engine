/**************************************************************************/
/*  ai_agent_v1_ui_adapter.h                                              */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/session/service/ai_session_service.h"

#include "core/object/ref_counted.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class AIAgentV1UIAdapter : public RefCounted {
	GDCLASS(AIAgentV1UIAdapter, RefCounted);

	Ref<AISessionService> session_service;
	String active_session_id;
	String project_scope_id;
	String project_scope_directory;
	HashSet<String> emitted_permission_requests;
	HashSet<String> queued_message_session_ids;
	HashSet<String> queued_run_state_session_ids;
	Vector<String> queued_message_sessions;
	Vector<String> queued_run_state_sessions;
	bool message_flush_queued = false;
	bool run_state_flush_queued = false;

	void _ensure_defaults();
	void _wire_service_signals();

	static Dictionary _make_error_result(const String &p_message, const Dictionary &p_details = Dictionary());
	static String _message_role(const AISessionMessage &p_message);
	static Array _files_to_ui_attachments(const Vector<AIFileAttachment> &p_files);
	static Array _references_to_ui_array(const Vector<AIPromptReference> &p_references);
	static Dictionary _input_to_ui_message(const AISessionInput &p_input);
	static Dictionary _base_message_metadata(const AISessionMessage &p_message);
	static String _variant_to_display_text(const Variant &p_value);
	static Dictionary _tool_content_to_ui_message(const AISessionMessage &p_message, const AIAssistantContent &p_content);
	static void _append_ui_messages_from_session_message(const AISessionMessage &p_message, Array &r_messages);
	static Dictionary _permission_request_to_view(const Dictionary &p_request);

	String _resolve_session_id(const String &p_session_id = String()) const;
	String _get_effective_project_scope_id() const;
	String _get_effective_project_scope_directory() const;
	static String _session_workspace_id(const Dictionary &p_session);
	static String _session_directory(const Dictionary &p_session);
	bool _session_matches_project_scope(const Dictionary &p_session) const;
	Dictionary _with_project_scope_defaults(const Dictionary &p_options) const;
	int64_t _next_ui_last_active_seq() const;
	String _select_restorable_session_id() const;
	Dictionary _touch_session_as_active(const String &p_session_id);
	Array _project_and_get_messages(const String &p_session_id);
	Dictionary _build_run_state(const String &p_session_id) const;
	void _emit_error(const Dictionary &p_error_result);
	void _emit_messages_changed(const String &p_session_id);
	void _emit_run_state_changed(const String &p_session_id);
	void _queue_messages_changed(const String &p_session_id);
	void _queue_run_state_changed(const String &p_session_id);
	void _flush_queued_messages_changed();
	void _flush_queued_run_state_changed();
	void _project_live_event_for_ui(const Dictionary &p_event);
	Dictionary _apply_selected_model_profile(const String &p_model_id, const String &p_agent_id);

	void _permission_asked(const Dictionary &p_request);
	void _permission_replied(const Dictionary &p_reply);
	void _event_appended(const Dictionary &p_event);
	void _drain_requested(const String &p_session_id, const String &p_run_id, int64_t p_wake_seq);
	void _drain_settled(const String &p_session_id, const String &p_run_id, bool p_interrupted);
	void _interrupt_requested(const String &p_session_id, const String &p_reason);

protected:
	static void _bind_methods();

public:
	AIAgentV1UIAdapter();

	void set_session_service(const Ref<AISessionService> &p_service);
	Ref<AISessionService> get_session_service() const;
	void set_project_scope(const String &p_project_id, const String &p_directory = String());
	String get_project_scope_id() const;
	String get_project_scope_directory() const;

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

	Array get_pending_permissions() const;
	Array refresh_pending_permissions();
	Dictionary reply_permission(const String &p_request_id, bool p_allowed, const String &p_reason = String(), const Dictionary &p_options = Dictionary());
};
