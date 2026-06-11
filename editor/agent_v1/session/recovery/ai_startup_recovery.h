/**************************************************************************/
/*  ai_startup_recovery.h                                                 */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_error.h"
#include "editor/agent_v1/domain/events/ai_event_store.h"
#include "editor/agent_v1/domain/projection/ai_session_projector.h"
#include "editor/agent_v1/session/service/ai_session_store.h"

#include "core/object/ref_counted.h"
#include "core/variant/variant.h"

class AIStartupRecovery : public RefCounted {
	GDCLASS(AIStartupRecovery, RefCounted);

	Ref<AISessionStore> session_store;
	Ref<AIEventStore> event_store;
	Ref<AISessionProjector> projector;

	static String _step_key(const Dictionary &p_data);
	static String _tool_key(const Dictionary &p_data);
	static String _tool_call_id(const Dictionary &p_data);
	static Dictionary _dictionary_from_variant(const Variant &p_value);
	static Dictionary _make_error_result(const AIError &p_error);
	static bool _append_failed_step(const Ref<AIEventStore> &p_event_store, const Ref<AISessionProjector> &p_projector, const String &p_session_id, const Dictionary &p_started, const String &p_reason, Dictionary &r_report, AIError &r_error);
	static bool _append_failed_tool(const Ref<AIEventStore> &p_event_store, const Ref<AISessionProjector> &p_projector, const String &p_session_id, const Dictionary &p_called, const String &p_reason, Dictionary &r_report, AIError &r_error);

protected:
	static void _bind_methods();

public:
	void set_session_store(const Ref<AISessionStore> &p_store);
	Ref<AISessionStore> get_session_store() const;
	void set_event_store(const Ref<AIEventStore> &p_store);
	Ref<AIEventStore> get_event_store() const;
	void set_projector(const Ref<AISessionProjector> &p_projector);
	Ref<AISessionProjector> get_projector() const;

	static bool cleanup_open_activity_struct(const Ref<AIEventStore> &p_event_store, const Ref<AISessionProjector> &p_projector, const String &p_session_id, const String &p_reason, bool p_fail_steps, bool p_fail_tools, bool p_retain_permission_pending_tools, Dictionary &r_report, AIError &r_error);

	bool recover_struct(Dictionary &r_report, AIError &r_error);
	Dictionary recover();
};
