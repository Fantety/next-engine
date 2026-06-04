/**************************************************************************/
/*  ai_next_project_state.cpp                                             */
/**************************************************************************/

#include "ai_next_project_state.h"

#include "core/object/class_db.h"
#include "core/os/time.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"

namespace {

bool _is_terminal_dependency_status(AINextTaskStatus p_status) {
	return p_status == AI_NEXT_TASK_COMPLETED || p_status == AI_NEXT_TASK_SKIPPED;
}

bool _is_unfinished_task_status(AINextTaskStatus p_status) {
	return p_status == AI_NEXT_TASK_PENDING || p_status == AI_NEXT_TASK_BLOCKED || p_status == AI_NEXT_TASK_READY || p_status == AI_NEXT_TASK_IN_PROGRESS;
}

String _format_next_id(const String &p_prefix, int p_number) {
	return p_prefix + "_" + itos(p_number).pad_zeros(3);
}

bool _visit_task_dependencies_for_cycle(const AINextTask &p_task, const HashMap<String, const AINextTask *> &p_tasks_by_id, HashMap<String, int> &r_visit_state, String &r_error) {
	const int *state = r_visit_state.getptr(p_task.id);
	if (state) {
		if (*state == 1) {
			r_error = "NEXT task dependencies contain a cycle.";
			return true;
		}
		if (*state == 2) {
			return false;
		}
	}

	r_visit_state.insert(p_task.id, 1);
	for (const String &dependency_id : p_task.depends_on) {
		const AINextTask *const *dependency = p_tasks_by_id.getptr(dependency_id);
		if (!dependency || !*dependency) {
			continue;
		}
		if (_visit_task_dependencies_for_cycle(**dependency, p_tasks_by_id, r_visit_state, r_error)) {
			return true;
		}
	}
	r_visit_state.insert(p_task.id, 2);
	return false;
}

} // namespace

void AINextProjectState::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear"), &AINextProjectState::clear);
	ClassDB::bind_method(D_METHOD("set_brief", "brief"), &AINextProjectState::set_brief);
	ClassDB::bind_method(D_METHOD("get_brief"), &AINextProjectState::get_brief);
	ClassDB::bind_method(D_METHOD("get_session_state_name"), &AINextProjectState::get_session_state_name);
	ClassDB::bind_method(D_METHOD("set_active_milestone_id", "milestone_id"), &AINextProjectState::set_active_milestone_id);
	ClassDB::bind_method(D_METHOD("get_active_milestone_id"), &AINextProjectState::get_active_milestone_id);
	ClassDB::bind_method(D_METHOD("get_last_error"), &AINextProjectState::get_last_error);
	ClassDB::bind_method(D_METHOD("create_milestone", "title", "description"), &AINextProjectState::create_milestone);
	ClassDB::bind_method(D_METHOD("add_task", "milestone_id", "title", "assigned_agent_id", "depends_on", "description", "task_id"), &AINextProjectState::add_task, DEFVAL(String()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("mark_task_completed", "task_id", "result_summary", "output_paths"), &AINextProjectState::mark_task_completed);
	ClassDB::bind_method(D_METHOD("mark_task_failed", "task_id", "error"), &AINextProjectState::mark_task_failed);
	ClassDB::bind_method(D_METHOD("reset_interrupted_task", "task_id"), &AINextProjectState::reset_interrupted_task);
	ClassDB::bind_method(D_METHOD("skip_task", "task_id", "reason"), &AINextProjectState::skip_task);
	ClassDB::bind_method(D_METHOD("retry_task", "task_id"), &AINextProjectState::retry_task);
	ClassDB::bind_method(D_METHOD("reassign_task", "task_id", "assigned_agent_id"), &AINextProjectState::reassign_task);
	ClassDB::bind_method(D_METHOD("get_ready_tasks", "milestone_id"), &AINextProjectState::get_ready_tasks);
	ClassDB::bind_method(D_METHOD("get_milestones_as_array"), &AINextProjectState::get_milestones_as_array);
	ClassDB::bind_method(D_METHOD("get_milestone", "milestone_id"), &AINextProjectState::get_milestone);
	ClassDB::bind_method(D_METHOD("get_task", "task_id"), &AINextProjectState::get_task);
	ClassDB::bind_method(D_METHOD("get_milestone_count"), &AINextProjectState::get_milestone_count);
	ClassDB::bind_method(D_METHOD("get_task_count", "milestone_id"), &AINextProjectState::get_task_count);
	ClassDB::bind_method(D_METHOD("has_milestone", "milestone_id"), &AINextProjectState::has_milestone);
	ClassDB::bind_method(D_METHOD("has_task", "task_id"), &AINextProjectState::has_task);
	ClassDB::bind_method(D_METHOD("get_task_milestone_id", "task_id"), &AINextProjectState::get_task_milestone_id);
	ClassDB::bind_method(D_METHOD("can_run_milestone", "milestone_id"), &AINextProjectState::can_run_milestone);
	ClassDB::bind_method(D_METHOD("register_asset", "path", "source", "protected_from_agent_edits", "parent_asset_id", "baseline_milestone_id", "asset_id"), &AINextProjectState::register_asset, DEFVAL(String()), DEFVAL(String()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_asset_count"), &AINextProjectState::get_asset_count);
	ClassDB::bind_method(D_METHOD("to_dict"), &AINextProjectState::to_dict);
	ClassDB::bind_method(D_METHOD("load_from_dict", "dict"), &AINextProjectState::load_from_dict);
}

String AINextProjectState::_make_milestone_id() {
	String id;
	do {
		id = _format_next_id("milestone", next_milestone_number++);
	} while (_find_milestone(id) != nullptr);
	return id;
}

String AINextProjectState::_make_task_id() {
	String id;
	do {
		id = _format_next_id("task", next_task_number++);
	} while (_find_task(id) != nullptr);
	return id;
}

String AINextProjectState::_make_asset_id() {
	String id;
	bool exists = false;
	do {
		exists = false;
		id = _format_next_id("asset", next_asset_number++);
		for (const AINextAssetRecord &asset : assets) {
			if (asset.id == id) {
				exists = true;
				break;
			}
		}
	} while (exists);
	return id;
}

uint64_t AINextProjectState::_now() const {
	return Time::get_singleton()->get_unix_time_from_system();
}

int AINextProjectState::_find_milestone_index(const String &p_milestone_id) const {
	for (int i = 0; i < milestones.size(); i++) {
		if (milestones[i].id == p_milestone_id) {
			return i;
		}
	}
	return -1;
}

int AINextProjectState::_find_task_index(const String &p_task_id, int *r_milestone_index) const {
	for (int i = 0; i < milestones.size(); i++) {
		for (int j = 0; j < milestones[i].tasks.size(); j++) {
			if (milestones[i].tasks[j].id == p_task_id) {
				if (r_milestone_index) {
					*r_milestone_index = i;
				}
				return j;
			}
		}
	}
	if (r_milestone_index) {
		*r_milestone_index = -1;
	}
	return -1;
}

AINextMilestone *AINextProjectState::_find_milestone(const String &p_milestone_id) {
	for (AINextMilestone &milestone : milestones) {
		if (milestone.id == p_milestone_id) {
			return &milestone;
		}
	}
	return nullptr;
}

const AINextMilestone *AINextProjectState::_find_milestone(const String &p_milestone_id) const {
	for (const AINextMilestone &milestone : milestones) {
		if (milestone.id == p_milestone_id) {
			return &milestone;
		}
	}
	return nullptr;
}

AINextTask *AINextProjectState::_find_task(const String &p_task_id, AINextMilestone **r_milestone) {
	for (AINextMilestone &milestone : milestones) {
		for (AINextTask &task : milestone.tasks) {
			if (task.id == p_task_id) {
				if (r_milestone) {
					*r_milestone = &milestone;
				}
				return &task;
			}
		}
	}
	return nullptr;
}

const AINextTask *AINextProjectState::_find_task(const String &p_task_id, const AINextMilestone **r_milestone) const {
	for (const AINextMilestone &milestone : milestones) {
		for (const AINextTask &task : milestone.tasks) {
			if (task.id == p_task_id) {
				if (r_milestone) {
					*r_milestone = &milestone;
				}
				return &task;
			}
		}
	}
	return nullptr;
}

bool AINextProjectState::_are_dependencies_satisfied(const AINextTask &p_task) const {
	for (const String &dependency_id : p_task.depends_on) {
		const AINextTask *dependency = _find_task(dependency_id);
		if (!dependency || !_is_terminal_dependency_status(dependency->status)) {
			return false;
		}
	}
	return true;
}

bool AINextProjectState::_has_unfinished_dependency(const AINextTask &p_task) const {
	for (const String &dependency_id : p_task.depends_on) {
		const AINextTask *dependency = _find_task(dependency_id);
		if (!dependency || !_is_terminal_dependency_status(dependency->status)) {
			return true;
		}
	}
	return false;
}

bool AINextProjectState::_is_milestone_locked(const String &p_milestone_id) const {
	const AINextMilestone *milestone = _find_milestone(p_milestone_id);
	return milestone && milestone->status == AI_NEXT_MILESTONE_LOCKED;
}

bool AINextProjectState::_is_task_milestone_locked(const String &p_task_id) const {
	const AINextMilestone *milestone = nullptr;
	_find_task(p_task_id, &milestone);
	return milestone && milestone->status == AI_NEXT_MILESTONE_LOCKED;
}

bool AINextProjectState::_validate_task_dependencies(const String &p_task_id, const Vector<String> &p_depends_on, String &r_error) const {
	if (!_find_task(p_task_id)) {
		r_error = "Unknown NEXT task.";
		return false;
	}

	for (const String &dependency_id : p_depends_on) {
		const String clean_dependency_id = dependency_id.strip_edges();
		if (clean_dependency_id.is_empty()) {
			continue;
		}
		if (clean_dependency_id == p_task_id) {
			r_error = "NEXT task cannot depend on itself.";
			return false;
		}
		if (!_find_task(clean_dependency_id)) {
			r_error = "Unknown NEXT task dependency.";
			return false;
		}
	}
	return true;
}

void AINextProjectState::_remove_task_dependency_references(const String &p_task_id) {
	for (AINextMilestone &milestone : milestones) {
		bool changed = false;
		for (AINextTask &task : milestone.tasks) {
			for (int i = task.depends_on.size() - 1; i >= 0; i--) {
				if (task.depends_on[i] == p_task_id) {
					task.depends_on.remove_at(i);
					task.updated_at = _now();
					changed = true;
				}
			}
		}
		if (changed) {
			milestone.updated_at = _now();
			_refresh_milestone_status(milestone);
		}
	}
}

void AINextProjectState::_touch_task(AINextTask &r_task, AINextMilestone *p_milestone) {
	const uint64_t now = _now();
	r_task.updated_at = now;
	if (p_milestone) {
		p_milestone->updated_at = now;
		_refresh_milestone_status(*p_milestone);
	}
}

void AINextProjectState::_refresh_milestone_status(AINextMilestone &r_milestone) {
	if (r_milestone.status == AI_NEXT_MILESTONE_LOCKED) {
		return;
	}

	bool has_failed = false;
	bool has_in_progress = false;
	bool has_unfinished = false;
	bool has_ready = false;
	for (const AINextTask &task : r_milestone.tasks) {
		if (task.status == AI_NEXT_TASK_FAILED) {
			has_failed = true;
		}
		if (task.status == AI_NEXT_TASK_IN_PROGRESS) {
			has_in_progress = true;
		}
		if (_is_unfinished_task_status(task.status)) {
			has_unfinished = true;
		}
		if ((task.status == AI_NEXT_TASK_PENDING || task.status == AI_NEXT_TASK_READY) && _are_dependencies_satisfied(task)) {
			has_ready = true;
		}
	}

	if (has_failed) {
		r_milestone.status = AI_NEXT_MILESTONE_FAILED;
	} else if (has_in_progress) {
		r_milestone.status = AI_NEXT_MILESTONE_EXECUTING;
	} else if (!has_unfinished && !r_milestone.tasks.is_empty()) {
		r_milestone.status = AI_NEXT_MILESTONE_WAITING_PLAYTEST;
	} else if (has_ready) {
		r_milestone.status = AI_NEXT_MILESTONE_READY;
	} else if (r_milestone.tasks.is_empty()) {
		r_milestone.status = AI_NEXT_MILESTONE_DRAFT;
	}
}

void AINextProjectState::_sync_id_counters() {
	next_milestone_number = MAX(1, milestones.size() + 1);
	next_task_number = 1;
	next_asset_number = MAX(1, assets.size() + 1);

	for (const AINextMilestone &milestone : milestones) {
		if (milestone.id.begins_with("milestone_")) {
			next_milestone_number = MAX(next_milestone_number, milestone.id.get_slice("_", 1).to_int() + 1);
		}
		for (const AINextTask &task : milestone.tasks) {
			if (task.id.begins_with("task_")) {
				next_task_number = MAX(next_task_number, task.id.get_slice("_", 1).to_int() + 1);
			}
		}
	}
	for (const AINextAssetRecord &asset : assets) {
		if (asset.id.begins_with("asset_")) {
			next_asset_number = MAX(next_asset_number, asset.id.get_slice("_", 1).to_int() + 1);
		}
	}
}

void AINextProjectState::clear() {
	brief.clear();
	session_state = AI_NEXT_SESSION_IDLE;
	milestones.clear();
	assets.clear();
	active_milestone_id.clear();
	last_error.clear();
	next_milestone_number = 1;
	next_task_number = 1;
	next_asset_number = 1;
}

void AINextProjectState::set_brief(const String &p_brief) {
	brief = p_brief;
}

String AINextProjectState::get_brief() const {
	return brief;
}

void AINextProjectState::set_session_state(AINextSessionState p_state) {
	session_state = p_state;
}

AINextSessionState AINextProjectState::get_session_state() const {
	return session_state;
}

String AINextProjectState::get_session_state_name() const {
	return ai_next_session_state_to_string(session_state);
}

void AINextProjectState::set_active_milestone_id(const String &p_milestone_id) {
	active_milestone_id = p_milestone_id;
}

String AINextProjectState::get_active_milestone_id() const {
	return active_milestone_id;
}

String AINextProjectState::get_last_error() const {
	return last_error;
}

String AINextProjectState::create_milestone(const String &p_title, const String &p_description) {
	AINextMilestone milestone;
	milestone.id = _make_milestone_id();
	milestone.title = p_title;
	milestone.description = p_description;
	milestone.created_at = _now();
	milestone.updated_at = milestone.created_at;
	milestones.push_back(milestone);
	if (active_milestone_id.is_empty()) {
		active_milestone_id = milestone.id;
	}
	return milestone.id;
}

bool AINextProjectState::update_milestone(const String &p_milestone_id, const Dictionary &p_patch, String &r_error) {
	AINextMilestone *milestone = _find_milestone(p_milestone_id);
	if (!milestone) {
		r_error = "Unknown NEXT milestone.";
		last_error = r_error;
		return false;
	}
	if (milestone->status == AI_NEXT_MILESTONE_LOCKED) {
		r_error = "locked NEXT milestones cannot be edited.";
		last_error = r_error;
		return false;
	}
	if (p_patch.has("title")) {
		milestone->title = String(p_patch["title"]);
	}
	if (p_patch.has("description")) {
		milestone->description = String(p_patch["description"]);
	}
	if (p_patch.has("status")) {
		milestone->status = ai_next_milestone_status_from_string(String(p_patch["status"]));
	}
	if (p_patch.has("feedback_iteration")) {
		milestone->feedback_iteration = (int)p_patch["feedback_iteration"];
	}
	milestone->updated_at = _now();
	last_error.clear();
	return true;
}

bool AINextProjectState::delete_milestone(const String &p_milestone_id, String &r_error) {
	const int milestone_index = _find_milestone_index(p_milestone_id);
	if (milestone_index < 0) {
		r_error = "Unknown NEXT milestone.";
		last_error = r_error;
		return false;
	}
	if (milestones[milestone_index].status == AI_NEXT_MILESTONE_LOCKED) {
		r_error = "locked NEXT milestones cannot be edited.";
		last_error = r_error;
		return false;
	}

	Vector<String> removed_task_ids;
	for (const AINextTask &task : milestones[milestone_index].tasks) {
		removed_task_ids.push_back(task.id);
	}

	milestones.remove_at(milestone_index);
	for (const String &task_id : removed_task_ids) {
		_remove_task_dependency_references(task_id);
	}

	if (active_milestone_id == p_milestone_id) {
		if (milestones.is_empty()) {
			active_milestone_id.clear();
		} else {
			active_milestone_id = milestones[MIN(milestone_index, milestones.size() - 1)].id;
		}
	}
	_sync_id_counters();
	last_error.clear();
	r_error.clear();
	return true;
}

bool AINextProjectState::move_milestone(const String &p_milestone_id, int p_to_index, String &r_error) {
	const int from_index = _find_milestone_index(p_milestone_id);
	if (from_index < 0) {
		r_error = "Unknown NEXT milestone.";
		last_error = r_error;
		return false;
	}
	if (milestones[from_index].status == AI_NEXT_MILESTONE_LOCKED) {
		r_error = "locked NEXT milestones cannot be edited.";
		last_error = r_error;
		return false;
	}
	if (milestones.size() <= 1) {
		r_error = "NEXT milestone order is already stable.";
		last_error = r_error;
		return false;
	}

	const int to_index = CLAMP(p_to_index, 0, milestones.size() - 1);
	if (from_index == to_index) {
		r_error.clear();
		last_error.clear();
		return true;
	}

	AINextMilestone milestone = milestones[from_index];
	milestones.remove_at(from_index);
	milestone.updated_at = _now();
	milestones.insert(to_index, milestone);
	last_error.clear();
	r_error.clear();
	return true;
}

bool AINextProjectState::merge_milestones(const String &p_target_milestone_id, const String &p_source_milestone_id, String &r_error) {
	if (p_target_milestone_id == p_source_milestone_id) {
		r_error = "Cannot merge a NEXT milestone into itself.";
		last_error = r_error;
		return false;
	}

	AINextMilestone *target = _find_milestone(p_target_milestone_id);
	AINextMilestone *source = _find_milestone(p_source_milestone_id);
	if (!target || !source) {
		r_error = "Unknown NEXT milestone.";
		last_error = r_error;
		return false;
	}
	if (target->status == AI_NEXT_MILESTONE_LOCKED || source->status == AI_NEXT_MILESTONE_LOCKED) {
		r_error = "locked NEXT milestones cannot be edited.";
		last_error = r_error;
		return false;
	}

	const int source_index = _find_milestone_index(p_source_milestone_id);
	for (const AINextTask &task : source->tasks) {
		target->tasks.push_back(task);
	}
	target->updated_at = _now();
	_refresh_milestone_status(*target);
	milestones.remove_at(source_index);

	if (active_milestone_id == p_source_milestone_id) {
		active_milestone_id = p_target_milestone_id;
	}
	_sync_id_counters();
	last_error.clear();
	r_error.clear();
	return true;
}

String AINextProjectState::add_task(const String &p_milestone_id, const String &p_title, const String &p_assigned_agent_id, const Array &p_depends_on, const String &p_description, const String &p_task_id) {
	AINextMilestone *milestone = _find_milestone(p_milestone_id);
	if (!milestone) {
		last_error = "Unknown NEXT milestone.";
		return String();
	}
	if (milestone->status == AI_NEXT_MILESTONE_LOCKED) {
		last_error = "locked NEXT milestones cannot be edited.";
		return String();
	}

	const String requested_id = p_task_id.strip_edges();
	if (!requested_id.is_empty() && _find_task(requested_id)) {
		last_error = "Duplicate NEXT task id.";
		return String();
	}

	AINextTask task;
	task.id = requested_id.is_empty() ? _make_task_id() : requested_id;
	task.title = p_title;
	task.description = p_description;
	task.assigned_agent_id = p_assigned_agent_id;
	task.depends_on = ai_next_array_to_string_vector(p_depends_on);
	task.created_at = _now();
	task.updated_at = task.created_at;
	const Dictionary snapshot = to_dict();
	milestone->tasks.push_back(task);
	milestone->updated_at = task.created_at;
	_refresh_milestone_status(*milestone);
	String dependency_error;
	if (!_validate_task_dependencies(task.id, task.depends_on, dependency_error) || has_dependency_cycle(dependency_error)) {
		load_from_dict(snapshot);
		last_error = dependency_error;
		return String();
	}
	last_error.clear();
	return task.id;
}

bool AINextProjectState::append_tasks(const String &p_milestone_id, const Array &p_tasks, String &r_error) {
	AINextMilestone *milestone = _find_milestone(p_milestone_id);
	if (!milestone) {
		r_error = "Unknown NEXT milestone.";
		last_error = r_error;
		return false;
	}
	if (milestone->status == AI_NEXT_MILESTONE_LOCKED) {
		r_error = "locked NEXT milestones cannot be edited.";
		last_error = r_error;
		return false;
	}

	const Dictionary snapshot = to_dict();
	Vector<AINextTask> new_tasks;
	for (int i = 0; i < p_tasks.size(); i++) {
		if (Variant(p_tasks[i]).get_type() != Variant::DICTIONARY) {
			r_error = "Task entries must be dictionaries.";
			last_error = r_error;
			return false;
		}
		Dictionary task_dict = p_tasks[i];
		AINextTask task = ai_next_task_from_dict(task_dict);
		if (task.id.strip_edges().is_empty()) {
			task.id = _make_task_id();
		}
		if (task.title.strip_edges().is_empty()) {
			r_error = "Task title is required.";
			last_error = r_error;
			return false;
		}
		if (task.assigned_agent_id.strip_edges().is_empty()) {
			r_error = "Task assigned_agent_id is required.";
			last_error = r_error;
			return false;
		}
		if (_find_task(task.id)) {
			r_error = "Duplicate NEXT task id.";
			last_error = r_error;
			return false;
		}
		for (const AINextTask &queued_task : new_tasks) {
			if (queued_task.id == task.id) {
				r_error = "Duplicate NEXT task id.";
				last_error = r_error;
				return false;
			}
		}
		const uint64_t now = _now();
		if (task.created_at == 0) {
			task.created_at = now;
		}
		task.updated_at = now;
		new_tasks.push_back(task);
	}

	for (const AINextTask &task : new_tasks) {
		milestone->tasks.push_back(task);
	}
	milestone->updated_at = _now();
	_refresh_milestone_status(*milestone);
	for (const AINextTask &task : new_tasks) {
		if (!_validate_task_dependencies(task.id, task.depends_on, r_error)) {
			load_from_dict(snapshot);
			last_error = r_error;
			return false;
		}
	}
	if (has_dependency_cycle(r_error)) {
		load_from_dict(snapshot);
		last_error = r_error;
		return false;
	}
	_sync_id_counters();
	last_error.clear();
	r_error.clear();
	return true;
}

bool AINextProjectState::update_task(const String &p_task_id, const Dictionary &p_patch, String &r_error) {
	AINextMilestone *milestone = nullptr;
	AINextTask *task = _find_task(p_task_id, &milestone);
	if (!task) {
		r_error = "Unknown NEXT task.";
		last_error = r_error;
		return false;
	}
	if (milestone && milestone->status == AI_NEXT_MILESTONE_LOCKED) {
		r_error = "locked NEXT milestones cannot be edited.";
		last_error = r_error;
		return false;
	}

	const Dictionary snapshot = p_patch.has("depends_on") ? to_dict() : Dictionary();
	if (p_patch.has("title")) {
		task->title = String(p_patch["title"]);
	}
	if (p_patch.has("description")) {
		task->description = String(p_patch["description"]);
	}
	if (p_patch.has("assigned_agent_id")) {
		task->assigned_agent_id = String(p_patch["assigned_agent_id"]);
	}
	if (p_patch.has("depends_on") && Variant(p_patch["depends_on"]).get_type() == Variant::ARRAY) {
		Vector<String> depends_on = ai_next_array_to_string_vector(p_patch["depends_on"]);
		if (!_validate_task_dependencies(p_task_id, depends_on, r_error)) {
			load_from_dict(snapshot);
			last_error = r_error;
			return false;
		}
		task->depends_on = depends_on;
	}
	if (p_patch.has("asset_refs") && Variant(p_patch["asset_refs"]).get_type() == Variant::ARRAY) {
		task->asset_refs = ai_next_array_to_string_vector(p_patch["asset_refs"]);
	}
	if (p_patch.has("output_paths") && Variant(p_patch["output_paths"]).get_type() == Variant::ARRAY) {
		task->output_paths = ai_next_array_to_string_vector(p_patch["output_paths"]);
	}
	if (p_patch.has("attachments") && Variant(p_patch["attachments"]).get_type() == Variant::ARRAY) {
		task->attachments = Array(p_patch["attachments"]).duplicate(true);
	}
	if (p_patch.has("status")) {
		task->status = ai_next_task_status_from_string(String(p_patch["status"]));
	}
	if (p_patch.has("run_id")) {
		task->run_id = String(p_patch["run_id"]);
	}
	if (p_patch.has("result_summary")) {
		task->result_summary = String(p_patch["result_summary"]);
	}
	if (p_patch.has("error")) {
		task->error = String(p_patch["error"]);
	}
	_touch_task(*task, milestone);
	if (p_patch.has("depends_on") && has_dependency_cycle(r_error)) {
		load_from_dict(snapshot);
		last_error = r_error;
		return false;
	}
	last_error.clear();
	r_error.clear();
	return true;
}

bool AINextProjectState::delete_task(const String &p_task_id, String &r_error) {
	int milestone_index = -1;
	const int task_index = _find_task_index(p_task_id, &milestone_index);
	if (task_index < 0 || milestone_index < 0) {
		r_error = "Unknown NEXT task.";
		last_error = r_error;
		return false;
	}
	AINextMilestone &milestone = milestones.write[milestone_index];
	if (milestone.status == AI_NEXT_MILESTONE_LOCKED) {
		r_error = "locked NEXT milestones cannot be edited.";
		last_error = r_error;
		return false;
	}

	milestone.tasks.remove_at(task_index);
	milestone.updated_at = _now();
	_refresh_milestone_status(milestone);
	_remove_task_dependency_references(p_task_id);
	_sync_id_counters();
	last_error.clear();
	r_error.clear();
	return true;
}

bool AINextProjectState::move_task(const String &p_task_id, const String &p_target_milestone_id, int p_to_index, String &r_error) {
	int source_milestone_index = -1;
	const int source_task_index = _find_task_index(p_task_id, &source_milestone_index);
	const int target_milestone_index = _find_milestone_index(p_target_milestone_id);
	if (source_task_index < 0 || source_milestone_index < 0 || target_milestone_index < 0) {
		r_error = "Unknown NEXT task or milestone.";
		last_error = r_error;
		return false;
	}
	if (milestones[source_milestone_index].status == AI_NEXT_MILESTONE_LOCKED || milestones[target_milestone_index].status == AI_NEXT_MILESTONE_LOCKED) {
		r_error = "locked NEXT milestones cannot be edited.";
		last_error = r_error;
		return false;
	}

	AINextTask task = milestones[source_milestone_index].tasks[source_task_index];
	milestones.write[source_milestone_index].tasks.remove_at(source_task_index);

	AINextMilestone &target_milestone = milestones.write[target_milestone_index];
	int to_index = CLAMP(p_to_index, 0, target_milestone.tasks.size());

	task.updated_at = _now();
	target_milestone.tasks.insert(to_index, task);
	milestones.write[source_milestone_index].updated_at = _now();
	target_milestone.updated_at = _now();
	_refresh_milestone_status(milestones.write[source_milestone_index]);
	_refresh_milestone_status(target_milestone);
	last_error.clear();
	r_error.clear();
	return true;
}

bool AINextProjectState::set_task_dependencies(const String &p_task_id, const Array &p_depends_on, String &r_error) {
	if (_is_task_milestone_locked(p_task_id)) {
		r_error = "locked NEXT milestones cannot be edited.";
		last_error = r_error;
		return false;
	}

	Dictionary patch;
	patch["depends_on"] = p_depends_on;
	return update_task(p_task_id, patch, r_error);
}

bool AINextProjectState::set_task_status(const String &p_task_id, AINextTaskStatus p_status, const String &p_error) {
	AINextMilestone *milestone = nullptr;
	AINextTask *task = _find_task(p_task_id, &milestone);
	if (!task) {
		last_error = "Unknown NEXT task.";
		return false;
	}
	task->status = p_status;
	task->error = p_error;
	_touch_task(*task, milestone);
	last_error.clear();
	return true;
}

bool AINextProjectState::mark_task_completed(const String &p_task_id, const String &p_result_summary, const Array &p_output_paths) {
	AINextMilestone *milestone = nullptr;
	AINextTask *task = _find_task(p_task_id, &milestone);
	if (!task) {
		last_error = "Unknown NEXT task.";
		return false;
	}

	task->status = AI_NEXT_TASK_COMPLETED;
	task->result_summary = p_result_summary;
	task->error.clear();
	task->output_paths = ai_next_array_to_string_vector(p_output_paths);
	_touch_task(*task, milestone);
	last_error.clear();
	return true;
}

bool AINextProjectState::mark_task_failed(const String &p_task_id, const String &p_error) {
	return set_task_status(p_task_id, AI_NEXT_TASK_FAILED, p_error);
}

bool AINextProjectState::reset_interrupted_task(const String &p_task_id) {
	AINextMilestone *milestone = nullptr;
	AINextTask *task = _find_task(p_task_id, &milestone);
	if (!task) {
		last_error = "Unknown NEXT task.";
		return false;
	}
	if (task->status == AI_NEXT_TASK_IN_PROGRESS) {
		task->status = AI_NEXT_TASK_PENDING;
		task->error.clear();
		_touch_task(*task, milestone);
	}
	last_error.clear();
	return true;
}

bool AINextProjectState::skip_task(const String &p_task_id, const String &p_reason) {
	AINextMilestone *milestone = nullptr;
	AINextTask *task = _find_task(p_task_id, &milestone);
	if (!task) {
		last_error = "Unknown NEXT task.";
		return false;
	}
	task->status = AI_NEXT_TASK_SKIPPED;
	task->result_summary = p_reason;
	task->error.clear();
	_touch_task(*task, milestone);
	last_error.clear();
	return true;
}

bool AINextProjectState::retry_task(const String &p_task_id) {
	AINextMilestone *milestone = nullptr;
	AINextTask *task = _find_task(p_task_id, &milestone);
	if (!task) {
		last_error = "Unknown NEXT task.";
		return false;
	}
	if (task->status != AI_NEXT_TASK_FAILED) {
		last_error = "Only failed NEXT tasks can be retried.";
		return false;
	}
	task->status = AI_NEXT_TASK_PENDING;
	task->error.clear();
	task->run_id.clear();
	_touch_task(*task, milestone);
	last_error.clear();
	return true;
}

bool AINextProjectState::reassign_task(const String &p_task_id, const String &p_assigned_agent_id) {
	const String assigned_agent_id = p_assigned_agent_id.strip_edges();
	if (assigned_agent_id.is_empty()) {
		last_error = "NEXT task assigned_agent_id is required.";
		return false;
	}

	AINextMilestone *milestone = nullptr;
	AINextTask *task = _find_task(p_task_id, &milestone);
	if (!task) {
		last_error = "Unknown NEXT task.";
		return false;
	}
	task->assigned_agent_id = assigned_agent_id;
	_touch_task(*task, milestone);
	last_error.clear();
	return true;
}

bool AINextProjectState::split_task(const String &p_task_id, const Array &p_split_tasks, String &r_error) {
	r_error.clear();

	AINextMilestone *milestone = nullptr;
	AINextTask *task = _find_task(p_task_id, &milestone);
	if (!task || !milestone) {
		r_error = "Unknown NEXT task.";
		last_error = r_error;
		return false;
	}
	if (task->status != AI_NEXT_TASK_FAILED) {
		r_error = "Only failed NEXT tasks can be split.";
		last_error = r_error;
		return false;
	}
	if (p_split_tasks.is_empty()) {
		r_error = "Split requires replacement tasks.";
		last_error = r_error;
		return false;
	}

	const Dictionary snapshot = to_dict();
	const int original_next_task_number = next_task_number;
	Vector<AINextTask> new_tasks;
	for (int i = 0; i < p_split_tasks.size(); i++) {
		if (Variant(p_split_tasks[i]).get_type() != Variant::DICTIONARY) {
			next_task_number = original_next_task_number;
			r_error = "Split task entries must be dictionaries.";
			last_error = r_error;
			return false;
		}

		AINextTask split_task = ai_next_task_from_dict(p_split_tasks[i]);
		if (split_task.id.strip_edges().is_empty()) {
			split_task.id = _make_task_id();
		}
		if (split_task.title.strip_edges().is_empty()) {
			next_task_number = original_next_task_number;
			r_error = "Split task title is required.";
			last_error = r_error;
			return false;
		}
		if (split_task.assigned_agent_id.strip_edges().is_empty()) {
			next_task_number = original_next_task_number;
			r_error = "Split task assigned_agent_id is required.";
			last_error = r_error;
			return false;
		}
		if (_find_task(split_task.id)) {
			next_task_number = original_next_task_number;
			r_error = "Duplicate NEXT task id.";
			last_error = r_error;
			return false;
		}
		for (const AINextTask &queued_task : new_tasks) {
			if (queued_task.id == split_task.id) {
				next_task_number = original_next_task_number;
				r_error = "Duplicate NEXT task id.";
				last_error = r_error;
				return false;
			}
		}
		if (split_task.depends_on.is_empty()) {
			if (new_tasks.is_empty()) {
				split_task.depends_on = task->depends_on;
			} else {
				split_task.depends_on.push_back(new_tasks[new_tasks.size() - 1].id);
			}
		}
		const uint64_t now = _now();
		if (split_task.created_at == 0) {
			split_task.created_at = now;
		}
		split_task.updated_at = now;
		new_tasks.push_back(split_task);
	}

	task->status = AI_NEXT_TASK_SKIPPED;
	task->result_summary = "Split into replacement NEXT tasks.";
	task->error.clear();
	for (const AINextTask &split_task : new_tasks) {
		milestone->tasks.push_back(split_task);
	}
	milestone->updated_at = _now();
	_refresh_milestone_status(*milestone);

	if (has_dependency_cycle(r_error)) {
		load_from_dict(snapshot);
		last_error = r_error;
		return false;
	}

	_sync_id_counters();
	last_error.clear();
	return true;
}

Array AINextProjectState::get_ready_tasks(const String &p_milestone_id) const {
	Array ready;
	const AINextMilestone *milestone = _find_milestone(p_milestone_id);
	if (!milestone) {
		return ready;
	}

	for (const AINextTask &task : milestone->tasks) {
		if ((task.status == AI_NEXT_TASK_PENDING || task.status == AI_NEXT_TASK_READY) && _are_dependencies_satisfied(task)) {
			Dictionary task_dict = ai_next_task_to_dict(task);
			task_dict["status"] = ai_next_task_status_to_string(AI_NEXT_TASK_READY);
			ready.push_back(task_dict);
		}
	}
	return ready;
}

Array AINextProjectState::get_milestones_as_array() const {
	Array result;
	for (const AINextMilestone &milestone : milestones) {
		result.push_back(ai_next_milestone_to_dict(milestone));
	}
	return result;
}

Dictionary AINextProjectState::get_milestone(const String &p_milestone_id) const {
	const AINextMilestone *milestone = _find_milestone(p_milestone_id);
	if (!milestone) {
		return Dictionary();
	}
	return ai_next_milestone_to_dict(*milestone);
}

Dictionary AINextProjectState::get_task(const String &p_task_id) const {
	const AINextTask *task = _find_task(p_task_id);
	if (!task) {
		return Dictionary();
	}
	Dictionary task_dict = ai_next_task_to_dict(*task);
	if (task->status == AI_NEXT_TASK_PENDING && _has_unfinished_dependency(*task)) {
		task_dict["status"] = ai_next_task_status_to_string(AI_NEXT_TASK_BLOCKED);
	} else if (task->status == AI_NEXT_TASK_PENDING && _are_dependencies_satisfied(*task)) {
		task_dict["status"] = ai_next_task_status_to_string(AI_NEXT_TASK_READY);
	}
	return task_dict;
}

int AINextProjectState::get_milestone_count() const {
	return milestones.size();
}

int AINextProjectState::get_task_count(const String &p_milestone_id) const {
	const AINextMilestone *milestone = _find_milestone(p_milestone_id);
	return milestone ? milestone->tasks.size() : 0;
}

bool AINextProjectState::has_milestone(const String &p_milestone_id) const {
	return _find_milestone(p_milestone_id) != nullptr;
}

bool AINextProjectState::has_task(const String &p_task_id) const {
	return _find_task(p_task_id) != nullptr;
}

String AINextProjectState::get_task_milestone_id(const String &p_task_id) const {
	const AINextMilestone *milestone = nullptr;
	if (!_find_task(p_task_id, &milestone) || !milestone) {
		return String();
	}
	return milestone->id;
}

bool AINextProjectState::has_dependency_cycle(String &r_error) const {
	HashMap<String, const AINextTask *> tasks_by_id;
	for (const AINextMilestone &milestone : milestones) {
		for (const AINextTask &task : milestone.tasks) {
			tasks_by_id.insert(task.id, &task);
		}
	}

	HashMap<String, int> visit_state;
	for (const AINextMilestone &milestone : milestones) {
		for (const AINextTask &task : milestone.tasks) {
			if (_visit_task_dependencies_for_cycle(task, tasks_by_id, visit_state, r_error)) {
				return true;
			}
		}
	}
	return false;
}

bool AINextProjectState::can_run_milestone(const String &p_milestone_id) const {
	const AINextMilestone *milestone = _find_milestone(p_milestone_id);
	if (!milestone) {
		return false;
	}
	if (milestone->status != AI_NEXT_MILESTONE_READY && milestone->status != AI_NEXT_MILESTONE_WAITING_PLAYTEST) {
		return false;
	}
	return !get_ready_tasks(p_milestone_id).is_empty();
}

bool AINextProjectState::can_lock_milestone(const String &p_milestone_id, String &r_error) const {
	const AINextMilestone *milestone = _find_milestone(p_milestone_id);
	if (!milestone) {
		r_error = "Unknown NEXT milestone.";
		return false;
	}
	for (const AINextTask &task : milestone->tasks) {
		if (task.status == AI_NEXT_TASK_FAILED) {
			r_error = "Milestone has failed tasks.";
			return false;
		}
		if (task.status == AI_NEXT_TASK_IN_PROGRESS) {
			r_error = "Milestone has running tasks.";
			return false;
		}
		if (task.status != AI_NEXT_TASK_COMPLETED && task.status != AI_NEXT_TASK_SKIPPED) {
			r_error = "Milestone has unfinished tasks.";
			return false;
		}
	}
	return true;
}

bool AINextProjectState::lock_milestone(const String &p_milestone_id, String &r_error) {
	if (!can_lock_milestone(p_milestone_id, r_error)) {
		return false;
	}
	AINextMilestone *milestone = _find_milestone(p_milestone_id);
	ERR_FAIL_NULL_V(milestone, false);
	milestone->status = AI_NEXT_MILESTONE_LOCKED;
	milestone->updated_at = _now();
	return true;
}

String AINextProjectState::register_asset(const String &p_path, const String &p_source, bool p_protected_from_agent_edits, const String &p_parent_asset_id, const String &p_baseline_milestone_id, const String &p_asset_id) {
	const String requested_id = p_asset_id.strip_edges();
	if (!requested_id.is_empty()) {
		for (const AINextAssetRecord &asset : assets) {
			if (asset.id == requested_id) {
				last_error = "Duplicate NEXT asset id.";
				return String();
			}
		}
	}

	AINextAssetRecord asset;
	asset.id = requested_id.is_empty() ? _make_asset_id() : requested_id;
	asset.path = p_path;
	asset.source = p_source;
	asset.protected_from_agent_edits = p_protected_from_agent_edits;
	asset.parent_asset_id = p_parent_asset_id;
	asset.baseline_milestone_id = p_baseline_milestone_id;
	assets.push_back(asset);
	last_error.clear();
	return asset.id;
}

int AINextProjectState::get_asset_count() const {
	return assets.size();
}

bool AINextProjectState::replace_from_milestones_array(const Array &p_milestones, String &r_error) {
	Vector<AINextMilestone> parsed_milestones;
	int generated_task_number = 1;
	for (int i = 0; i < p_milestones.size(); i++) {
		if (Variant(p_milestones[i]).get_type() != Variant::DICTIONARY) {
			r_error = "Milestone entries must be dictionaries.";
			return false;
		}
		AINextMilestone milestone = ai_next_milestone_from_dict(p_milestones[i]);
		if (milestone.id.strip_edges().is_empty()) {
			milestone.id = _format_next_id("milestone", i + 1);
		}
		if (milestone.title.strip_edges().is_empty()) {
			r_error = "Milestone title is required.";
			return false;
		}
		for (int j = 0; j < parsed_milestones.size(); j++) {
			if (parsed_milestones[j].id == milestone.id) {
				r_error = "Duplicate NEXT milestone id.";
				return false;
			}
		}
		for (int task_index = 0; task_index < milestone.tasks.size(); task_index++) {
			AINextTask &task = milestone.tasks.write[task_index];
			if (task.id.strip_edges().is_empty()) {
				task.id = _format_next_id("task", generated_task_number++);
			}
			if (task.title.strip_edges().is_empty()) {
				r_error = "Task title is required.";
				return false;
			}
			if (task.assigned_agent_id.strip_edges().is_empty()) {
				r_error = "Task assigned_agent_id is required.";
				return false;
			}
		}
		parsed_milestones.push_back(milestone);
	}

	milestones = parsed_milestones;
	active_milestone_id = milestones.is_empty() ? String() : milestones[0].id;
	for (AINextMilestone &milestone : milestones) {
		_refresh_milestone_status(milestone);
	}
	_sync_id_counters();
	return true;
}

Dictionary AINextProjectState::to_dict() const {
	Dictionary dict;
	dict["brief"] = brief;
	dict["session_state"] = ai_next_session_state_to_string(session_state);
	dict["active_milestone_id"] = active_milestone_id;
	dict["last_error"] = last_error;
	dict["milestones"] = get_milestones_as_array();

	Array asset_array;
	for (const AINextAssetRecord &asset : assets) {
		asset_array.push_back(ai_next_asset_record_to_dict(asset));
	}
	dict["assets"] = asset_array;
	return dict;
}

void AINextProjectState::load_from_dict(const Dictionary &p_dict) {
	clear();
	brief = String(p_dict.get("brief", String()));
	session_state = ai_next_session_state_from_string(String(p_dict.get("session_state", "idle")));
	active_milestone_id = String(p_dict.get("active_milestone_id", String()));
	last_error = String(p_dict.get("last_error", String()));

	if (p_dict.has("milestones") && Variant(p_dict["milestones"]).get_type() == Variant::ARRAY) {
		Array milestone_array = p_dict["milestones"];
		for (int i = 0; i < milestone_array.size(); i++) {
			if (Variant(milestone_array[i]).get_type() == Variant::DICTIONARY) {
				milestones.push_back(ai_next_milestone_from_dict(milestone_array[i]));
			}
		}
	}
	if (p_dict.has("assets") && Variant(p_dict["assets"]).get_type() == Variant::ARRAY) {
		Array asset_array = p_dict["assets"];
		for (int i = 0; i < asset_array.size(); i++) {
			if (Variant(asset_array[i]).get_type() == Variant::DICTIONARY) {
				assets.push_back(ai_next_asset_record_from_dict(asset_array[i]));
			}
		}
	}
	if (active_milestone_id.is_empty() && !milestones.is_empty()) {
		active_milestone_id = milestones[0].id;
	}
	_sync_id_counters();
}
