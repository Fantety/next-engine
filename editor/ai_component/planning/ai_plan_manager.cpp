/**************************************************************************/
/*  ai_plan_manager.cpp                                                    */
/**************************************************************************/

#include "ai_plan_manager.h"

#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/variant/variant.h"

void AIPlanManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("has_active_plan"), &AIPlanManager::has_active_plan);
	ClassDB::bind_method(D_METHOD("get_active_plan_id"), &AIPlanManager::get_active_plan_id);
	ClassDB::bind_method(D_METHOD("get_active_plan"), &AIPlanManager::get_active_plan);
	ClassDB::bind_method(D_METHOD("get_archived_plan"), &AIPlanManager::get_archived_plan);

	ADD_SIGNAL(MethodInfo("plan_changed", PropertyInfo(Variant::DICTIONARY, "plan")));
	ADD_SIGNAL(MethodInfo("plan_archived", PropertyInfo(Variant::DICTIONARY, "plan")));
}

Ref<AIPlanManager> AIPlanManager::get_singleton() {
	if (singleton.is_null()) {
		singleton.instantiate();
	}
	return singleton;
}

String AIPlanManager::_normalize_status(const String &p_status) {
	const String status = p_status.strip_edges().to_lower();
	if (status == "in_progress" || status == "completed") {
		return status;
	}
	return "pending";
}

Dictionary AIPlanManager::_task_to_dict(const AIPlanTask &p_task) {
	Dictionary task;
	task["id"] = p_task.id;
	task["title"] = p_task.title;
	task["status"] = _normalize_status(p_task.status);
	return task;
}

AIPlanTask AIPlanManager::_task_from_dict(const Dictionary &p_task) const {
	AIPlanTask task;
	task.id = String(p_task.get("id", String())).strip_edges();
	task.title = String(p_task.get("title", String())).strip_edges();
	task.status = _normalize_status(String(p_task.get("status", "pending")));
	return task;
}

String AIPlanManager::_make_plan_id() const {
	return "plan:" + String::num_uint64(OS::get_singleton()->get_ticks_usec()) + ":" + itos(Math::rand());
}

String AIPlanManager::_make_task_id(int p_index) const {
	return "task:" + itos(p_index + 1);
}

void AIPlanManager::_emit_plan_changed() {
	emit_signal(SNAME("plan_changed"), get_active_plan());
}

void AIPlanManager::_archive_active_plan() {
	if (active_plan_id.is_empty()) {
		return;
	}

	archived_plan = get_active_plan();
	emit_signal(SNAME("plan_archived"), archived_plan);

	active_plan_id.clear();
	active_title.clear();
	active_tasks.clear();
	_emit_plan_changed();
}

bool AIPlanManager::has_active_plan() const {
	return !active_plan_id.is_empty();
}

String AIPlanManager::get_active_plan_id() const {
	return active_plan_id;
}

Dictionary AIPlanManager::get_active_plan() const {
	Dictionary plan;
	if (active_plan_id.is_empty()) {
		return plan;
	}

	plan["id"] = active_plan_id;
	plan["title"] = active_title;

	Array tasks;
	for (int i = 0; i < active_tasks.size(); i++) {
		tasks.push_back(_task_to_dict(active_tasks[i]));
	}
	plan["tasks"] = tasks;
	return plan;
}

Dictionary AIPlanManager::get_archived_plan() const {
	return archived_plan.duplicate(true);
}

void AIPlanManager::clear_for_test() {
	active_plan_id.clear();
	active_title.clear();
	active_tasks.clear();
	archived_plan.clear();
	_emit_plan_changed();
}

bool AIPlanManager::create_plan(const String &p_title, const Array &p_tasks, String &r_error) {
	r_error.clear();
	if (has_active_plan()) {
		r_error = "An active plan already exists.";
		return false;
	}
	if (p_tasks.is_empty()) {
		r_error = "Plan requires at least one task.";
		return false;
	}

	Vector<AIPlanTask> tasks;
	for (int i = 0; i < p_tasks.size(); i++) {
		AIPlanTask task;
		if (Variant(p_tasks[i]).get_type() == Variant::DICTIONARY) {
			task = _task_from_dict(p_tasks[i]);
		} else {
			task.title = String(p_tasks[i]).strip_edges();
			task.status = "pending";
		}

		if (task.title.is_empty()) {
			continue;
		}
		if (task.id.is_empty()) {
			task.id = _make_task_id(tasks.size());
		}
		tasks.push_back(task);
	}

	if (tasks.is_empty()) {
		r_error = "Plan requires at least one non-empty task.";
		return false;
	}

	active_plan_id = _make_plan_id();
	active_title = p_title.strip_edges();
	if (active_title.is_empty()) {
		active_title = "Plan";
	}
	active_tasks = tasks;
	_emit_plan_changed();
	return true;
}

bool AIPlanManager::update_task(const String &p_task_id, const String &p_status, String &r_error) {
	r_error.clear();
	if (!has_active_plan()) {
		r_error = "No active plan exists.";
		return false;
	}

	const String task_id = p_task_id.strip_edges();
	if (task_id.is_empty()) {
		r_error = "Task id is required.";
		return false;
	}

	const String status = p_status.strip_edges().to_lower();
	if (status != "pending" && status != "in_progress" && status != "completed") {
		r_error = "Unsupported task status.";
		return false;
	}

	bool found = false;
	for (int i = 0; i < active_tasks.size(); i++) {
		if (active_tasks[i].id != task_id) {
			continue;
		}
		active_tasks.write[i].status = status;
		found = true;
		break;
	}

	if (!found) {
		r_error = "Task id was not found.";
		return false;
	}

	bool all_completed = true;
	for (int i = 0; i < active_tasks.size(); i++) {
		if (active_tasks[i].status != "completed") {
			all_completed = false;
			break;
		}
	}

	if (all_completed) {
		_archive_active_plan();
	} else {
		_emit_plan_changed();
	}
	return true;
}

bool AIPlanManager::archive_plan(String &r_error) {
	r_error.clear();
	if (!has_active_plan()) {
		r_error = "No active plan exists.";
		return false;
	}
	_archive_active_plan();
	return true;
}
