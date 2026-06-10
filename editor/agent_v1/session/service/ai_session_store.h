/**************************************************************************/
/*  ai_session_store.h                                                    */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/session/model/ai_session_types.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

class AISessionStore : public RefCounted {
	GDCLASS(AISessionStore, RefCounted);

	String base_dir;
	HashMap<String, AISessionRow> sessions_by_id;
	bool loaded = false;
	mutable Mutex mutex;

	static uint64_t _now_unix_time();
	String _get_log_path() const;
	bool _ensure_base_dir_locked(String &r_error) const;
	bool _ensure_loaded_locked(String &r_error);
	bool _append_snapshot_locked(const AISessionRow &p_session, String &r_error) const;

protected:
	static void _bind_methods();

public:
	AISessionStore();

	void set_base_dir(const String &p_base_dir);
	String get_base_dir() const;

	bool create_or_reuse(const Dictionary &p_input, AISessionRow &r_session, bool &r_created, String &r_error);
	bool get_session_struct(const String &p_session_id, AISessionRow &r_session);

	Dictionary create_session(const Dictionary &p_input);
	Dictionary get_session(const String &p_session_id);
	Array list_sessions();
	void clear_memory();
};
