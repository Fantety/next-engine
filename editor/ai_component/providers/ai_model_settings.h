/**************************************************************************/
/*  ai_model_settings.h                                                   */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"
#include "core/variant/array.h"

#include "editor/ai_component/providers/ai_provider_config.h"

struct AIModelProviderPreset {
	String id;
	String display_name;
	String default_base_url;
	Vector<String> preset_models;
};

struct AIModelDescriptor {
	String id;
	String provider_id;
	String provider_name;
	String model;
	String base_url;
	String api_key;
	bool enabled = false;
	bool custom = false;
};

class AIModelSettings {
	static String _get_provider_path(const String &p_provider_id, const String &p_property);
	static String _get_model_path(const String &p_provider_id, const String &p_model);
	static String _get_legacy_deepseek_value(const String &p_property, const String &p_default_value);
	static bool _is_default_model_enabled(const String &p_provider_id, const String &p_model);
	static PackedStringArray _split_custom_models(const String &p_models);

public:
	static Vector<AIModelProviderPreset> get_provider_presets();
	static String get_model_id(const String &p_provider_id, const String &p_model);
	static AIModelDescriptor get_model(const String &p_model_id);
	static Vector<AIModelDescriptor> get_enabled_models();
	static AIProviderConfig get_provider_config(const String &p_model_id);
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
