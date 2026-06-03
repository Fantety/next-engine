/**************************************************************************/
/*  ai_next_project_memory_store.cpp                                      */
/**************************************************************************/

#include "ai_next_project_memory_store.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/os/time.h"
#include "core/variant/variant.h"
#include "editor/ai_component/next/ai_next_types.h"

bool AINextProjectMemory::is_empty() const {
	return language.strip_edges().is_empty() &&
			renderer.strip_edges().is_empty() &&
			architecture_notes.is_empty() &&
			coding_conventions.is_empty() &&
			scene_conventions.is_empty() &&
			user_preferences.is_empty();
}

Dictionary AINextProjectMemory::to_dict() const {
	Dictionary dict;
	dict["language"] = language;
	dict["renderer"] = renderer;
	dict["architecture_notes"] = ai_next_string_vector_to_array(architecture_notes);
	dict["coding_conventions"] = ai_next_string_vector_to_array(coding_conventions);
	dict["scene_conventions"] = ai_next_string_vector_to_array(scene_conventions);
	dict["user_preferences"] = ai_next_string_vector_to_array(user_preferences);
	dict["updated_at"] = updated_at;
	return dict;
}

void AINextProjectMemory::load_from_dict(const Dictionary &p_dict) {
	language = String(p_dict.get("language", String()));
	renderer = String(p_dict.get("renderer", String()));
	architecture_notes.clear();
	coding_conventions.clear();
	scene_conventions.clear();
	user_preferences.clear();
	if (p_dict.has("architecture_notes") && Variant(p_dict["architecture_notes"]).get_type() == Variant::ARRAY) {
		architecture_notes = ai_next_array_to_string_vector(p_dict["architecture_notes"]);
	}
	if (p_dict.has("coding_conventions") && Variant(p_dict["coding_conventions"]).get_type() == Variant::ARRAY) {
		coding_conventions = ai_next_array_to_string_vector(p_dict["coding_conventions"]);
	}
	if (p_dict.has("scene_conventions") && Variant(p_dict["scene_conventions"]).get_type() == Variant::ARRAY) {
		scene_conventions = ai_next_array_to_string_vector(p_dict["scene_conventions"]);
	}
	if (p_dict.has("user_preferences") && Variant(p_dict["user_preferences"]).get_type() == Variant::ARRAY) {
		user_preferences = ai_next_array_to_string_vector(p_dict["user_preferences"]);
	}
	updated_at = (uint64_t)(int64_t)p_dict.get("updated_at", 0);
}

void AINextProjectMemoryStore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_memory_dict"), &AINextProjectMemoryStore::load_memory_dict);
	ClassDB::bind_method(D_METHOD("save_memory_dict", "memory"), &AINextProjectMemoryStore::save_memory_dict);
}

AINextProjectMemoryStore::AINextProjectMemoryStore() {
	base_dir = "user://ai_agent/projects/global";
}

String AINextProjectMemoryStore::_get_memory_path() const {
	return base_dir.path_join("next_memory.json");
}

void AINextProjectMemoryStore::set_project_scope(const String &p_project_scope_key) {
	base_dir = String("user://ai_agent/projects").path_join(_sanitize_path_segment(p_project_scope_key));
}

void AINextProjectMemoryStore::set_base_dir_for_test(const String &p_base_dir) {
	base_dir = p_base_dir;
}

String AINextProjectMemoryStore::get_base_dir_for_test() const {
	return base_dir;
}

String AINextProjectMemoryStore::get_memory_path_for_test() const {
	return _get_memory_path();
}

Error AINextProjectMemoryStore::save_memory(AINextProjectMemory p_memory) const {
	p_memory.updated_at = Time::get_singleton()->get_unix_time_from_system();

	Error err = _ensure_base_dir();
	ERR_FAIL_COND_V(err != OK, err);

	const String path = _get_memory_path();
	const String temp_path = path + ".tmp";
	Ref<FileAccess> file = FileAccess::open(temp_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V(file.is_null() || err != OK, err);
	file->store_string(JSON::stringify(p_memory.to_dict(), "\t"));
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

bool AINextProjectMemoryStore::load_memory(AINextProjectMemory &r_memory) const {
	const String path = _get_memory_path();
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

	r_memory.load_from_dict(json->get_data());
	return true;
}

Dictionary AINextProjectMemoryStore::load_memory_dict() const {
	AINextProjectMemory memory;
	if (!load_memory(memory)) {
		return Dictionary();
	}
	return memory.to_dict();
}

Error AINextProjectMemoryStore::save_memory_dict(const Dictionary &p_memory) const {
	AINextProjectMemory memory;
	memory.load_from_dict(p_memory);
	return save_memory(memory);
}
