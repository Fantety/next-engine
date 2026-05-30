/**************************************************************************/
/*  ai_next_agent_settings.h                                              */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"

class AINextAgentSettings {
	static String _get_agent_model_path(const String &p_agent_id);
	static bool _is_valid_agent_id(const String &p_agent_id);
	static String _get_first_enabled_model_profile_id();

public:
	static Vector<String> get_agent_ids();
	static String get_agent_display_name(const String &p_agent_id);
	static String get_model_profile_id(const String &p_agent_id);
	static String get_effective_model_profile_id(const String &p_agent_id);
	static bool set_model_profile_id(const String &p_agent_id, const String &p_model_profile_id);

	static Dictionary get_agent_model_storage_for_test();
	static void set_agent_model_storage_for_test(const Dictionary &p_storage);
	static void clear_agent_models_for_test();
};
