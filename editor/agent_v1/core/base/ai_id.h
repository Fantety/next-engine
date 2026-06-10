/**************************************************************************/
/*  ai_id.h                                                               */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"

class AIId {
public:
	static String make(const String &p_prefix = String());
	static bool is_valid_name(const String &p_name, int p_max_length = 64);
};
