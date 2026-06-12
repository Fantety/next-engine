/**************************************************************************/
/*  ai_todo_service.cpp                                                   */
/**************************************************************************/

#include "ai_todo_service.h"

#include "editor/agent_v1/domain/events/ai_event_types.h"
#include "editor/agent_v1/domain/projection/ai_session_projector.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/os/time.h"
#include "core/variant/variant.h"

namespace {

bool _ai_todo_is_valid_status(const String &p_status) {
	const String status = p_status.strip_edges().to_lower();
	return status == "pending" || status == "in_progress" || status == "completed" || status == "cancelled";
}

bool _ai_todo_is_valid_priority(const String &p_priority) {
	const String priority = p_priority.strip_edges().to_lower();
	return priority == "high" || priority == "medium" || priority == "low";
}

String _ai_todo_normalize_status(const String &p_status) {
	String status = p_status.strip_edges().to_lower().replace("-", "_");
	if (status == "canceled") {
		status = "cancelled";
	}
	return status;
}

String _ai_todo_normalize_priority(const String &p_priority) {
	return p_priority.strip_edges().to_lower();
}

Array _ai_todo_storage_rows_from_todos(const Array &p_todos) {
	Array rows;
	for (int i = 0; i < p_todos.size(); i++) {
		if (p_todos[i].get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary row = Dictionary(p_todos[i]).duplicate(true);
		row["position"] = i;
		rows.push_back(row);
	}
	return rows;
}

Array _ai_todo_todos_from_storage_rows(const Array &p_rows) {
	Array todos;
	for (int i = 0; i < p_rows.size(); i++) {
		if (p_rows[i].get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary todo = Dictionary(p_rows[i]).duplicate(true);
		todo.erase("position");
		todos.push_back(todo);
	}
	return todos;
}

} // namespace

void AITodoStore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_base_dir", "base_dir"), &AITodoStore::set_base_dir);
	ClassDB::bind_method(D_METHOD("get_base_dir"), &AITodoStore::get_base_dir);
	ClassDB::bind_method(D_METHOD("update_session_todos", "session_id", "todos"), &AITodoStore::update_session_todos);
	ClassDB::bind_method(D_METHOD("get_session_todos", "session_id"), &AITodoStore::get_session_todos);
	ClassDB::bind_method(D_METHOD("clear_memory"), &AITodoStore::clear_memory);
}

AITodoStore::AITodoStore() {
	base_dir = "user://net.nextengine/agent_v1/todos";
}

uint64_t AITodoStore::_now_unix_time() {
	return Time::get_singleton() ? Time::get_singleton()->get_unix_time_from_system() : 0;
}

String AITodoStore::_get_log_path() const {
	return base_dir.path_join("todos.jsonl");
}

bool AITodoStore::_ensure_base_dir_locked(String &r_error) const {
	if (base_dir.is_empty()) {
		return true;
	}

	const Error err = DirAccess::make_dir_recursive_absolute(base_dir);
	if (err != OK) {
		r_error = "Failed to create AI todo store directory: " + base_dir;
		return false;
	}
	return true;
}

bool AITodoStore::_ensure_loaded_locked(String &r_error) {
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
		r_error = "Failed to open AI todo store: " + path;
		return false;
	}

	HashMap<String, Array> loaded_todos;
	while (!file->eof_reached()) {
		const String line = file->get_line().strip_edges();
		if (line.is_empty()) {
			continue;
		}

		Ref<JSON> json;
		json.instantiate();
		const Error parse_err = json->parse(line);
		if (parse_err != OK || json->get_data().get_type() != Variant::DICTIONARY) {
			r_error = "Failed to parse AI todo store row: " + path;
			return false;
		}

		const Dictionary snapshot = json->get_data();
		const String session_id = String(snapshot.get("session_id", snapshot.get("sessionID", String()))).strip_edges();
		if (session_id.is_empty()) {
			continue;
		}
		if (snapshot.get("todos", Variant()).get_type() == Variant::ARRAY) {
			loaded_todos[session_id] = _ai_todo_todos_from_storage_rows(snapshot["todos"]);
		} else {
			loaded_todos[session_id] = Array();
		}
	}

	todos_by_session = loaded_todos;
	loaded = true;
	return true;
}

bool AITodoStore::_append_snapshot_locked(const String &p_session_id, const Array &p_todos, String &r_error) const {
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
		r_error = "Failed to open AI todo store for append: " + path;
		return false;
	}

	Dictionary snapshot;
	snapshot["session_id"] = p_session_id;
	snapshot["todos"] = _ai_todo_storage_rows_from_todos(p_todos);
	snapshot["updated_at"] = _now_unix_time();

	file->seek_end();
	file->store_string(JSON::stringify(snapshot) + "\n");
	file->flush();
	return true;
}

void AITodoStore::set_base_dir(const String &p_base_dir) {
	MutexLock lock(mutex);
	base_dir = p_base_dir.strip_edges();
	todos_by_session.clear();
	loaded = false;
}

String AITodoStore::get_base_dir() const {
	MutexLock lock(mutex);
	return base_dir;
}

bool AITodoStore::update_session_todos_struct(const String &p_session_id, const Array &p_todos, Array &r_todos, String &r_error) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		r_error = "AI todo session id cannot be empty.";
		return false;
	}

	MutexLock lock(mutex);
	if (!_ensure_loaded_locked(r_error)) {
		return false;
	}

	const Array todos = p_todos.duplicate(true);
	if (!_append_snapshot_locked(session_id, todos, r_error)) {
		return false;
	}

	todos_by_session[session_id] = todos.duplicate(true);
	r_todos = todos.duplicate(true);
	return true;
}

bool AITodoStore::get_session_todos_struct(const String &p_session_id, Array &r_todos) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		r_todos = Array();
		return false;
	}

	MutexLock lock(mutex);
	String error;
	if (!_ensure_loaded_locked(error)) {
		r_todos = Array();
		return false;
	}

	HashMap<String, Array>::ConstIterator todos = todos_by_session.find(session_id);
	if (!todos) {
		r_todos = Array();
		return true;
	}

	r_todos = todos->value.duplicate(true);
	return true;
}

Dictionary AITodoStore::update_session_todos(const String &p_session_id, const Array &p_todos) {
	Array todos;
	String error;
	if (!update_session_todos_struct(p_session_id, p_todos, todos, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error;
		return result;
	}

	Dictionary result;
	result["success"] = true;
	result["session_id"] = p_session_id;
	result["todos"] = todos;
	return result;
}

Array AITodoStore::get_session_todos(const String &p_session_id) {
	Array todos;
	(void)get_session_todos_struct(p_session_id, todos);
	return todos;
}

void AITodoStore::clear_memory() {
	MutexLock lock(mutex);
	todos_by_session.clear();
	loaded = false;
}

void AITodoService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_todo_store", "store"), &AITodoService::set_todo_store);
	ClassDB::bind_method(D_METHOD("get_todo_store"), &AITodoService::get_todo_store);
	ClassDB::bind_method(D_METHOD("set_event_store", "event_store"), &AITodoService::set_event_store);
	ClassDB::bind_method(D_METHOD("get_event_store"), &AITodoService::get_event_store);
	ClassDB::bind_method(D_METHOD("set_projector", "projector"), &AITodoService::set_projector);
	ClassDB::bind_method(D_METHOD("get_projector"), &AITodoService::get_projector);
	ClassDB::bind_method(D_METHOD("update_todos", "session_id", "todos"), &AITodoService::update_todos);
	ClassDB::bind_method(D_METHOD("get_todos", "session_id"), &AITodoService::get_todos);
}

AITodoService::AITodoService() {
	todo_store.instantiate();
}

Dictionary AITodoService::_make_error_result(const AIError &p_error) {
	Dictionary result;
	result["success"] = false;
	result["error"] = p_error.to_dictionary();
	return result;
}

bool AITodoService::_normalize_todos(const Array &p_todos, Array &r_todos, AIError &r_error) {
	Array normalized;
	for (int i = 0; i < p_todos.size(); i++) {
		if (p_todos[i].get_type() != Variant::DICTIONARY) {
			Dictionary details;
			details["index"] = i;
			r_error = AIError::make(AI_ERROR_VALIDATION, "Todo item must be an object.", details);
			return false;
		}

		const Dictionary input = p_todos[i];
		const String content = String(input.get("content", String())).strip_edges();
		if (content.is_empty()) {
			Dictionary details;
			details["index"] = i;
			details["field"] = "content";
			r_error = AIError::make(AI_ERROR_VALIDATION, "Todo content is required.", details);
			return false;
		}

		const String status = _ai_todo_normalize_status(input.get("status", "pending"));
		if (!_ai_todo_is_valid_status(status)) {
			Dictionary details;
			details["index"] = i;
			details["field"] = "status";
			details["value"] = status;
			r_error = AIError::make(AI_ERROR_VALIDATION, "Todo status must be pending, in_progress, completed, or cancelled.", details);
			return false;
		}

		const String priority = _ai_todo_normalize_priority(input.get("priority", "medium"));
		if (!_ai_todo_is_valid_priority(priority)) {
			Dictionary details;
			details["index"] = i;
			details["field"] = "priority";
			details["value"] = priority;
			r_error = AIError::make(AI_ERROR_VALIDATION, "Todo priority must be high, medium, or low.", details);
			return false;
		}

		Dictionary todo;
		todo["content"] = content;
		todo["status"] = status;
		todo["priority"] = priority;
		normalized.push_back(todo);
	}

	r_todos = normalized;
	r_error = AIError::none();
	return true;
}

void AITodoService::set_todo_store(const Ref<AITodoStore> &p_store) {
	todo_store = p_store;
	if (todo_store.is_null()) {
		todo_store.instantiate();
	}
}

Ref<AITodoStore> AITodoService::get_todo_store() const {
	return todo_store;
}

void AITodoService::set_event_store(const Ref<AIEventStore> &p_event_store) {
	event_store = p_event_store;
}

Ref<AIEventStore> AITodoService::get_event_store() const {
	return event_store;
}

void AITodoService::set_projector(const Ref<AISessionProjector> &p_projector) {
	projector = p_projector;
}

Ref<AISessionProjector> AITodoService::get_projector() const {
	return projector;
}

bool AITodoService::update_todos_struct(const String &p_session_id, const Array &p_todos, Array &r_todos, AIError &r_error) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Todo session id is required.");
		return false;
	}
	if (todo_store.is_null()) {
		todo_store.instantiate();
	}

	Array normalized;
	if (!_normalize_todos(p_todos, normalized, r_error)) {
		return false;
	}

	String store_error;
	if (!todo_store->update_session_todos_struct(session_id, normalized, r_todos, store_error)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, store_error);
		return false;
	}

	if (event_store.is_valid()) {
		Dictionary data;
		data["session_id"] = session_id;
		data["todos"] = r_todos.duplicate(true);
		data["updated_at"] = Time::get_singleton() ? Time::get_singleton()->get_unix_time_from_system() : 0;

		AIEventRow row;
		String event_error;
		if (!event_store->append(session_id, AIDomainEventTypes::todo_updated(), data, false, row, event_error)) {
			r_error = AIError::make(AI_ERROR_INTERNAL, event_error);
			return false;
		}
		if (projector.is_valid()) {
			projector->project(row);
		}
	}

	r_error = AIError::none();
	return true;
}

bool AITodoService::get_todos_struct(const String &p_session_id, Array &r_todos) {
	if (todo_store.is_null()) {
		r_todos = Array();
		return false;
	}
	return todo_store->get_session_todos_struct(p_session_id, r_todos);
}

Dictionary AITodoService::update_todos(const String &p_session_id, const Array &p_todos) {
	Array todos;
	AIError error;
	if (!update_todos_struct(p_session_id, p_todos, todos, error)) {
		return _make_error_result(error);
	}

	Dictionary result;
	result["success"] = true;
	result["session_id"] = p_session_id;
	result["todos"] = todos;
	return result;
}

Array AITodoService::get_todos(const String &p_session_id) {
	Array todos;
	(void)get_todos_struct(p_session_id, todos);
	return todos;
}
