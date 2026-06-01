/**************************************************************************/
/*  ai_storage_base.cpp                                                   */
/**************************************************************************/

#include "ai_storage_base.h"

#include "core/io/dir_access.h"

void AIStorageBase::_bind_methods() {
}

Error AIStorageBase::_ensure_base_dir() const {
	return DirAccess::make_dir_recursive_absolute(base_dir);
}

String AIStorageBase::_sanitize_path_segment(const String &p_segment, const String &p_fallback) {
	String segment = p_segment.strip_edges();
	if (segment.is_empty()) {
		segment = p_fallback;
	}
	return segment.validate_filename();
}

String AIStorageBase::_get_file_path(const String &p_id, const String &p_extension, bool p_sanitize_id, const String &p_fallback) const {
	const String id = p_sanitize_id ? _sanitize_path_segment(p_id, p_fallback) : p_id;
	return base_dir.path_join(id + p_extension);
}
