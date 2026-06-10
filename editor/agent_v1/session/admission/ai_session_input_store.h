/**************************************************************************/
/*  ai_session_input_store.h                                              */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_error.h"
#include "editor/agent_v1/domain/events/ai_event_store.h"
#include "editor/agent_v1/domain/projection/ai_session_projector.h"
#include "editor/agent_v1/session/model/ai_session_types.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/templates/vector.h"

class AISessionInputStore : public RefCounted {
	GDCLASS(AISessionInputStore, RefCounted);

	String base_dir;
	Vector<AISessionInputRecord> inputs;
	bool loaded = false;
	Ref<AIEventStore> event_store;
	Ref<AISessionProjector> projector;
	mutable Mutex mutex;

	static uint64_t _now_unix_time();
	static bool _same_retry_shape(const AISessionInputRecord &p_existing, const AISessionInputRecord &p_request, bool p_compare_message_id);
	static bool _same_prompt_content(const AIPrompt &p_left, const AIPrompt &p_right);
	static AIPrompt _prompt_from_message(const AISessionMessage &p_message);

	String _get_log_path() const;
	bool _ensure_base_dir_locked(AIError &r_error) const;
	bool _ensure_loaded_locked(AIError &r_error);
	bool _append_snapshot_locked(const AISessionInputRecord &p_input, AIError &r_error) const;
	int _find_by_id_locked(const String &p_prompt_id) const;
	int _find_by_message_id_locked(const String &p_message_id) const;
	int _find_by_idempotency_key_locked(const String &p_idempotency_key) const;
	bool _try_synthesize_from_projected_locked(const AISessionInputRecord &p_request, AISessionInputAdmission &r_admission, AIError &r_error);

protected:
	static void _bind_methods();

public:
	AISessionInputStore();

	void set_base_dir(const String &p_base_dir);
	String get_base_dir() const;

	void set_event_store(const Ref<AIEventStore> &p_event_store);
	Ref<AIEventStore> get_event_store() const;
	void set_projector(const Ref<AISessionProjector> &p_projector);
	Ref<AISessionProjector> get_projector() const;

	bool admit(const AISessionInputRecord &p_request, AISessionInputAdmission &r_admission, AIError &r_error);
	bool set_admitted_seq(const String &p_session_id, const String &p_prompt_id, int64_t p_admitted_seq, AISessionInputRecord &r_input, AIError &r_error);
	bool mark_promoted(const String &p_session_id, const Vector<String> &p_prompt_ids, Vector<AISessionInputRecord> &r_promoted, AIError &r_error);
	bool cancel(const String &p_session_id, const String &p_prompt_id, const String &p_reason, AISessionInputRecord &r_input, AIError &r_error);

	Vector<AISessionInputRecord> list_admitted_struct(const String &p_session_id);
	Vector<AISessionInputRecord> list_inputs_struct(const String &p_session_id);
	bool find_by_message_id_struct(const String &p_session_id, const String &p_message_id, AISessionInputRecord &r_input);

	Dictionary admit_prompt(const Dictionary &p_input);
	Dictionary mark_promoted_inputs(const String &p_session_id, const Array &p_prompt_ids);
	Array list_admitted(const String &p_session_id);
	Array list_inputs(const String &p_session_id);
	void clear_memory();
};
