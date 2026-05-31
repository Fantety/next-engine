/**************************************************************************/
/*  ai_next_project_state.h                                               */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "editor/ai_component/next/ai_next_types.h"

class AINextProjectState : public RefCounted {
	GDCLASS(AINextProjectState, RefCounted);

	String brief;
	AINextSessionState session_state = AI_NEXT_SESSION_IDLE;
	Vector<AINextMilestone> milestones;
	Vector<AINextAssetRecord> assets;
	String active_milestone_id;
	String last_error;
	int next_milestone_number = 1;
	int next_task_number = 1;
	int next_asset_number = 1;

	String _make_milestone_id();
	String _make_task_id();
	String _make_asset_id();
	uint64_t _now() const;
	AINextMilestone *_find_milestone(const String &p_milestone_id);
	const AINextMilestone *_find_milestone(const String &p_milestone_id) const;
	AINextTask *_find_task(const String &p_task_id, AINextMilestone **r_milestone = nullptr);
	const AINextTask *_find_task(const String &p_task_id, const AINextMilestone **r_milestone = nullptr) const;
	bool _are_dependencies_satisfied(const AINextTask &p_task) const;
	bool _has_unfinished_dependency(const AINextTask &p_task) const;
	void _touch_task(AINextTask &r_task, AINextMilestone *p_milestone);
	void _refresh_milestone_status(AINextMilestone &r_milestone);
	void _sync_id_counters();

protected:
	static void _bind_methods();

public:
	void clear();

	void set_brief(const String &p_brief);
	String get_brief() const;
	void set_session_state(AINextSessionState p_state);
	AINextSessionState get_session_state() const;
	String get_session_state_name() const;
	void set_active_milestone_id(const String &p_milestone_id);
	String get_active_milestone_id() const;
	String get_last_error() const;

	String create_milestone(const String &p_title, const String &p_description);
	bool update_milestone(const String &p_milestone_id, const Dictionary &p_patch, String &r_error);
	String add_task(const String &p_milestone_id, const String &p_title, const String &p_assigned_agent_id, const Array &p_depends_on, const String &p_description = String(), const String &p_task_id = String());
	bool append_tasks(const String &p_milestone_id, const Array &p_tasks, String &r_error);
	bool update_task(const String &p_task_id, const Dictionary &p_patch, String &r_error);
	bool set_task_status(const String &p_task_id, AINextTaskStatus p_status, const String &p_error = String());
	bool mark_task_completed(const String &p_task_id, const String &p_result_summary, const Array &p_output_paths);
	bool mark_task_failed(const String &p_task_id, const String &p_error);
	bool skip_task(const String &p_task_id, const String &p_reason);
	bool retry_task(const String &p_task_id);
	bool reassign_task(const String &p_task_id, const String &p_assigned_agent_id);
	bool split_task(const String &p_task_id, const Array &p_split_tasks, String &r_error);
	Array get_ready_tasks(const String &p_milestone_id) const;
	Array get_milestones_as_array() const;
	Dictionary get_milestone(const String &p_milestone_id) const;
	Dictionary get_task(const String &p_task_id) const;
	int get_milestone_count() const;
	int get_task_count(const String &p_milestone_id) const;
	bool has_milestone(const String &p_milestone_id) const;
	bool has_task(const String &p_task_id) const;
	String get_task_milestone_id(const String &p_task_id) const;
	bool has_dependency_cycle(String &r_error) const;
	bool can_run_milestone(const String &p_milestone_id) const;
	bool can_lock_milestone(const String &p_milestone_id, String &r_error) const;
	bool lock_milestone(const String &p_milestone_id, String &r_error);
	String register_asset(const String &p_path, const String &p_source, bool p_protected_from_agent_edits, const String &p_parent_asset_id = String(), const String &p_baseline_milestone_id = String(), const String &p_asset_id = String());
	int get_asset_count() const;
	bool replace_from_milestones_array(const Array &p_milestones, String &r_error);
	Dictionary to_dict() const;
	void load_from_dict(const Dictionary &p_dict);
};
