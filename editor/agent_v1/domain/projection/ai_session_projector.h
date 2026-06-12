/**************************************************************************/
/*  ai_session_projector.h                                                */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/domain/events/ai_event_store.h"
#include "editor/agent_v1/domain/model/ai_domain_types.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"

class AISessionProjector : public RefCounted {
	GDCLASS(AISessionProjector, RefCounted);

	HashMap<String, Vector<AISessionInput>> inputs_by_session;
	HashMap<String, Vector<AISessionMessage>> messages_by_session;
	HashMap<String, AIContextEpoch> context_epochs_by_session;
	HashMap<String, int64_t> projected_seq_by_session;
	HashMap<String, HashSet<String>> live_projected_event_ids_by_session;
	mutable Mutex mutex;

	static int _find_input_index(const Vector<AISessionInput> &p_inputs, const String &p_id);
	static int _find_message_index(const Vector<AISessionMessage> &p_messages, const String &p_id);
	static int _find_tool_content_index(const AISessionMessage &p_message, const String &p_call_id);
	static int _find_content_index(const AISessionMessage &p_message, const String &p_type, const String &p_content_id);
	static String _fallback_message_id(const String &p_prefix, int64_t p_seq);

	int _ensure_assistant_message_locked(const String &p_session_id, int64_t p_seq, const String &p_message_id, const Dictionary &p_metadata, uint64_t p_time_created);
	int _ensure_tool_content_locked(AISessionMessage &r_message, const String &p_call_id, const String &p_tool_name, const AIToolState &p_initial_state, const Dictionary &p_provider_metadata);
	void _upsert_text_content_locked(AISessionMessage &r_message, const String &p_type, const String &p_content_id, const String &p_text, const Dictionary &p_provider_metadata, bool p_delta, bool p_final);
	void _project_live_row_locked(const AIEventRow &p_row);
	void _project_row_locked(const AIEventRow &p_row);

protected:
	static void _bind_methods();

public:
	bool project(const AIEventRow &p_row);
	int project(const Vector<AIEventRow> &p_rows);
	bool project_event(const Dictionary &p_event);
	int project_events(const Array &p_events);
	int project_from_store(const Ref<AIEventStore> &p_store, const String &p_session_id, int64_t p_after_seq = 0);
	int rebuild_from_store(const Ref<AIEventStore> &p_store, const String &p_session_id);

	Vector<AISessionInput> get_inputs_struct(const String &p_session_id) const;
	Vector<AISessionMessage> get_messages_struct(const String &p_session_id) const;
	bool get_context_epoch_struct(const String &p_session_id, AIContextEpoch &r_epoch) const;

	Array get_inputs(const String &p_session_id) const;
	Array get_messages(const String &p_session_id) const;
	Dictionary get_context_epoch(const String &p_session_id) const;
	int64_t get_projected_seq(const String &p_session_id) const;
	int mark_pending_tools_interrupted(const String &p_session_id, const String &p_reason = "Tool execution interrupted");
	void clear_session(const String &p_session_id);
	void clear();
};
