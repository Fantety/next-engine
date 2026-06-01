/**************************************************************************/
/*  ai_next_project_store.cpp                                             */
/**************************************************************************/

#include "ai_next_project_store.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/variant/variant.h"

void AINextProjectStore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_base_dir_for_test", "base_dir"), &AINextProjectStore::set_base_dir_for_test);
	ClassDB::bind_method(D_METHOD("get_base_dir_for_test"), &AINextProjectStore::get_base_dir_for_test);
	ClassDB::bind_method(D_METHOD("get_project_path_for_test", "project_key"), &AINextProjectStore::get_project_path_for_test);
	ClassDB::bind_method(D_METHOD("delete_project_for_test", "project_key"), &AINextProjectStore::delete_project_for_test);
	ClassDB::bind_method(D_METHOD("save", "project_key", "state"), &AINextProjectStore::save);
	ClassDB::bind_method(D_METHOD("load", "project_key"), &AINextProjectStore::load);
}

AINextProjectStore::AINextProjectStore() {
	base_dir = "user://ai_agent/next";
}

String AINextProjectStore::_get_project_path(const String &p_project_key) const {
	return _get_file_path(p_project_key);
}

void AINextProjectStore::set_base_dir_for_test(const String &p_base_dir) {
	base_dir = p_base_dir;
}

String AINextProjectStore::get_base_dir_for_test() const {
	return base_dir;
}

String AINextProjectStore::get_project_path_for_test(const String &p_project_key) const {
	return _get_project_path(p_project_key);
}

bool AINextProjectStore::delete_project_for_test(const String &p_project_key) const {
	const String path = _get_project_path(p_project_key);
	if (!FileAccess::exists(path)) {
		return false;
	}
	return DirAccess::remove_absolute(path) == OK;
}

String AINextProjectStore::save(const String &p_project_key, const Ref<AINextProjectState> &p_state) const {
	if (p_state.is_null()) {
		return "NEXT project state is null.";
	}

	Error err = _ensure_base_dir();
	if (err != OK) {
		return vformat("Failed to create NEXT project state directory (error %d).", err);
	}

	const String path = _get_project_path(p_project_key);
	const String temp_path = path + ".tmp";
	Dictionary root = p_state->to_dict();

	Ref<FileAccess> file = FileAccess::open(temp_path, FileAccess::WRITE, &err);
	if (file.is_null() || err != OK) {
		return vformat("Failed to open NEXT project state file for writing (error %d).", err);
	}
	file->store_string(JSON::stringify(root, "\t"));
	file.unref();

	if (FileAccess::exists(path)) {
		err = DirAccess::remove_absolute(path);
		if (err != OK) {
			DirAccess::remove_absolute(temp_path);
			return vformat("Failed to replace existing NEXT project state (error %d).", err);
		}
	}

	err = DirAccess::rename_absolute(temp_path, path);
	if (err != OK) {
		DirAccess::remove_absolute(temp_path);
		return vformat("Failed to finalize NEXT project state write (error %d).", err);
	}

	return String();
}

Ref<AINextProjectState> AINextProjectStore::load(const String &p_project_key) const {
	Ref<AINextProjectState> state;
	state.instantiate();

	const String path = _get_project_path(p_project_key);
	if (!FileAccess::exists(path)) {
		return state;
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		return state;
	}

	Ref<JSON> json;
	json.instantiate();
	err = json->parse(file->get_as_text());
	if (err != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		return state;
	}

	state->load_from_dict(json->get_data());
	return state;
}
