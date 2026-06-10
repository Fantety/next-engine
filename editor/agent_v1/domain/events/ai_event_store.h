/**************************************************************************/
/*  ai_event_store.h                                                      */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/domain/events/ai_event_types.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"

class AIEventStore : public RefCounted {
	GDCLASS(AIEventStore, RefCounted);

	String base_dir;
	HashMap<String, Vector<AIEventRow>> durable_events_by_aggregate;
	HashMap<String, Vector<AIEventRow>> live_events_by_aggregate;
	HashMap<String, int64_t> latest_seq_by_aggregate;
	HashMap<String, bool> loaded_aggregates;
	mutable Mutex mutex;

	static String _sanitize_aggregate_id(const String &p_aggregate_id);
	static uint64_t _now_unix_time();
	String _get_aggregate_path(const String &p_aggregate_id) const;
	bool _ensure_base_dir_locked(String &r_error) const;
	bool _ensure_loaded_locked(const String &p_aggregate_id, String &r_error);
	bool _write_durable_event_locked(const AIEventRow &p_row, String &r_error) const;
	void _queue_event_signal(const AIEventRow &p_row);

protected:
	static void _bind_methods();

public:
	AIEventStore();

	void set_base_dir(const String &p_base_dir);
	String get_base_dir() const;

	bool append(const String &p_aggregate_id, const String &p_type, const Dictionary &p_data, bool p_live_only, AIEventRow &r_row, String &r_error);
	Dictionary append_event(const String &p_aggregate_id, const String &p_type, const Dictionary &p_data, bool p_live_only = false);
	Dictionary append_durable_event(const String &p_aggregate_id, const String &p_type, const Dictionary &p_data);
	Dictionary append_live_event(const String &p_aggregate_id, const String &p_type, const Dictionary &p_data);

	Vector<AIEventRow> list(const String &p_aggregate_id, int64_t p_after_seq = 0, bool p_include_live = false);
	Array list_events(const String &p_aggregate_id, int64_t p_after_seq = 0, bool p_include_live = false);
	Array replay_events(const String &p_aggregate_id, int64_t p_after_seq = 0);

	int64_t get_latest_seq(const String &p_aggregate_id);
	bool load_aggregate(const String &p_aggregate_id);
	void clear_memory();
};
