/**************************************************************************/
/*  ai_next_marquee_settings.h                                            */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"

struct AINextMarqueePreset {
	String id;
	String display_name;
	String shader_code;
	bool custom = false;
};

class AINextMarqueeSettings {
	static String _get_preset_path();
	static String _get_custom_shader_path();
	static String _get_default_preset_id();
	static String _get_default_custom_shader_code();

public:
	static constexpr const char *CUSTOM_PRESET_ID = "custom";

	static Vector<AINextMarqueePreset> get_presets();
	static AINextMarqueePreset get_preset(const String &p_preset_id);
	static bool is_valid_preset_id(const String &p_preset_id);
	static String get_current_preset_id();
	static bool set_current_preset_id(const String &p_preset_id);
	static String get_custom_shader_code();
	static bool set_custom_shader_code(const String &p_shader_code);
	static String get_effective_shader_code();
};
