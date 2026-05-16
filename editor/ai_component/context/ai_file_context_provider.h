/**************************************************************************/
/*  ai_file_context_provider.h                                             */
/**************************************************************************/

#pragma once

#include "core/templates/hash_set.h"

#include "editor/ai_component/context/ai_context_provider.h"

class AIFileContextProvider : public AIContextProvider {
	GDCLASS(AIFileContextProvider, AIContextProvider);

	PackedStringArray file_paths;
	int max_file_bytes = 256 * 1024;

	bool _is_allowed_path(const String &p_path) const;
	bool _is_allowed_extension(const String &p_path) const;

protected:
	static void _bind_methods();

public:
	void set_file_paths(const PackedStringArray &p_paths);
	void set_max_file_bytes(int p_max_file_bytes);
	Array collect_context() override;
};
