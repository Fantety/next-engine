/**************************************************************************/
/*  ai_next_workflow_store.h                                              */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

#include "editor/ai_component/next/ai_next_workflow_snapshot.h"

class AINextWorkflowStore : public RefCounted {
	GDCLASS(AINextWorkflowStore, RefCounted);

	String base_dir = "user://ai_agent/projects/global/next_workflows";

	Error _ensure_base_dir() const;
	String _get_workflow_path(const String &p_workflow_id) const;
	static String _sanitize_scope_key(const String &p_scope_key);
	static String _sanitize_workflow_id(const String &p_workflow_id);

protected:
	static void _bind_methods();

public:
	void set_project_scope(const String &p_project_scope_key);
	void set_base_dir_for_test(const String &p_base_dir);
	String get_base_dir_for_test() const;
	String get_workflow_path_for_test(const String &p_workflow_id) const;

	Error save_workflow(AINextWorkflowSnapshot p_snapshot) const;
	bool load_workflow(const String &p_workflow_id, AINextWorkflowSnapshot &r_snapshot) const;
	bool load_workflow_metadata(const String &p_workflow_id, Dictionary &r_metadata) const;
	bool delete_workflow(const String &p_workflow_id) const;
	bool get_most_recent_workflow_id(String &r_workflow_id) const;
	Array list_workflows() const;
};
