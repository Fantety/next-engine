/**************************************************************************/
/*  ai_plan_manager.h                                                      */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

struct AIPlanTask {
	String id;
	String title;
	String status = "pending";
};

class AIPlanManager : public RefCounted {
	GDCLASS(AIPlanManager, RefCounted);

	static inline Ref<AIPlanManager> singleton;

	String active_plan_id;
	String active_title;
	Vector<AIPlanTask> active_tasks;
	Dictionary archived_plan;

	static String _normalize_status(const String &p_status);
	static Dictionary _task_to_dict(const AIPlanTask &p_task);
	AIPlanTask _task_from_dict(const Dictionary &p_task) const;
	String _make_plan_id() const;
	String _make_task_id(int p_index) const;
	void _emit_plan_changed();
	void _archive_active_plan();

protected:
	static void _bind_methods();

public:
	static Ref<AIPlanManager> get_singleton();

	bool has_active_plan() const;
	String get_active_plan_id() const;
	Dictionary get_active_plan() const;
	Dictionary get_archived_plan() const;
	void clear_for_test();

	bool create_plan(const String &p_title, const Array &p_tasks, String &r_error);
	bool update_task(const String &p_task_id, const String &p_status, String &r_error);
	bool archive_plan(String &r_error);
};
