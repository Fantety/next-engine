/**************************************************************************/
/*  ai_local_settings_store.cpp                                           */
/**************************************************************************/

#include "ai_local_settings_store.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/variant/variant.h"

void AILocalSettingsStore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_settings_path", "path"), &AILocalSettingsStore::set_settings_path);
	ClassDB::bind_method(D_METHOD("get_settings_path"), &AILocalSettingsStore::get_settings_path);
	ClassDB::bind_method(D_METHOD("get_settings"), &AILocalSettingsStore::get_settings);
	ClassDB::bind_method(D_METHOD("update_settings", "patch"), &AILocalSettingsStore::update_settings);
	ClassDB::bind_method(D_METHOD("clear_settings"), &AILocalSettingsStore::clear_settings);

	ADD_SIGNAL(MethodInfo("settings_changed", PropertyInfo(Variant::DICTIONARY, "settings")));
}

AILocalSettingsStore::AILocalSettingsStore() {
	settings_path = "user://agent_v1/settings.v3.json";
}

Dictionary AILocalSettingsStore::_merge_dicts(const Dictionary &p_base, const Dictionary &p_patch) {
	Dictionary result = p_base.duplicate(true);
	for (const KeyValue<Variant, Variant> &kv : p_patch) {
		if (kv.value.get_type() == Variant::DICTIONARY && result.get(kv.key, Variant()).get_type() == Variant::DICTIONARY) {
			result[kv.key] = _merge_dicts(Dictionary(result[kv.key]), Dictionary(kv.value));
		} else {
			result[kv.key] = kv.value;
		}
	}
	return result;
}

bool AILocalSettingsStore::_read_settings_file(const String &p_path, Dictionary &r_settings, AIError &r_error) {
	r_settings = Dictionary();
	if (p_path.strip_edges().is_empty() || !FileAccess::exists(p_path)) {
		return true;
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to open local settings file: " + p_path);
		return false;
	}

	Ref<JSON> json;
	json.instantiate();
	const Error parse_err = json->parse(file->get_as_text());
	if (parse_err != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Failed to parse local settings file: " + p_path);
		return false;
	}

	r_settings = Dictionary(json->get_data()).duplicate(true);
	return true;
}

bool AILocalSettingsStore::_write_settings_file(const String &p_path, const Dictionary &p_settings, AIError &r_error) {
	if (p_path.strip_edges().is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Local settings path is required.");
		return false;
	}

	const String base_dir = p_path.get_base_dir();
	if (!base_dir.is_empty() && DirAccess::make_dir_recursive_absolute(base_dir) != OK) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to create local settings directory: " + base_dir);
		return false;
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	if (file.is_null() || err != OK) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to write local settings file: " + p_path);
		return false;
	}

	file->store_string(JSON::stringify(p_settings, "\t") + "\n");
	file->flush();
	return true;
}

void AILocalSettingsStore::set_settings_path(const String &p_path) {
	MutexLock lock(mutex);
	settings_path = p_path.strip_edges();
}

String AILocalSettingsStore::get_settings_path() const {
	MutexLock lock(mutex);
	return settings_path;
}

Dictionary AILocalSettingsStore::get_settings() {
	MutexLock lock(mutex);
	Dictionary settings;
	AIError error;
	if (!_read_settings_file(settings_path, settings, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	settings["success"] = true;
	return settings;
}

Dictionary AILocalSettingsStore::update_settings(const Dictionary &p_patch) {
	Dictionary updated;
	{
		MutexLock lock(mutex);
		Dictionary existing;
		AIError error;
		if (!_read_settings_file(settings_path, existing, error)) {
			Dictionary result;
			result["success"] = false;
			result["error"] = error.to_dictionary();
			return result;
		}

		updated = _merge_dicts(existing, p_patch);
		updated["version"] = 3;
		if (!_write_settings_file(settings_path, updated, error)) {
			Dictionary result;
			result["success"] = false;
			result["error"] = error.to_dictionary();
			return result;
		}
	}

	Dictionary result = updated.duplicate(true);
	result["success"] = true;
	call_deferred("emit_signal", SNAME("settings_changed"), result);
	return result;
}

Dictionary AILocalSettingsStore::clear_settings() {
	Dictionary empty;
	empty["version"] = 3;
	{
		MutexLock lock(mutex);
		AIError error;
		if (!_write_settings_file(settings_path, empty, error)) {
			Dictionary result;
			result["success"] = false;
			result["error"] = error.to_dictionary();
			return result;
		}
	}

	Dictionary result = empty.duplicate(true);
	result["success"] = true;
	call_deferred("emit_signal", SNAME("settings_changed"), result);
	return result;
}
