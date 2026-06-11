/**************************************************************************/
/*  ai_permission_service.h                                                */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_error.h"
#include "editor/agent_v1/domain/events/ai_event_store.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

struct AIPermissionDecision {
	bool allowed = false;
	bool pending = false;
	bool denied = false;
	String request_id;
	String session_id;
	String action;
	String resource;
	String effect;
	String reply;
	String reason;
	Dictionary source;
	AIError error;

	Dictionary to_dictionary() const;
};

class AIPermissionService : public RefCounted {
	GDCLASS(AIPermissionService, RefCounted);

	struct PendingRequest {
		String request_id;
		String session_id;
		String action;
		String resource;
		String effect = "ask";
		String status = "pending";
		String reason;
		Dictionary source;

		Dictionary to_dictionary() const;
	};

	Ref<AIEventStore> event_store;
	Array rules;
	HashMap<String, PendingRequest> pending_requests;
	HashMap<String, bool> saved_approvals;
	mutable Mutex mutex;

	static String _normalize_effect(const String &p_effect);
	static String _normalize_reply(const String &p_reply);
	static String _request_key(const String &p_session_id, const String &p_action, const String &p_resource, const Dictionary &p_source);
	static String _approval_key(const String &p_action, const String &p_resource);
	static bool _rule_matches(const Dictionary &p_rule, const String &p_action, const String &p_resource);
	static bool _is_resource_match(const String &p_pattern, const String &p_resource);
	String _default_effect_for_action(const String &p_action) const;
	String _evaluate_effect_locked(const String &p_action, const String &p_resource, String &r_reason) const;
	bool _append_permission_event(const String &p_session_id, const String &p_type, const Dictionary &p_data, AIError &r_error);

protected:
	static void _bind_methods();

public:
	void set_event_store(const Ref<AIEventStore> &p_event_store);
	Ref<AIEventStore> get_event_store() const;
	void set_rules(const Array &p_rules);
	Array get_rules() const;

	bool assert_permission_struct(const Dictionary &p_input, AIPermissionDecision &r_decision, AIError &r_error);
	bool reply_struct(const Dictionary &p_input, AIPermissionDecision &r_decision, AIError &r_error);

	Dictionary assert_permission(const Dictionary &p_input);
	Dictionary reply(const Dictionary &p_input);
	Array get_pending_requests() const;
	void clear();
};
