/**************************************************************************/
/*  ai_skill_service_v1.h                                                  */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_error.h"
#include "editor/agent_v1/core/registry/ai_scoped_registration.h"
#include "editor/agent_v1/tools/ai_tool_registry_v1.h"

#include "core/object/ref_counted.h"
#include "core/object/object_id.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class AIV1SkillService;

class AIV1SkillScriptToolAdapter : public AIV1Tool {
	GDCLASS(AIV1SkillScriptToolAdapter, AIV1Tool);

	ObjectID service_id;
	Dictionary manifest;
	Dictionary descriptor;
	String skill_id;
	String skill_name;
	String tool_name;
	String agent_tool_name;
	String permission_default = "ask";

protected:
	static void _bind_methods();

public:
	void setup(AIV1SkillService *p_service, const Dictionary &p_manifest, const Dictionary &p_descriptor, const String &p_permission_default);

	String get_skill_id() const;
	String get_tool_name() const;
	String get_agent_tool_name() const;

	virtual bool execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) override;
};

class AIV1SkillService : public RefCounted {
	GDCLASS(AIV1SkillService, RefCounted);
	friend class AIV1SkillScriptToolAdapter;

	struct SkillRecord {
		Dictionary manifest;
		String root_dir;
		String entry_path;
	};

	Ref<AIV1ToolRegistry> tool_registry;
	Array source_roots;
	Array enabled_skill_ids;
	Array disabled_skill_ids;
	bool auto_select = true;
	int max_skills_per_turn = 4;
	bool script_tools_enabled = false;
	String script_permission_default = "ask";
	HashMap<String, SkillRecord> skills;
	Vector<String> skill_order;
	Vector<Ref<AIScopedRegistration>> registrations;
	mutable Mutex mutex;

	static Dictionary _dictionary_from_variant(const Variant &p_value);
	static Array _array_from_variant(const Variant &p_value);
	static String _manifest_id_from_directory(const String &p_path);
	static String _normalize_skill_id(const String &p_text);
	static String _frontmatter_value(const String &p_frontmatter, const String &p_key);
	static String _strip_markdown_frontmatter(const String &p_markdown);
	static bool _is_valid_effect(const String &p_effect);
	static bool _is_path_safe_relative(const String &p_path);
	static String _content_hash(const String &p_text);
	static String _command_preview(const Array &p_command);
	static String _permission_resource_for_script_tool(const String &p_skill_id, const String &p_tool_name, const Array &p_command);
	static Dictionary _metadata_for_skill_tool(const Dictionary &p_manifest, const Dictionary &p_descriptor, const String &p_agent_tool_name);
	static bool _read_text_file(const String &p_path, String &r_text, AIError &r_error);
	static bool _read_markdown_summary_file(const String &p_path, String &r_name, String &r_description, AIError &r_error);
	static bool _parse_json_file(const String &p_path, Dictionary &r_data, AIError &r_error);
	static bool _parse_skill_directory(const String &p_dir, SkillRecord &r_record, AIError &r_error);
	static bool _resource_declared(const SkillRecord &p_record, const String &p_path, const String &p_kind, Dictionary *r_resource = nullptr);
	static bool _is_disabled_id_or_name(const Dictionary &p_manifest, const Array &p_disabled);
	static bool _matches_id_or_name(const Dictionary &p_manifest, const String &p_text);
	static bool _trigger_matches_prompt(const Dictionary &p_trigger, const String &p_prompt_lower);
	static void _close_registration_list(Vector<Ref<AIScopedRegistration>> &r_registrations);

	bool _register_skill_tools_locked(Vector<Ref<AIScopedRegistration>> &r_new_registrations, AIError &r_error);
	bool _load_skill_document_locked(const String &p_skill_id, Dictionary &r_document, AIError &r_error) const;
	bool _execute_script_tool(const Dictionary &p_manifest, const Dictionary &p_descriptor, const String &p_permission_default, const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) const;

protected:
	static void _bind_methods();

public:
	~AIV1SkillService();

	static String sanitize_name_part(const String &p_text);
	static String make_tool_name(const String &p_skill_id, const String &p_tool_name);

	void set_tool_registry(const Ref<AIV1ToolRegistry> &p_tool_registry);
	Ref<AIV1ToolRegistry> get_tool_registry() const;

	bool import_config_struct(const Dictionary &p_config, AIError &r_error);
	Dictionary import_config(const Dictionary &p_config);
	void clear();

	bool refresh_struct(AIError &r_error);
	Dictionary refresh();

	Array list_manifests() const;
	Array select(const String &p_prompt, const Array &p_explicit_names) const;
	Dictionary make_context_source(const String &p_skill_id, bool p_required = true, int p_priority = 150);
	Dictionary read_resource(const String &p_skill_id, const String &p_path, const String &p_kind);
};
