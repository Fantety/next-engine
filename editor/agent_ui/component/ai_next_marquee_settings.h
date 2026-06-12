/**************************************************************************/
/*  ai_next_marquee_settings.h                                            */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

struct AINextMarqueePreset {
	String id;
	String display_name;
	String shader_code;
	bool custom = false;
};

class AINextMarqueeSettings {
	static String _get_preset_path();
	static String _get_custom_marquees_path();
	static String _get_legacy_custom_shader_path();
	static String _get_default_preset_id();
	static String _get_default_custom_shader_code();
	static Array _get_custom_marquee_storage();
	static void _set_custom_marquee_storage(const Array &p_marquees);
	static AINextMarqueePreset _marquee_from_dictionary(const Dictionary &p_marquee);
	static Dictionary _marquee_to_dictionary(const AINextMarqueePreset &p_marquee);
	static String _make_custom_marquee_id();
	static String _normalize_display_name(const String &p_display_name);
	static Vector<AINextMarqueePreset> _get_builtin_presets();

public:
	static Vector<AINextMarqueePreset> get_presets();
	static AINextMarqueePreset get_preset(const String &p_preset_id);
	static bool is_valid_preset_id(const String &p_preset_id);
	static String get_current_preset_id();
	static bool set_current_preset_id(const String &p_preset_id);
	static String add_custom_marquee(const String &p_display_name, const String &p_shader_code);
	static bool update_custom_marquee(const String &p_marquee_id, const String &p_display_name, const String &p_shader_code);
	static bool remove_custom_marquee(const String &p_marquee_id);
	static String get_custom_shader_code();
	static bool set_custom_shader_code(const String &p_shader_code);
	static String get_effective_shader_code();
	static Array get_custom_marquee_storage_for_test();
	static void set_custom_marquee_storage_for_test(const Array &p_marquees);
	static void clear_custom_marquees_for_test();
};
