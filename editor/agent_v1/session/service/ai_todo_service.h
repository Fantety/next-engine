/**************************************************************************/
/*  ai_todo_service.h                                                     */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_error.h"
#include "editor/agent_v1/domain/events/ai_event_store.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class AISessionProjector;

class AITodoStore : public RefCounted {
	GDCLASS(AITodoStore, RefCounted);

	String base_dir;
	HashMap<String, Array> todos_by_session;
	bool loaded = false;
	mutable Mutex mutex;

	static uint64_t _now_unix_time();
	String _get_log_path() const;
	bool _ensure_base_dir_locked(String &r_error) const;
	bool _ensure_loaded_locked(String &r_error);
	bool _append_snapshot_locked(const String &p_session_id, const Array &p_todos, String &r_error) const;

protected:
	static void _bind_methods();

public:
	AITodoStore();

	void set_base_dir(const String &p_base_dir);
	String get_base_dir() const;

	bool update_session_todos_struct(const String &p_session_id, const Array &p_todos, Array &r_todos, String &r_error);
	bool get_session_todos_struct(const String &p_session_id, Array &r_todos);

	Dictionary update_session_todos(const String &p_session_id, const Array &p_todos);
	Array get_session_todos(const String &p_session_id);
	void clear_memory();
};

class AITodoService : public RefCounted {
	GDCLASS(AITodoService, RefCounted);

	Ref<AITodoStore> todo_store;
	Ref<AIEventStore> event_store;
	Ref<AISessionProjector> projector;

	static Dictionary _make_error_result(const AIError &p_error);
	static bool _normalize_todos(const Array &p_todos, Array &r_todos, AIError &r_error);

protected:
	static void _bind_methods();

public:
	AITodoService();

	void set_todo_store(const Ref<AITodoStore> &p_store);
	Ref<AITodoStore> get_todo_store() const;
	void set_event_store(const Ref<AIEventStore> &p_event_store);
	Ref<AIEventStore> get_event_store() const;
	void set_projector(const Ref<AISessionProjector> &p_projector);
	Ref<AISessionProjector> get_projector() const;

	bool update_todos_struct(const String &p_session_id, const Array &p_todos, Array &r_todos, AIError &r_error);
	bool get_todos_struct(const String &p_session_id, Array &r_todos);

	Dictionary update_todos(const String &p_session_id, const Array &p_todos);
	Array get_todos(const String &p_session_id);
};
