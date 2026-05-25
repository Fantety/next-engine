/**************************************************************************/
/*  ai_skill_settings.h                                                    */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

struct AISkillConfig {
	String id;
	String display_name;
	String description;
	String content;
	String kind = "prompt_context";
	bool enabled = true;
};

class AISkillSettings {
	static String _get_skills_path();
	static Array _get_skill_storage();
	static void _set_skill_storage(const Array &p_skills);
	static AISkillConfig _skill_from_dictionary(const Dictionary &p_skill);
	static Dictionary _skill_to_dictionary(const AISkillConfig &p_skill);
	static String _make_skill_id(const String &p_display_name);

public:
	static String normalize_kind(const String &p_kind);
	static bool is_supported_kind(const String &p_kind);
	static String add_skill(const String &p_display_name, const String &p_description, const String &p_content, bool p_enabled = true);
	static String add_skill_config(const AISkillConfig &p_skill);
	static bool update_skill(const String &p_skill_id, const String &p_display_name, const String &p_description, const String &p_content, bool p_enabled);
	static bool update_skill_config(const AISkillConfig &p_skill);
	static bool import_skill_folder(const String &p_folder_path, String &r_error, String *r_skill_id = nullptr);
	static bool remove_skill(const String &p_skill_id);
	static bool set_skill_enabled(const String &p_skill_id, bool p_enabled);
	static AISkillConfig get_skill(const String &p_skill_id);
	static Vector<AISkillConfig> get_skills(bool p_enabled_only = false);
	static Array get_skill_storage_for_test();
	static void set_skill_storage_for_test(const Array &p_skills);
	static void clear_skills_for_test();
};
