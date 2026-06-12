/**************************************************************************/
/*  ai_diff_service.h                                                      */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

class AIDiffService {
public:
	static Dictionary build_text_change(const String &p_path, const String &p_change_type, const String &p_old_text, const String &p_new_text, const String &p_language = String(), const Dictionary &p_metadata = Dictionary());
	static String build_unified_diff(const String &p_path, const String &p_old_text, const String &p_new_text, int &r_added_lines, int &r_removed_lines);
};
