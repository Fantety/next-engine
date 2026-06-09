/**************************************************************************/
/*  ai_project_tool_utils.h                                               */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

class AIProjectToolUtils {
public:
	static bool is_allowed_path(const String &p_path);
	static bool is_allowed_text_extension(const String &p_path);
	static bool is_allowed_image_extension(const String &p_path);
	static String get_path_argument(const Dictionary &p_arguments, const String &p_default_path = "res://");
	static int get_int_argument(const Dictionary &p_arguments, const String &p_key, int p_default_value, int p_min_value, int p_max_value);
	static void refresh_editor_file_system(const String &p_path, bool p_update_file);
};
