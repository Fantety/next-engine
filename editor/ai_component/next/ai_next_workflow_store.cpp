/**************************************************************************/
/*  ai_next_workflow_store.cpp                                            */
/**************************************************************************/

#include "ai_next_workflow_store.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/time.h"
#include "core/variant/variant.h"

static bool _ai_next_workflow_updated_desc(const Variant &p_a, const Variant &p_b) {
	Dictionary a = p_a;
	Dictionary b = p_b;
	return uint64_t(a.get("updated_at", 0)) > uint64_t(b.get("updated_at", 0));
}

void AINextWorkflowStore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("list_workflows"), &AINextWorkflowStore::list_workflows);
	ClassDB::bind_method(D_METHOD("delete_workflow", "workflow_id"), &AINextWorkflowStore::delete_workflow);
}

Error AINextWorkflowStore::_ensure_base_dir() const {
	return DirAccess::make_dir_recursive_absolute(base_dir);
}

String AINextWorkflowStore::_get_workflow_path(const String &p_workflow_id) const {
	return base_dir.path_join(_sanitize_workflow_id(p_workflow_id) + ".json");
}

String AINextWorkflowStore::_sanitize_scope_key(const String &p_scope_key) {
	String scope_key = p_scope_key.strip_edges();
	if (scope_key.is_empty()) {
		scope_key = "global";
	}
	return scope_key.validate_filename();
}

String AINextWorkflowStore::_sanitize_workflow_id(const String &p_workflow_id) {
	return p_workflow_id.strip_edges().validate_filename();
}

void AINextWorkflowStore::set_project_scope(const String &p_project_scope_key) {
	base_dir = String("user://ai_agent/projects").path_join(_sanitize_scope_key(p_project_scope_key)).path_join("next_workflows");
}

void AINextWorkflowStore::set_base_dir_for_test(const String &p_base_dir) {
	base_dir = p_base_dir;
}

String AINextWorkflowStore::get_base_dir_for_test() const {
	return base_dir;
}

String AINextWorkflowStore::get_workflow_path_for_test(const String &p_workflow_id) const {
	return _get_workflow_path(p_workflow_id);
}

Error AINextWorkflowStore::save_workflow(AINextWorkflowSnapshot p_snapshot) const {
	ERR_FAIL_COND_V(p_snapshot.id.strip_edges().is_empty(), ERR_INVALID_PARAMETER);

	const uint64_t now = Time::get_singleton()->get_unix_time_from_system();
	if (p_snapshot.created_at == 0) {
		p_snapshot.created_at = now;
	}
	if (p_snapshot.updated_at == 0) {
		p_snapshot.updated_at = now;
	}
	if (p_snapshot.title.strip_edges().is_empty()) {
		p_snapshot.title = "New NEXT Workflow";
	}
	if (p_snapshot.project_state.is_null()) {
		p_snapshot.project_state.instantiate();
	}

	Error err = _ensure_base_dir();
	ERR_FAIL_COND_V(err != OK, err);

	const String path = _get_workflow_path(p_snapshot.id);
	const String temp_path = path + ".tmp";
	Ref<FileAccess> file = FileAccess::open(temp_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V(file.is_null() || err != OK, err);
	file->store_string(JSON::stringify(p_snapshot.to_dict(), "\t"));
	file.unref();

	if (FileAccess::exists(path)) {
		err = DirAccess::remove_absolute(path);
		if (err != OK) {
			DirAccess::remove_absolute(temp_path);
			return err;
		}
	}

	err = DirAccess::rename_absolute(temp_path, path);
	if (err != OK) {
		DirAccess::remove_absolute(temp_path);
	}
	return err;
}

bool AINextWorkflowStore::load_workflow(const String &p_workflow_id, AINextWorkflowSnapshot &r_snapshot) const {
	if (p_workflow_id.strip_edges().is_empty()) {
		return false;
	}

	const String path = _get_workflow_path(p_workflow_id);
	if (!FileAccess::exists(path)) {
		return false;
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		return false;
	}

	Ref<JSON> json;
	json.instantiate();
	err = json->parse(file->get_as_text());
	if (err != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		return false;
	}

	return r_snapshot.load_from_dict(json->get_data());
}

bool AINextWorkflowStore::load_workflow_metadata(const String &p_workflow_id, Dictionary &r_metadata) const {
	AINextWorkflowSnapshot snapshot;
	if (!load_workflow(p_workflow_id, snapshot)) {
		return false;
	}
	r_metadata = snapshot.to_metadata().to_dict();
	return true;
}

bool AINextWorkflowStore::delete_workflow(const String &p_workflow_id) const {
	if (p_workflow_id.strip_edges().is_empty()) {
		return false;
	}

	const String path = _get_workflow_path(p_workflow_id);
	if (!FileAccess::exists(path)) {
		return false;
	}
	return DirAccess::remove_absolute(path) == OK;
}

bool AINextWorkflowStore::get_most_recent_workflow_id(String &r_workflow_id) const {
	Array workflows = list_workflows();
	if (workflows.is_empty() || Variant(workflows[0]).get_type() != Variant::DICTIONARY) {
		return false;
	}

	Dictionary item = workflows[0];
	r_workflow_id = String(item.get("id", String()));
	return !r_workflow_id.is_empty();
}

Array AINextWorkflowStore::list_workflows() const {
	Array workflows;
	Ref<DirAccess> dir = DirAccess::open(base_dir);
	if (dir.is_null()) {
		return workflows;
	}

	dir->list_dir_begin();
	String entry = dir->get_next();
	while (!entry.is_empty()) {
		if (!dir->current_is_dir() && entry.get_extension().to_lower() == "json") {
			Dictionary item_metadata;
			const String workflow_id = entry.get_basename();
			if (load_workflow_metadata(workflow_id, item_metadata)) {
				item_metadata["path"] = base_dir.path_join(entry);
				workflows.push_back(item_metadata);
			}
		}
		entry = dir->get_next();
	}
	dir->list_dir_end();
	workflows.sort_custom(callable_mp_static(_ai_next_workflow_updated_desc));
	return workflows;
}
