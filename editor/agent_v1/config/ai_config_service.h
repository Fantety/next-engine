/**************************************************************************/
/*  ai_config_service.h                                                   */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_error.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

struct AIConfigEntry {
	String source;
	String path;
	int priority = 0;
	Dictionary data;

	Dictionary to_dictionary() const;
	static AIConfigEntry from_dictionary(const Dictionary &p_dict);
};

class AIConfigService : public RefCounted {
	GDCLASS(AIConfigService, RefCounted);

	String global_config_path;
	String project_config_path;
	String opencode_config_path;
	String account_config_path;
	String managed_config_path;
	String remote_config_path;
	Dictionary runtime_override;
	Vector<AIConfigEntry> cached_entries;
	Dictionary cached_config;
	bool cache_valid = false;
	mutable Mutex mutex;

	static Dictionary _default_config();
	static Dictionary _dictionary_from_variant(const Variant &p_value);
	static Array _array_from_variant(const Variant &p_value);
	static Dictionary _merge_dicts(const Dictionary &p_base, const Dictionary &p_patch);
	static Dictionary _merge_patch_dicts(const Dictionary &p_base, const Dictionary &p_patch);
	static String _strip_json_comments(const String &p_text);
	static bool _parse_json_text(const String &p_text, const String &p_label, Dictionary &r_data, AIError &r_error);
	static bool _read_json_file(const String &p_path, Dictionary &r_data, AIError &r_error);
	static bool _write_json_file(const String &p_path, const Dictionary &p_data, AIError &r_error);
	static Dictionary _migrate_config(const Dictionary &p_config);
	static bool _environment_config(Dictionary &r_data, AIError &r_error);
	static Variant _resolve_variables(const Variant &p_value);
	static String _resolve_variables_in_string(const String &p_value);
	static void _sort_entries_by_priority(Vector<AIConfigEntry> &r_entries);
	static void _push_entry(Vector<AIConfigEntry> &r_entries, const String &p_source, const String &p_path, int p_priority, const Dictionary &p_data);

	bool _ensure_loaded_locked(AIError &r_error);
	Vector<AIConfigEntry> _discover_entries_locked(AIError &r_error) const;

protected:
	static void _bind_methods();

public:
	AIConfigService();

	void set_global_config_path(const String &p_path);
	String get_global_config_path() const;
	void set_project_config_path(const String &p_path);
	String get_project_config_path() const;
	void set_opencode_config_path(const String &p_path);
	String get_opencode_config_path() const;
	void set_account_config_path(const String &p_path);
	String get_account_config_path() const;
	void set_managed_config_path(const String &p_path);
	String get_managed_config_path() const;
	void set_remote_config_path(const String &p_path);
	String get_remote_config_path() const;

	Dictionary get_config();
	Array entries();
	Dictionary patch_config(const Dictionary &p_patch, const String &p_scope = "project");
	Dictionary update(const Dictionary &p_patch);
	Dictionary update_global(const Dictionary &p_patch);
	void set_runtime_override(const Dictionary &p_override);
	Dictionary get_runtime_override() const;
	void invalidate(const String &p_reason = String());

	String get_default_provider();
	String get_default_model();
	Dictionary get_provider_config(const String &p_provider);
	Array get_system_prompt(const String &p_agent_id = String());
};
