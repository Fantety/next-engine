/**************************************************************************/
/*  ai_session_execution.h                                                */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/session/model/ai_session_types.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"

class AISessionExecution : public RefCounted {
	GDCLASS(AISessionExecution, RefCounted);

	HashMap<String, AISessionExecutionState> slots;
	mutable Mutex mutex;

	static void _merge_wake_seq(AISessionExecutionState &r_state, int64_t p_seq);
	void _emit_drain_requested(const AISessionExecutionState &p_state);

protected:
	static void _bind_methods();

public:
	bool wake_struct(const String &p_session_id, int64_t p_seq, AISessionExecutionState &r_state);
	bool settle_struct(const String &p_session_id, AISessionExecutionState &r_state);
	bool interrupt_struct(const String &p_session_id, const String &p_reason, AISessionExecutionState &r_state);
	AISessionExecutionState get_state_struct(const String &p_session_id) const;

	Dictionary wake(const String &p_session_id, int64_t p_seq = 0);
	Dictionary settle(const String &p_session_id);
	Dictionary interrupt(const String &p_session_id, const String &p_reason = String());
	Dictionary get_state(const String &p_session_id) const;
	void clear();
};
