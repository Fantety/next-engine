/**************************************************************************/
/*  ai_change_set_store.h                                                  */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class AIChangeSetStore : public RefCounted {
	GDCLASS(AIChangeSetStore, RefCounted);

	Vector<Dictionary> change_sets;
	mutable Mutex mutex;

	static inline Ref<AIChangeSetStore> singleton;

	static String _get_current_project_scope_key();
	static String _make_change_set_id();
	static bool _read_text_file(const String &p_path, String &r_text, String &r_error);
	static bool _ensure_parent_directory(const String &p_path, String &r_error);
	static bool _save_text_resource(const String &p_path, const String &p_text, String &r_error);
	static bool _revert_file_change(const Dictionary &p_change, String &r_error);
	static bool _validate_file_change(const Dictionary &p_change, String &r_error);
	static void _refresh_file_system(const String &p_path);
	static Dictionary _merge_file_change(const Dictionary &p_existing_change, const Dictionary &p_next_change);
	static bool _is_noop_change(const Dictionary &p_change);

	int _find_change_set_index(const String &p_change_set_id) const;
	int _find_pending_change_set_index_for_file(const String &p_project_scope, const String &p_session_id, const String &p_path) const;
	void _recalculate_change_set_totals(Dictionary &r_change_set) const;
	void _mark_change_set_status(const String &p_change_set_id, const String &p_status);
	void _emit_changed_deferred();

protected:
	static void _bind_methods();

public:
	static Ref<AIChangeSetStore> get_singleton();

	String add_change_set(const String &p_title, const String &p_session_id, const String &p_tool_call_id, const Array &p_changes, const Dictionary &p_metadata = Dictionary());
	Array list_change_sets(const String &p_status = "pending") const;
	Dictionary get_change_set(const String &p_change_set_id) const;
	int get_pending_count() const;
	bool keep_change_set(const String &p_change_set_id, String &r_error);
	bool revert_change_set(const String &p_change_set_id, String &r_error);
	void clear_for_test();
};
