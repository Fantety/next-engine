/**************************************************************************/
/*  ai_context_epoch_store.h                                              */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/domain/model/ai_domain_types.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"

class AIContextEpochStore : public RefCounted {
	GDCLASS(AIContextEpochStore, RefCounted);

	HashMap<String, AIContextEpoch> epochs_by_session;
	mutable Mutex mutex;

protected:
	static void _bind_methods();

public:
	bool set_epoch_struct(const AIContextEpoch &p_epoch);
	bool get_epoch_struct(const String &p_session_id, AIContextEpoch &r_epoch) const;
	AIContextEpoch reset_epoch_struct(const String &p_session_id, const String &p_baseline, const Dictionary &p_snapshot, const String &p_agent_id, int64_t p_baseline_seq, int64_t p_replacement_seq = 0);
	bool clear_epoch_struct(const String &p_session_id);

	bool set_epoch(const Dictionary &p_epoch);
	Dictionary get_epoch(const String &p_session_id) const;
	Dictionary reset_epoch(const String &p_session_id, const String &p_baseline, const Dictionary &p_snapshot, const String &p_agent_id, int64_t p_baseline_seq, int64_t p_replacement_seq = 0);
	bool clear_epoch(const String &p_session_id);
	bool has_epoch(const String &p_session_id) const;
	void clear();
};
