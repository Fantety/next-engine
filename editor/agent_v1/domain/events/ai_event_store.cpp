/**************************************************************************/
/*  ai_event_store.cpp                                                    */
/**************************************************************************/

#include "ai_event_store.h"

#include "editor/agent_v1/core/base/ai_id.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/os/time.h"
#include "core/variant/variant.h"

void AIEventStore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_base_dir", "base_dir"), &AIEventStore::set_base_dir);
	ClassDB::bind_method(D_METHOD("get_base_dir"), &AIEventStore::get_base_dir);
	ClassDB::bind_method(D_METHOD("append_event", "aggregate_id", "type", "data", "live_only"), &AIEventStore::append_event, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("append_event_idempotent", "aggregate_id", "type", "data", "idempotency_key", "live_only"), &AIEventStore::append_event_idempotent, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("append_durable_event", "aggregate_id", "type", "data"), &AIEventStore::append_durable_event);
	ClassDB::bind_method(D_METHOD("append_live_event", "aggregate_id", "type", "data"), &AIEventStore::append_live_event);
	ClassDB::bind_method(D_METHOD("list_events", "aggregate_id", "after_seq", "include_live"), &AIEventStore::list_events, DEFVAL(0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("replay_events", "aggregate_id", "after_seq"), &AIEventStore::replay_events, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_latest_seq", "aggregate_id"), &AIEventStore::get_latest_seq);
	ClassDB::bind_method(D_METHOD("load_aggregate", "aggregate_id"), &AIEventStore::load_aggregate);
	ClassDB::bind_method(D_METHOD("clear_memory"), &AIEventStore::clear_memory);

	ADD_SIGNAL(MethodInfo("event_appended", PropertyInfo(Variant::DICTIONARY, "event")));
	ADD_SIGNAL(MethodInfo("durable_event_appended", PropertyInfo(Variant::DICTIONARY, "event")));
	ADD_SIGNAL(MethodInfo("live_event_appended", PropertyInfo(Variant::DICTIONARY, "event")));
}

AIEventStore::AIEventStore() {
	base_dir = "user://agent_v1/events";
}

String AIEventStore::_sanitize_aggregate_id(const String &p_aggregate_id) {
	const String clean = p_aggregate_id.strip_edges().validate_filename();
	return clean.is_empty() ? String("session") : clean;
}

uint64_t AIEventStore::_now_unix_time() {
	return Time::get_singleton() ? Time::get_singleton()->get_unix_time_from_system() : 0;
}

String AIEventStore::_get_aggregate_path(const String &p_aggregate_id) const {
	return base_dir.path_join(_sanitize_aggregate_id(p_aggregate_id) + ".jsonl");
}

bool AIEventStore::_ensure_base_dir_locked(String &r_error) const {
	if (base_dir.is_empty()) {
		return true;
	}

	const Error err = DirAccess::make_dir_recursive_absolute(base_dir);
	if (err != OK) {
		r_error = "Failed to create AI event store directory: " + base_dir;
		return false;
	}
	return true;
}

bool AIEventStore::_ensure_loaded_locked(const String &p_aggregate_id, String &r_error) {
	if (loaded_aggregates.has(p_aggregate_id)) {
		return true;
	}

	if (base_dir.is_empty()) {
		durable_events_by_aggregate[p_aggregate_id] = Vector<AIEventRow>();
		latest_seq_by_aggregate[p_aggregate_id] = 0;
		loaded_aggregates[p_aggregate_id] = true;
		return true;
	}

	const String path = _get_aggregate_path(p_aggregate_id);
	if (!FileAccess::exists(path)) {
		durable_events_by_aggregate[p_aggregate_id] = Vector<AIEventRow>();
		latest_seq_by_aggregate[p_aggregate_id] = 0;
		loaded_aggregates[p_aggregate_id] = true;
		return true;
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		r_error = "Failed to open AI event log: " + path;
		return false;
	}

	Vector<AIEventRow> rows;
	int64_t latest_seq = 0;
	while (!file->eof_reached()) {
		const String line = file->get_line().strip_edges();
		if (line.is_empty()) {
			continue;
		}

		Ref<JSON> json;
		json.instantiate();
		const Error parse_err = json->parse(line);
		if (parse_err != OK || json->get_data().get_type() != Variant::DICTIONARY) {
			r_error = "Failed to parse AI event log row: " + path;
			return false;
		}

		AIEventRow row = AIEventRow::from_dictionary(json->get_data());
		if (row.aggregate_id.is_empty()) {
			row.aggregate_id = p_aggregate_id;
		}
		if (row.aggregate_id != p_aggregate_id || row.live_only) {
			continue;
		}

		rows.push_back(row);
		if (row.seq > latest_seq) {
			latest_seq = row.seq;
		}
	}

	durable_events_by_aggregate[p_aggregate_id] = rows;
	latest_seq_by_aggregate[p_aggregate_id] = latest_seq;
	loaded_aggregates[p_aggregate_id] = true;
	return true;
}

bool AIEventStore::_write_durable_event_locked(const AIEventRow &p_row, String &r_error) const {
	if (base_dir.is_empty()) {
		return true;
	}
	if (!_ensure_base_dir_locked(r_error)) {
		return false;
	}

	const String path = _get_aggregate_path(p_row.aggregate_id);
	const bool exists = FileAccess::exists(path);
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(path, exists ? FileAccess::READ_WRITE : FileAccess::WRITE_READ, &err);
	if (file.is_null() || err != OK) {
		r_error = "Failed to open AI event log for append: " + path;
		return false;
	}

	file->seek_end();
	file->store_string(JSON::stringify(p_row.to_dictionary()) + "\n");
	file->flush();
	return true;
}

bool AIEventStore::_find_idempotent_event_locked(const String &p_aggregate_id, const String &p_idempotency_key, AIEventRow &r_row) const {
	if (p_idempotency_key.is_empty()) {
		return false;
	}

	HashMap<String, Vector<AIEventRow>>::ConstIterator durable = durable_events_by_aggregate.find(p_aggregate_id);
	if (durable) {
		for (int i = 0; i < durable->value.size(); i++) {
			const AIEventRow &row = durable->value[i];
			if (row.idempotency_key == p_idempotency_key) {
				r_row = row;
				return true;
			}
		}
	}

	HashMap<String, Vector<AIEventRow>>::ConstIterator live = live_events_by_aggregate.find(p_aggregate_id);
	if (live) {
		for (int i = 0; i < live->value.size(); i++) {
			const AIEventRow &row = live->value[i];
			if (row.idempotency_key == p_idempotency_key) {
				r_row = row;
				return true;
			}
		}
	}

	return false;
}

void AIEventStore::_queue_event_signal(const AIEventRow &p_row) {
	const Dictionary row_dict = p_row.to_dictionary();
	call_deferred("emit_signal", SNAME("event_appended"), row_dict);
	call_deferred("emit_signal", p_row.live_only ? SNAME("live_event_appended") : SNAME("durable_event_appended"), row_dict);
}

void AIEventStore::set_base_dir(const String &p_base_dir) {
	MutexLock lock(mutex);
	base_dir = p_base_dir.strip_edges();
	durable_events_by_aggregate.clear();
	live_events_by_aggregate.clear();
	latest_seq_by_aggregate.clear();
	loaded_aggregates.clear();
}

String AIEventStore::get_base_dir() const {
	MutexLock lock(mutex);
	return base_dir;
}

bool AIEventStore::append(const String &p_aggregate_id, const String &p_type, const Dictionary &p_data, bool p_live_only, AIEventRow &r_row, String &r_error) {
	return append_idempotent(p_aggregate_id, p_type, p_data, p_live_only, String(), r_row, r_error);
}

bool AIEventStore::append_idempotent(const String &p_aggregate_id, const String &p_type, const Dictionary &p_data, bool p_live_only, const String &p_idempotency_key, AIEventRow &r_row, String &r_error) {
	const String aggregate_id = p_aggregate_id.strip_edges();
	const String type = p_type.strip_edges();
	const String idempotency_key = p_idempotency_key.strip_edges();
	if (aggregate_id.is_empty()) {
		r_error = "AI event aggregate_id cannot be empty.";
		return false;
	}
	if (type.is_empty()) {
		r_error = "AI event type cannot be empty.";
		return false;
	}

	AIEventRow row;
	bool reused = false;
	{
		MutexLock lock(mutex);
		if (!_ensure_loaded_locked(aggregate_id, r_error)) {
			return false;
		}

		const bool live_only = p_live_only || AIDomainEventTypes::is_live_only_event(type);
		if (!idempotency_key.is_empty() && _find_idempotent_event_locked(aggregate_id, idempotency_key, row)) {
			if (row.type != type || row.live_only != live_only || !row.data.recursive_equal(p_data, 0)) {
				r_error = "AI event idempotency conflict for key: " + idempotency_key;
				return false;
			}
			reused = true;
		}

		if (reused) {
			r_row = row;
			return true;
		}

		const int64_t latest_seq = latest_seq_by_aggregate.has(aggregate_id) ? latest_seq_by_aggregate[aggregate_id] : 0;
		row.id = AIId::make(live_only ? "live_evt" : "evt");
		row.aggregate_id = aggregate_id;
		row.schema_version = AIEventRow::CURRENT_SCHEMA_VERSION;
		row.seq = live_only ? latest_seq : latest_seq + 1;
		row.type = type;
		row.data = p_data.duplicate(true);
		row.idempotency_key = idempotency_key;
		row.timestamp = _now_unix_time();
		row.live_only = live_only;

		if (live_only) {
			live_events_by_aggregate[aggregate_id].push_back(row);
		} else {
			if (!_write_durable_event_locked(row, r_error)) {
				return false;
			}
			durable_events_by_aggregate[aggregate_id].push_back(row);
			latest_seq_by_aggregate[aggregate_id] = row.seq;
		}
	}

	r_row = row;
	_queue_event_signal(row);
	return true;
}

Dictionary AIEventStore::append_event(const String &p_aggregate_id, const String &p_type, const Dictionary &p_data, bool p_live_only) {
	AIEventRow row;
	String error;
	if (!append(p_aggregate_id, p_type, p_data, p_live_only, row, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error;
		return result;
	}

	Dictionary result = row.to_dictionary();
	result["success"] = true;
	return result;
}

Dictionary AIEventStore::append_event_idempotent(const String &p_aggregate_id, const String &p_type, const Dictionary &p_data, const String &p_idempotency_key, bool p_live_only) {
	AIEventRow row;
	String error;
	if (!append_idempotent(p_aggregate_id, p_type, p_data, p_live_only, p_idempotency_key, row, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error;
		return result;
	}

	Dictionary result = row.to_dictionary();
	result["success"] = true;
	return result;
}

Dictionary AIEventStore::append_durable_event(const String &p_aggregate_id, const String &p_type, const Dictionary &p_data) {
	return append_event(p_aggregate_id, p_type, p_data, false);
}

Dictionary AIEventStore::append_live_event(const String &p_aggregate_id, const String &p_type, const Dictionary &p_data) {
	return append_event(p_aggregate_id, p_type, p_data, true);
}

Vector<AIEventRow> AIEventStore::list(const String &p_aggregate_id, int64_t p_after_seq, bool p_include_live) {
	Vector<AIEventRow> result;
	const String aggregate_id = p_aggregate_id.strip_edges();
	if (aggregate_id.is_empty()) {
		return result;
	}

	MutexLock lock(mutex);
	String error;
	if (!_ensure_loaded_locked(aggregate_id, error)) {
		return result;
	}

	HashMap<String, Vector<AIEventRow>>::Iterator durable = durable_events_by_aggregate.find(aggregate_id);
	if (durable) {
		for (int i = 0; i < durable->value.size(); i++) {
			const AIEventRow &row = durable->value[i];
			if (row.seq > p_after_seq) {
				result.push_back(row);
			}
		}
	}

	if (p_include_live) {
		HashMap<String, Vector<AIEventRow>>::Iterator live = live_events_by_aggregate.find(aggregate_id);
		if (live) {
			for (int i = 0; i < live->value.size(); i++) {
				const AIEventRow &row = live->value[i];
				if (row.seq >= p_after_seq) {
					result.push_back(row);
				}
			}
		}
	}
	return result;
}

Array AIEventStore::list_events(const String &p_aggregate_id, int64_t p_after_seq, bool p_include_live) {
	Array result;
	const Vector<AIEventRow> rows = list(p_aggregate_id, p_after_seq, p_include_live);
	for (int i = 0; i < rows.size(); i++) {
		result.push_back(rows[i].to_dictionary());
	}
	return result;
}

Array AIEventStore::replay_events(const String &p_aggregate_id, int64_t p_after_seq) {
	return list_events(p_aggregate_id, p_after_seq, false);
}

int64_t AIEventStore::get_latest_seq(const String &p_aggregate_id) {
	const String aggregate_id = p_aggregate_id.strip_edges();
	if (aggregate_id.is_empty()) {
		return 0;
	}

	MutexLock lock(mutex);
	String error;
	if (!_ensure_loaded_locked(aggregate_id, error)) {
		return 0;
	}
	return latest_seq_by_aggregate.has(aggregate_id) ? latest_seq_by_aggregate[aggregate_id] : 0;
}

bool AIEventStore::load_aggregate(const String &p_aggregate_id) {
	const String aggregate_id = p_aggregate_id.strip_edges();
	if (aggregate_id.is_empty()) {
		return false;
	}

	MutexLock lock(mutex);
	String error;
	return _ensure_loaded_locked(aggregate_id, error);
}

void AIEventStore::clear_memory() {
	MutexLock lock(mutex);
	durable_events_by_aggregate.clear();
	live_events_by_aggregate.clear();
	latest_seq_by_aggregate.clear();
	loaded_aggregates.clear();
}
