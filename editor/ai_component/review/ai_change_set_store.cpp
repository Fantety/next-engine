/**************************************************************************/
/*  ai_change_set_store.cpp                                                */
/**************************************************************************/

#include "ai_change_set_store.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "editor/ai_component/review/ai_diff_service.h"
#include "editor/ai_component/tools/project/ai_project_tool_utils.h"
#include "editor/file_system/editor_file_system.h"
#include "scene/resources/shader.h"

void AIChangeSetStore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_emit_changed_deferred"), &AIChangeSetStore::_emit_changed_deferred);

	ADD_SIGNAL(MethodInfo("changed"));
}

Ref<AIChangeSetStore> AIChangeSetStore::get_singleton() {
	if (singleton.is_null()) {
		singleton.instantiate();
	}
	return singleton;
}

String AIChangeSetStore::_get_current_project_scope_key() {
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (!project_settings) {
		return "global";
	}
	const String resource_path = project_settings->get_resource_path();
	return resource_path.is_empty() ? String("global") : resource_path.md5_text();
}

String AIChangeSetStore::_make_change_set_id() {
	return OS::get_singleton()->get_unique_id() + "_" + itos(Time::get_singleton()->get_unix_time_from_system()) + "_" + itos(Math::rand());
}

bool AIChangeSetStore::_read_text_file(const String &p_path, String &r_text, String &r_error) {
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		r_error = vformat("Failed to read `%s` (error %d).", p_path, err);
		return false;
	}
	r_text = file->get_as_text();
	return true;
}

bool AIChangeSetStore::_ensure_parent_directory(const String &p_path, String &r_error) {
	const String base_dir = p_path.get_base_dir();
	if (base_dir.is_empty() || base_dir == "res://") {
		return true;
	}
	const String absolute_dir = ProjectSettings::get_singleton()->globalize_path(base_dir);
	const Error err = DirAccess::make_dir_recursive_absolute(absolute_dir);
	if (err != OK) {
		r_error = vformat("Failed to create directory `%s` (error %d).", base_dir, err);
		return false;
	}
	return true;
}

bool AIChangeSetStore::_save_text_resource(const String &p_path, const String &p_text, String &r_error) {
	if (!_ensure_parent_directory(p_path, r_error)) {
		return false;
	}

	const String extension = p_path.get_extension().to_lower();
	if (extension == "gd") {
		Ref<Script> script = ResourceLoader::load(p_path, "Script", ResourceLoader::CACHE_MODE_REPLACE);
		if (script.is_null()) {
			ScriptLanguage *language = ScriptServer::get_language_for_extension(extension);
			if (!language) {
				r_error = "GDScript language is not available.";
				return false;
			}
			script = language->make_template(String(), p_path.get_file().get_basename(), "Node");
		}
		if (script.is_null()) {
			r_error = vformat("Failed to create script resource `%s`.", p_path);
			return false;
		}
		script->set_source_code(p_text);
		script->set_path(p_path);
		const Error err = ResourceSaver::save(script, p_path, ResourceSaver::FLAG_CHANGE_PATH);
		if (err != OK) {
			r_error = vformat("Failed to save script `%s` (error %d).", p_path, err);
			return false;
		}
		_refresh_file_system(p_path);
		return true;
	}

	if (extension == "gdshader") {
		Ref<Shader> shader = ResourceLoader::load(p_path, "Shader", ResourceLoader::CACHE_MODE_REPLACE);
		if (shader.is_null()) {
			shader.instantiate();
		}
		shader->set_code(p_text);
		shader->set_path(p_path, true);
		const Error err = ResourceSaver::save(shader, p_path, ResourceSaver::FLAG_CHANGE_PATH);
		if (err != OK) {
			r_error = vformat("Failed to save shader `%s` (error %d).", p_path, err);
			return false;
		}
		_refresh_file_system(p_path);
		return true;
	}

	r_error = vformat("Review changes do not support writing `%s` files yet.", extension);
	return false;
}

bool AIChangeSetStore::_validate_file_change(const Dictionary &p_change, String &r_error) {
	const String path = p_change.get("path", String());
	const String type = p_change.get("type", String());
	const String new_text = p_change.get("new_text", String());
	if (!AIProjectToolUtils::is_allowed_path(path)) {
		r_error = vformat("Change path `%s` is outside the allowed project boundary.", path);
		return false;
	}

	const bool exists = FileAccess::exists(path);
	if (type == "delete") {
		if (exists) {
			r_error = vformat("File `%s` was recreated after the AI deletion. Review before reverting.", path);
			return false;
		}
		return true;
	}

	if (!exists) {
		r_error = vformat("File `%s` no longer exists. Cannot revert the AI change safely.", path);
		return false;
	}

	String current_text;
	if (!_read_text_file(path, current_text, r_error)) {
		return false;
	}
	if (current_text != new_text) {
		r_error = vformat("File `%s` changed after the AI edit. Review manually before reverting.", path);
		return false;
	}
	return true;
}

bool AIChangeSetStore::_revert_file_change(const Dictionary &p_change, String &r_error) {
	const String path = p_change.get("path", String());
	const String type = p_change.get("type", String());
	if (type == "create") {
		const Error err = DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));
		if (err != OK) {
			r_error = vformat("Failed to remove newly created `%s` (error %d).", path, err);
			return false;
		}
		_refresh_file_system(path);
		return true;
	}

	const String old_text = p_change.get("old_text", String());
	return _save_text_resource(path, old_text, r_error);
}

void AIChangeSetStore::_refresh_file_system(const String &p_path) {
	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(p_path);
		EditorFileSystem::get_singleton()->scan_changes();
	}
}

Dictionary AIChangeSetStore::_merge_file_change(const Dictionary &p_existing_change, const Dictionary &p_next_change) {
	const String path = p_existing_change.get("path", p_next_change.get("path", String()));
	const String old_text = p_existing_change.get("old_text", String());
	const String new_text = p_next_change.get("new_text", String());
	const String language = p_existing_change.get("language", p_next_change.get("language", String()));
	String type = p_existing_change.get("type", String());
	const String next_type = p_next_change.get("type", String());

	if (type == "create") {
		type = next_type == "delete" ? String("delete") : String("create");
	} else if (next_type == "delete") {
		type = "delete";
	} else {
		type = "modify";
	}

	Dictionary metadata = p_existing_change.get("metadata", Dictionary()).duplicate(true);
	Dictionary next_metadata = p_next_change.get("metadata", Dictionary());
	for (const KeyValue<Variant, Variant> &E : next_metadata) {
		metadata[E.key] = E.value;
	}
	metadata["merged_review_change"] = true;

	return AIDiffService::build_text_change(path, type, old_text, new_text, language, metadata);
}

bool AIChangeSetStore::_is_noop_change(const Dictionary &p_change) {
	const String old_text = p_change.get("old_text", String());
	const String new_text = p_change.get("new_text", String());
	return old_text == new_text;
}

int AIChangeSetStore::_find_change_set_index(const String &p_change_set_id) const {
	for (int i = 0; i < change_sets.size(); i++) {
		if (String(change_sets[i].get("id", String())) == p_change_set_id) {
			return i;
		}
	}
	return -1;
}

int AIChangeSetStore::_find_pending_change_set_index_for_file(const String &p_project_scope, const String &p_session_id, const String &p_path) const {
	for (int i = change_sets.size() - 1; i >= 0; i--) {
		const Dictionary &change_set = change_sets[i];
		if (String(change_set.get("project_scope", String())) != p_project_scope) {
			continue;
		}
		if (String(change_set.get("session_id", String())) != p_session_id) {
			continue;
		}
		if (String(change_set.get("status", String())) != "pending") {
			continue;
		}

		Array changes = change_set.get("changes", Array());
		for (int j = 0; j < changes.size(); j++) {
			if (Variant(changes[j]).get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary change = changes[j];
			if (String(change.get("path", String())) == p_path) {
				return i;
			}
		}
	}
	return -1;
}

void AIChangeSetStore::_recalculate_change_set_totals(Dictionary &r_change_set) const {
	int added_lines = 0;
	int removed_lines = 0;
	Array changes = r_change_set.get("changes", Array());
	for (int i = 0; i < changes.size(); i++) {
		if (Variant(changes[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary change = changes[i];
		added_lines += (int)change.get("added_lines", 0);
		removed_lines += (int)change.get("removed_lines", 0);
	}
	r_change_set["added_lines"] = added_lines;
	r_change_set["removed_lines"] = removed_lines;
}

void AIChangeSetStore::_emit_changed_deferred() {
	emit_signal(SNAME("changed"));
}

void AIChangeSetStore::_mark_change_set_status(const String &p_change_set_id, const String &p_status) {
	MutexLock lock(mutex);
	const int index = _find_change_set_index(p_change_set_id);
	if (index >= 0) {
		Dictionary updated = change_sets[index];
		updated["status"] = p_status;
		updated["updated_at"] = Time::get_singleton()->get_unix_time_from_system();
		change_sets.write[index] = updated;
	}
}

String AIChangeSetStore::add_change_set(const String &p_title, const String &p_session_id, const String &p_tool_call_id, const Array &p_changes, const Dictionary &p_metadata) {
	const String project_scope = _get_current_project_scope_key();
	Array new_changes;
	String merged_change_set_id;
	bool changed_existing = false;

	{
		MutexLock lock(mutex);
		for (int change_index = 0; change_index < p_changes.size(); change_index++) {
			if (Variant(p_changes[change_index]).get_type() != Variant::DICTIONARY) {
				continue;
			}

			Dictionary next_change = p_changes[change_index];
			if (_is_noop_change(next_change)) {
				continue;
			}

			const String path = next_change.get("path", String());
			bool merged = false;

			if (!path.is_empty()) {
				const int existing_index = _find_pending_change_set_index_for_file(project_scope, p_session_id, path);
				if (existing_index >= 0) {
					Dictionary updated_change_set = change_sets[existing_index];
					Array existing_changes = updated_change_set.get("changes", Array());
					for (int i = 0; i < existing_changes.size(); i++) {
						if (Variant(existing_changes[i]).get_type() != Variant::DICTIONARY) {
							continue;
						}
						Dictionary existing_change = existing_changes[i];
						if (String(existing_change.get("path", String())) != path) {
							continue;
						}

						Dictionary merged_change = _merge_file_change(existing_change, next_change);
						if (_is_noop_change(merged_change)) {
							existing_changes.remove_at(i);
						} else {
							existing_changes.set(i, merged_change);
						}
						break;
					}

					updated_change_set["tool_call_id"] = p_tool_call_id;
					updated_change_set["updated_at"] = Time::get_singleton()->get_unix_time_from_system();
					updated_change_set["changes"] = existing_changes;
					Dictionary metadata = updated_change_set.get("metadata", Dictionary()).duplicate(true);
					for (const KeyValue<Variant, Variant> &E : p_metadata) {
						metadata[E.key] = E.value;
					}
					metadata["merged_review_change"] = true;
					metadata["last_title"] = p_title;
					metadata["last_tool_call_id"] = p_tool_call_id;
					updated_change_set["metadata"] = metadata;
					_recalculate_change_set_totals(updated_change_set);

					if (existing_changes.is_empty()) {
						updated_change_set["status"] = "kept";
					}
					change_sets.write[existing_index] = updated_change_set;
					if (String(updated_change_set.get("status", String())) == "pending") {
						merged_change_set_id = updated_change_set["id"];
					}
					changed_existing = true;
					merged = true;
				}
			}

			if (!merged) {
				bool merged_into_new = false;
				if (!path.is_empty()) {
					for (int i = 0; i < new_changes.size(); i++) {
						if (Variant(new_changes[i]).get_type() != Variant::DICTIONARY) {
							continue;
						}
						Dictionary existing_new_change = new_changes[i];
						if (String(existing_new_change.get("path", String())) != path) {
							continue;
						}

						Dictionary merged_change = _merge_file_change(existing_new_change, next_change);
						if (_is_noop_change(merged_change)) {
							new_changes.remove_at(i);
						} else {
							new_changes.set(i, merged_change);
						}
						merged_into_new = true;
						break;
					}
				}
				if (!merged_into_new) {
					new_changes.push_back(next_change.duplicate(true));
				}
			}
		}
	}

	if (new_changes.is_empty()) {
		if (changed_existing) {
			call_deferred(SNAME("_emit_changed_deferred"));
			if (!merged_change_set_id.is_empty()) {
				Dictionary merged_change_set = get_change_set(merged_change_set_id);
				if (!merged_change_set.is_empty() && String(merged_change_set.get("status", String())) == "pending") {
					return merged_change_set_id;
				}
			}
		}
		return String();
	}

	Dictionary change_set;
	change_set["id"] = _make_change_set_id();
	change_set["title"] = p_title;
	change_set["session_id"] = p_session_id;
	change_set["tool_call_id"] = p_tool_call_id;
	change_set["status"] = "pending";
	change_set["project_scope"] = project_scope;
	change_set["created_at"] = Time::get_singleton()->get_unix_time_from_system();
	change_set["updated_at"] = change_set["created_at"];
	change_set["changes"] = new_changes;
	change_set["metadata"] = p_metadata.duplicate(true);

	_recalculate_change_set_totals(change_set);

	{
		MutexLock lock(mutex);
		change_sets.push_back(change_set);
	}

	call_deferred(SNAME("_emit_changed_deferred"));
	return change_set["id"];
}

Array AIChangeSetStore::list_change_sets(const String &p_status) const {
	Array list;
	const String project_scope = _get_current_project_scope_key();
	MutexLock lock(mutex);
	for (int i = 0; i < change_sets.size(); i++) {
		const Dictionary &change_set = change_sets[i];
		if (String(change_set.get("project_scope", String())) != project_scope) {
			continue;
		}
		if (!p_status.is_empty() && String(change_set.get("status", String())) != p_status) {
			continue;
		}
		list.push_back(change_set.duplicate(true));
	}
	return list;
}

Dictionary AIChangeSetStore::get_change_set(const String &p_change_set_id) const {
	MutexLock lock(mutex);
	const int index = _find_change_set_index(p_change_set_id);
	if (index < 0) {
		return Dictionary();
	}
	return change_sets[index].duplicate(true);
}

int AIChangeSetStore::get_pending_count() const {
	return list_change_sets("pending").size();
}

bool AIChangeSetStore::keep_change_set(const String &p_change_set_id, String &r_error) {
	Dictionary change_set = get_change_set(p_change_set_id);
	if (change_set.is_empty()) {
		r_error = "Review change set was not found.";
		return false;
	}
	if (String(change_set.get("status", String())) != "pending") {
		r_error = "Only pending review changes can be kept.";
		return false;
	}
	_mark_change_set_status(p_change_set_id, "kept");
	call_deferred(SNAME("_emit_changed_deferred"));
	return true;
}

bool AIChangeSetStore::revert_change_set(const String &p_change_set_id, String &r_error) {
	Dictionary change_set = get_change_set(p_change_set_id);
	if (change_set.is_empty()) {
		r_error = "Review change set was not found.";
		return false;
	}
	if (String(change_set.get("status", String())) != "pending") {
		r_error = "Only pending review changes can be reverted.";
		return false;
	}

	Array changes = change_set.get("changes", Array());
	for (int i = 0; i < changes.size(); i++) {
		if (Variant(changes[i]).get_type() != Variant::DICTIONARY) {
			r_error = "Review change set contains an invalid file change.";
			return false;
		}
		if (!_validate_file_change(changes[i], r_error)) {
			return false;
		}
	}

	for (int i = changes.size() - 1; i >= 0; i--) {
		if (!_revert_file_change(changes[i], r_error)) {
			return false;
		}
	}

	_mark_change_set_status(p_change_set_id, "reverted");

	call_deferred(SNAME("_emit_changed_deferred"));
	return true;
}

void AIChangeSetStore::clear_for_test() {
	MutexLock lock(mutex);
	change_sets.clear();
}
