/**************************************************************************/
/*  ai_storage_base.h                                                     */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"

class AIStorageBase : public RefCounted {
	GDCLASS(AIStorageBase, RefCounted);

protected:
	String base_dir;

	static void _bind_methods();

	Error _ensure_base_dir() const;
	static String _sanitize_path_segment(const String &p_segment, const String &p_fallback = "global");
	String _get_file_path(const String &p_id, const String &p_extension = ".json", bool p_sanitize_id = true, const String &p_fallback = "global") const;
};
