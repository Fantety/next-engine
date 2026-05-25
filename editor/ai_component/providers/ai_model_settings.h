/**************************************************************************/
/*  ai_model_settings.h                                                   */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

#include "editor/ai_component/providers/ai_provider_config.h"

struct AIModelProviderPreset {
	String id;
	String display_name;
	String default_base_url;
	Vector<String> preset_models;
};

struct AIModelDescriptor {
	String id;
	String display_name;
	String provider_id;
	String provider_name;
	String model;
	String base_url;
	String api_key;
	bool enabled = false;
	bool custom = false;
	int max_input_chars = 96000;
	int max_context_chars = 24000;
	int max_history_chars = 64000;
	int max_tool_result_chars = 16000;
	int min_recent_messages = 4;
	int max_provider_turns = 255;
	int max_tool_calls = 60;
	int max_output_tokens = 0;
	int timeout_seconds = 180;
};

using AIModelProfile = AIModelDescriptor;

class AIModelSettings {
	static String _get_provider_path(const String &p_provider_id, const String &p_property);
	static String _get_model_path(const String &p_provider_id, const String &p_model);
	static String _get_profiles_path();
	static String _get_legacy_deepseek_value(const String &p_property, const String &p_default_value);
	static bool _is_default_model_enabled(const String &p_provider_id, const String &p_model);
	static PackedStringArray _split_custom_models(const String &p_models);
	static bool _has_profile_storage();
	static Array _get_profile_storage();
	static void _set_profile_storage(const Array &p_profiles);
	static Array _build_legacy_profile_storage();
	static AIModelProviderPreset _get_provider_preset(const String &p_provider_id);
	static AIModelProfile _profile_from_dictionary(const Dictionary &p_profile);
	static Dictionary _profile_to_dictionary(const AIModelProfile &p_profile);
	static String _make_profile_id(const String &p_provider_id, const String &p_model);
	static String _make_profile_display_name(const String &p_provider_name, const String &p_model);

public:
	static Vector<AIModelProviderPreset> get_provider_presets();
	static String get_model_id(const String &p_provider_id, const String &p_model);
	static AIModelDescriptor get_model(const String &p_model_id);
	static Vector<AIModelDescriptor> get_enabled_models();
	static AIProviderConfig get_provider_config(const String &p_model_id);
	static String add_model_profile(const String &p_display_name, const String &p_provider_id, const String &p_model, const String &p_api_key, const String &p_base_url, bool p_custom);
	static String add_model_profile_config(const AIModelProfile &p_profile);
	static bool update_model_profile(const String &p_profile_id, const String &p_display_name, const String &p_provider_id, const String &p_model, const String &p_api_key, const String &p_base_url, bool p_custom);
	static bool update_model_profile_config(const AIModelProfile &p_profile);
	static bool remove_model_profile(const String &p_profile_id);
	static AIModelProfile get_model_profile(const String &p_profile_id);
	static Vector<AIModelProfile> get_model_profiles(bool p_enabled_only = true);
	static Array get_model_profile_storage_for_test();
	static void set_model_profile_storage_for_test(const Array &p_profiles);
	static void clear_model_profiles_for_test();
	static void set_provider_auth(const String &p_provider_id, const String &p_api_key, const String &p_base_url);
	static String get_provider_api_key(const String &p_provider_id);
	static String get_provider_base_url(const String &p_provider_id);
	static void set_model_enabled(const String &p_model_id, bool p_enabled);
	static void set_model_enabled(const String &p_provider_id, const String &p_model, bool p_enabled);
	static bool is_model_enabled(const String &p_model_id);
	static bool is_model_enabled(const String &p_provider_id, const String &p_model);
	static void set_custom_models(const String &p_provider_id, const String &p_models);
	static String get_custom_models(const String &p_provider_id);
};
