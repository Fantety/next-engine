/**************************************************************************/
/*  ai_project_tree_context_provider.h                                    */
/**************************************************************************/

#pragma once

#include "core/io/dir_access.h"
#include "core/string/string_builder.h"

#include "editor/ai_component/context/ai_context_provider.h"

class AIProjectTreeContextProvider : public AIContextProvider {
	GDCLASS(AIProjectTreeContextProvider, AIContextProvider);

	int max_depth = 3;
	int max_entries = 300;
	int max_chars = 12000;
	bool include_hidden = false;
	bool truncated = false;
	int entry_count = 0;

	void _append_tree(StringBuilder &r_builder, const String &p_path, int p_depth);

protected:
	static void _bind_methods();

public:
	void set_limits(int p_max_depth, int p_max_entries, int p_max_chars);
	Array collect_context() override;
};
