/**************************************************************************/
/*  ai_next_project_store.h                                               */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "editor/ai_component/next/ai_next_project_state.h"
#include "editor/ai_component/storage/ai_storage_base.h"

class AINextProjectStore : public AIStorageBase {
	GDCLASS(AINextProjectStore, AIStorageBase);

	String _get_project_path(const String &p_project_key) const;

protected:
	static void _bind_methods();

public:
	AINextProjectStore();

	void set_base_dir_for_test(const String &p_base_dir);
	String get_base_dir_for_test() const;
	String get_project_path_for_test(const String &p_project_key) const;
	bool delete_project_for_test(const String &p_project_key) const;

	String save(const String &p_project_key, const Ref<AINextProjectState> &p_state) const;
	Ref<AINextProjectState> load(const String &p_project_key) const;
};
