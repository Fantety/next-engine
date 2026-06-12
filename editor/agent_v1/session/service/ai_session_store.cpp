/**************************************************************************/
/*  ai_session_store.cpp                                                  */
/**************************************************************************/

#include "ai_session_store.h"

#include "editor/agent_v1/core/base/ai_id.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/os/time.h"
#include "core/variant/variant.h"

void AISessionStore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_base_dir", "base_dir"), &AISessionStore::set_base_dir);
	ClassDB::bind_method(D_METHOD("get_base_dir"), &AISessionStore::get_base_dir);
	ClassDB::bind_method(D_METHOD("create_session", "input"), &AISessionStore::create_session);
	ClassDB::bind_method(D_METHOD("get_session", "session_id"), &AISessionStore::get_session);
	ClassDB::bind_method(D_METHOD("update_metadata", "session_id", "metadata"), &AISessionStore::update_metadata);
	ClassDB::bind_method(D_METHOD("archive_session", "session_id"), &AISessionStore::archive_session);
	ClassDB::bind_method(D_METHOD("delete_session", "session_id"), &AISessionStore::delete_session);
	ClassDB::bind_method(D_METHOD("list_sessions"), &AISessionStore::list_sessions);
	ClassDB::bind_method(D_METHOD("clear_memory"), &AISessionStore::clear_memory);
}

AISessionStore::AISessionStore() {
	base_dir = "user://net.nextengine/agent_v1/sessions";
}

uint64_t AISessionStore::_now_unix_time() {
	return Time::get_singleton() ? Time::get_singleton()->get_unix_time_from_system() : 0;
}

String AISessionStore::_get_log_path() const {
	return base_dir.path_join("sessions.jsonl");
}

bool AISessionStore::_ensure_base_dir_locked(String &r_error) const {
	if (base_dir.is_empty()) {
		return true;
	}

	const Error err = DirAccess::make_dir_recursive_absolute(base_dir);
	if (err != OK) {
		r_error = "Failed to create AI session store directory: " + base_dir;
		return false;
	}
	return true;
}

bool AISessionStore::_ensure_loaded_locked(String &r_error) {
	if (loaded) {
		return true;
	}

	if (base_dir.is_empty()) {
		loaded = true;
		return true;
	}

	const String path = _get_log_path();
	if (!FileAccess::exists(path)) {
		loaded = true;
		return true;
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		r_error = "Failed to open AI session store: " + path;
		return false;
	}

	HashMap<String, AISessionRow> loaded_sessions;
	while (!file->eof_reached()) {
		const String line = file->get_line().strip_edges();
		if (line.is_empty()) {
			continue;
		}

		Ref<JSON> json;
		json.instantiate();
		const Error parse_err = json->parse(line);
		if (parse_err != OK || json->get_data().get_type() != Variant::DICTIONARY) {
			r_error = "Failed to parse AI session store row: " + path;
			return false;
		}

		const AISessionRow session = AISessionRow::from_dictionary(json->get_data());
		if (!session.id.is_empty()) {
			loaded_sessions[session.id] = session;
		}
	}

	sessions_by_id = loaded_sessions;
	loaded = true;
	return true;
}

bool AISessionStore::_append_snapshot_locked(const AISessionRow &p_session, String &r_error) const {
	if (base_dir.is_empty()) {
		return true;
	}
	if (!_ensure_base_dir_locked(r_error)) {
		return false;
	}

	const String path = _get_log_path();
	const bool exists = FileAccess::exists(path);
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(path, exists ? FileAccess::READ_WRITE : FileAccess::WRITE_READ, &err);
	if (file.is_null() || err != OK) {
		r_error = "Failed to open AI session store for append: " + path;
		return false;
	}

	file->seek_end();
	file->store_string(JSON::stringify(p_session.to_dictionary()) + "\n");
	file->flush();
	return true;
}

bool AISessionStore::_is_active_status(const String &p_status) {
	const String status = p_status.strip_edges().to_lower();
	return status.is_empty() || status == "active";
}

bool AISessionStore::_set_session_status_struct(const String &p_session_id, const String &p_status, AISessionRow &r_session, String &r_error) {
	const String session_id = p_session_id.strip_edges();
	const String status = p_status.strip_edges().to_lower();
	if (session_id.is_empty()) {
		r_error = "AI session id cannot be empty.";
		return false;
	}
	if (status.is_empty()) {
		r_error = "AI session status cannot be empty.";
		return false;
	}

	MutexLock lock(mutex);
	if (!_ensure_loaded_locked(r_error)) {
		return false;
	}

	HashMap<String, AISessionRow>::Iterator session = sessions_by_id.find(session_id);
	if (!session) {
		r_error = "AI session not found: " + session_id;
		return false;
	}

	AISessionRow updated = session->value;
	if (updated.status == "deleted") {
		r_error = "AI session not found: " + session_id;
		return false;
	}

	updated.status = status;
	updated.updated_at = _now_unix_time();
	if (!_append_snapshot_locked(updated, r_error)) {
		return false;
	}

	sessions_by_id[session_id] = updated;
	r_session = updated;
	return true;
}

void AISessionStore::set_base_dir(const String &p_base_dir) {
	MutexLock lock(mutex);
	base_dir = p_base_dir.strip_edges();
	sessions_by_id.clear();
	loaded = false;
}

String AISessionStore::get_base_dir() const {
	MutexLock lock(mutex);
	return base_dir;
}

bool AISessionStore::create_or_reuse(const Dictionary &p_input, AISessionRow &r_session, bool &r_created, String &r_error) {
	MutexLock lock(mutex);
	if (!_ensure_loaded_locked(r_error)) {
		return false;
	}

	const String requested_id = String(p_input.get("id", p_input.get("session_id", p_input.get("sessionID", String())))).strip_edges();
	if (!requested_id.is_empty() && sessions_by_id.has(requested_id)) {
		r_session = sessions_by_id[requested_id];
		if (!_is_active_status(r_session.status)) {
			r_error = "AI session is not active: " + requested_id;
			return false;
		}
		r_created = false;
		return true;
	}

	AISessionRow session;
	session.id = requested_id.is_empty() ? AIId::make("sess") : requested_id;
	session.agent_id = p_input.get("agent_id", p_input.get("agentID", String()));
	if (p_input.get("location", Variant()).get_type() == Variant::DICTIONARY) {
		session.location = AILocationRef::from_dictionary(p_input["location"]);
	} else {
		session.location.directory = p_input.get("directory", String());
		session.location.workspace_id = p_input.get("workspace_id", p_input.get("workspaceID", String()));
	}
	session.title = p_input.get("title", "New Chat");
	if (p_input.get("metadata", Variant()).get_type() == Variant::DICTIONARY) {
		session.metadata = Dictionary(p_input["metadata"]).duplicate(true);
	}
	session.created_at = _now_unix_time();
	session.updated_at = session.created_at;

	if (!_append_snapshot_locked(session, r_error)) {
		return false;
	}

	sessions_by_id[session.id] = session;
	r_session = session;
	r_created = true;
	return true;
}

bool AISessionStore::get_session_struct(const String &p_session_id, AISessionRow &r_session) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		return false;
	}

	MutexLock lock(mutex);
	String error;
	if (!_ensure_loaded_locked(error)) {
		return false;
	}

	HashMap<String, AISessionRow>::ConstIterator session = sessions_by_id.find(session_id);
	if (!session) {
		return false;
	}
	if (!_is_active_status(session->value.status)) {
		return false;
	}

	r_session = session->value;
	return true;
}

bool AISessionStore::update_metadata_struct(const String &p_session_id, const Dictionary &p_metadata, AISessionRow &r_session, String &r_error) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		r_error = "AI session id cannot be empty.";
		return false;
	}

	MutexLock lock(mutex);
	if (!_ensure_loaded_locked(r_error)) {
		return false;
	}

	HashMap<String, AISessionRow>::Iterator session = sessions_by_id.find(session_id);
	if (!session) {
		r_error = "AI session not found: " + session_id;
		return false;
	}
	if (!_is_active_status(session->value.status)) {
		r_error = "AI session not found: " + session_id;
		return false;
	}

	AISessionRow updated = session->value;
	updated.metadata = p_metadata.duplicate(true);
	updated.updated_at = _now_unix_time();
	if (!_append_snapshot_locked(updated, r_error)) {
		return false;
	}

	sessions_by_id[session_id] = updated;
	r_session = updated;
	return true;
}

bool AISessionStore::archive_session_struct(const String &p_session_id, AISessionRow &r_session, String &r_error) {
	return _set_session_status_struct(p_session_id, "archived", r_session, r_error);
}

bool AISessionStore::delete_session_struct(const String &p_session_id, AISessionRow &r_session, String &r_error) {
	return _set_session_status_struct(p_session_id, "deleted", r_session, r_error);
}

Dictionary AISessionStore::create_session(const Dictionary &p_input) {
	AISessionRow session;
	bool created = false;
	String error;
	if (!create_or_reuse(p_input, session, created, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error;
		return result;
	}

	Dictionary result = session.to_dictionary();
	result["success"] = true;
	result["created"] = created;
	return result;
}

Dictionary AISessionStore::update_metadata(const String &p_session_id, const Dictionary &p_metadata) {
	AISessionRow session;
	String error;
	if (!update_metadata_struct(p_session_id, p_metadata, session, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error;
		return result;
	}

	Dictionary result = session.to_dictionary();
	result["success"] = true;
	return result;
}

Dictionary AISessionStore::get_session(const String &p_session_id) {
	AISessionRow session;
	if (!get_session_struct(p_session_id, session)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = "Session not found.";
		return result;
	}

	Dictionary result = session.to_dictionary();
	result["success"] = true;
	return result;
}

Dictionary AISessionStore::archive_session(const String &p_session_id) {
	AISessionRow session;
	String error;
	if (!archive_session_struct(p_session_id, session, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error;
		return result;
	}

	Dictionary result = session.to_dictionary();
	result["success"] = true;
	return result;
}

Dictionary AISessionStore::delete_session(const String &p_session_id) {
	AISessionRow session;
	String error;
	if (!delete_session_struct(p_session_id, session, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error;
		return result;
	}

	Dictionary result = session.to_dictionary();
	result["success"] = true;
	return result;
}

Array AISessionStore::list_sessions() {
	Array result;
	MutexLock lock(mutex);
	String error;
	if (!_ensure_loaded_locked(error)) {
		return result;
	}

	for (const KeyValue<String, AISessionRow> &E : sessions_by_id) {
		if (!_is_active_status(E.value.status)) {
			continue;
		}
		result.push_back(E.value.to_dictionary());
	}
	return result;
}

void AISessionStore::clear_memory() {
	MutexLock lock(mutex);
	sessions_by_id.clear();
	loaded = false;
}
