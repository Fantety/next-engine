/**************************************************************************/
/*  ai_next_project_memory_store.h                                        */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "editor/ai_component/storage/ai_storage_base.h"

struct AINextProjectMemory {
	String language;
	String renderer;
	Vector<String> architecture_notes;
	Vector<String> coding_conventions;
	Vector<String> scene_conventions;
	Vector<String> user_preferences;
	uint64_t updated_at = 0;

	bool is_empty() const;
	Dictionary to_dict() const;
	void load_from_dict(const Dictionary &p_dict);
};

class AINextProjectMemoryStore : public AIStorageBase {
	GDCLASS(AINextProjectMemoryStore, AIStorageBase);

	String _get_memory_path() const;

protected:
	static void _bind_methods();

public:
	AINextProjectMemoryStore();

	void set_project_scope(const String &p_project_scope_key);
	void set_base_dir_for_test(const String &p_base_dir);
	String get_base_dir_for_test() const;
	String get_memory_path_for_test() const;

	Error save_memory(AINextProjectMemory p_memory) const;
	bool load_memory(AINextProjectMemory &r_memory) const;
	Dictionary load_memory_dict() const;
	Error save_memory_dict(const Dictionary &p_memory) const;
};
