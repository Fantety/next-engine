/**************************************************************************/
/*  ai_skill_settings.cpp                                                  */
/**************************************************************************/

#include "ai_skill_settings.h"

#include "core/io/file_access.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "core/variant/variant.h"
#include "editor/settings/editor_settings.h"

namespace {

String _extract_skill_frontmatter_value(const String &p_frontmatter, const String &p_key) {
	Vector<String> lines = p_frontmatter.split("\n");
	const String key_prefix = p_key + ":";
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (!line.begins_with(key_prefix)) {
			continue;
		}

		String value = line.substr(key_prefix.length()).strip_edges();
		if ((value.begins_with("\"") && value.ends_with("\"")) || (value.begins_with("'") && value.ends_with("'"))) {
			value = value.substr(1, value.length() - 2);
		}
		return value.strip_edges();
	}
	return String();
}

void _parse_skill_markdown(const String &p_markdown, String &r_name, String &r_description) {
	r_name.clear();
	r_description.clear();

	if (!p_markdown.begins_with("---\n") && !p_markdown.begins_with("---\r\n")) {
		return;
	}

	int first_line_end = p_markdown.find("\n");
	if (first_line_end < 0) {
		return;
	}

	const int frontmatter_end = p_markdown.find("\n---", first_line_end + 1);
	if (frontmatter_end < 0) {
		return;
	}

	const String frontmatter = p_markdown.substr(first_line_end + 1, frontmatter_end - first_line_end - 1);
	r_name = _extract_skill_frontmatter_value(frontmatter, "name");
	r_description = _extract_skill_frontmatter_value(frontmatter, "description");
}

bool _is_stale_sample_skill(const Dictionary &p_skill) {
	return String(p_skill.get("display_name", String())).strip_edges() == "TDD" &&
			String(p_skill.get("description", String())).strip_edges() == "Use when changing behavior." &&
			String(p_skill.get("content", String())).strip_edges() == "Write tests first.";
}

Array _remove_stale_sample_skills(const Array &p_skills, bool &r_changed) {
	Array filtered_skills;
	r_changed = false;
	for (int i = 0; i < p_skills.size(); i++) {
		const Variant skill_value = p_skills[i];
		if (skill_value.get_type() == Variant::DICTIONARY && _is_stale_sample_skill(skill_value)) {
			r_changed = true;
			continue;
		}
		filtered_skills.push_back(skill_value);
	}
	return filtered_skills;
}

} // namespace

String AISkillSettings::_get_skills_path() {
	return "ai_agent/skills";
}

Array AISkillSettings::_get_skill_storage() {
	EditorSettings *settings = EditorSettings::get_singleton();
	if (!settings) {
		return Array();
	}

	const String path = _get_skills_path();
	if (!settings->has_setting(path)) {
		return Array();
	}

	Variant value = settings->get(path);
	if (value.get_type() != Variant::ARRAY) {
		return Array();
	}

	bool changed = false;
	Array skills = _remove_stale_sample_skills(value, changed);
	if (changed) {
		_set_skill_storage(skills);
	}
	return skills;
}

void AISkillSettings::_set_skill_storage(const Array &p_skills) {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);
	settings->set(_get_skills_path(), p_skills);
}

AISkillConfig AISkillSettings::_skill_from_dictionary(const Dictionary &p_skill) {
	AISkillConfig skill;
	skill.id = String(p_skill.get("id", String()));
	skill.display_name = String(p_skill.get("display_name", String()));
	skill.description = String(p_skill.get("description", String()));
	skill.content = String(p_skill.get("content", String()));
	skill.kind = String(p_skill.get("kind", "prompt_context")).strip_edges();
	if (skill.kind.is_empty()) {
		skill.kind = "prompt_context";
	}
	skill.enabled = bool(p_skill.get("enabled", true));
	return skill;
}

Dictionary AISkillSettings::_skill_to_dictionary(const AISkillConfig &p_skill) {
	Dictionary skill;
	skill["id"] = p_skill.id;
	skill["display_name"] = p_skill.display_name;
	skill["description"] = p_skill.description;
	skill["content"] = p_skill.content;
	skill["kind"] = p_skill.kind.is_empty() ? String("prompt_context") : p_skill.kind;
	skill["enabled"] = p_skill.enabled;
	return skill;
}

String AISkillSettings::_make_skill_id(const String &p_display_name) {
	String name = p_display_name.strip_edges().validate_node_name().replace(" ", "_").to_lower();
	if (name.is_empty()) {
		name = "skill";
	}
	return "skill:" + name + ":" + String::num_uint64(OS::get_singleton()->get_ticks_usec()) + ":" + itos(Math::rand());
}

String AISkillSettings::normalize_kind(const String &p_kind) {
	const String kind = p_kind.strip_edges().to_lower();
	if (kind == "prompt_context") {
		return "prompt_context";
	}
	return "prompt_context";
}

bool AISkillSettings::is_supported_kind(const String &p_kind) {
	return p_kind.strip_edges().to_lower() == "prompt_context";
}

String AISkillSettings::add_skill(const String &p_display_name, const String &p_description, const String &p_content, bool p_enabled) {
	AISkillConfig skill;
	skill.display_name = p_display_name.strip_edges();
	skill.description = p_description.strip_edges();
	skill.content = p_content.strip_edges();
	skill.kind = "prompt_context";
	skill.enabled = p_enabled;
	return add_skill_config(skill);
}

String AISkillSettings::add_skill_config(const AISkillConfig &p_skill) {
	AISkillConfig skill = p_skill;
	skill.display_name = skill.display_name.strip_edges();
	skill.description = skill.description.strip_edges();
	skill.content = skill.content.strip_edges();
	skill.kind = normalize_kind(skill.kind);
	if (skill.display_name.is_empty() || skill.content.is_empty()) {
		return String();
	}
	if (skill.id.is_empty()) {
		skill.id = _make_skill_id(skill.display_name);
	}

	Array skills = _get_skill_storage();
	skills.push_back(_skill_to_dictionary(skill));
	_set_skill_storage(skills);
	return skill.id;
}

bool AISkillSettings::update_skill(const String &p_skill_id, const String &p_display_name, const String &p_description, const String &p_content, bool p_enabled) {
	AISkillConfig skill;
	skill.id = p_skill_id;
	skill.display_name = p_display_name;
	skill.description = p_description;
	skill.content = p_content;
	skill.kind = "prompt_context";
	skill.enabled = p_enabled;
	return update_skill_config(skill);
}

bool AISkillSettings::update_skill_config(const AISkillConfig &p_skill) {
	if (p_skill.id.is_empty()) {
		return false;
	}

	AISkillConfig skill = p_skill;
	skill.display_name = skill.display_name.strip_edges();
	skill.description = skill.description.strip_edges();
	skill.content = skill.content.strip_edges();
	skill.kind = normalize_kind(skill.kind);
	if (skill.display_name.is_empty() || skill.content.is_empty()) {
		return false;
	}

	Array skills = _get_skill_storage();
	for (int i = 0; i < skills.size(); i++) {
		if (Variant(skills[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary existing = skills[i];
		if (String(existing.get("id", String())) == skill.id) {
			skills[i] = _skill_to_dictionary(skill);
			_set_skill_storage(skills);
			return true;
		}
	}
	return false;
}

bool AISkillSettings::import_skill_folder(const String &p_folder_path, String &r_error, String *r_skill_id) {
	r_error.clear();
	if (r_skill_id) {
		*r_skill_id = String();
	}

	const String folder_path = p_folder_path.strip_edges();
	if (folder_path.is_empty()) {
		r_error = "Select a Skill folder.";
		return false;
	}

	const String skill_path = folder_path.path_join("SKILL.md");
	if (!FileAccess::exists(skill_path)) {
		r_error = "The selected folder does not contain SKILL.md.";
		return false;
	}

	Error err = OK;
	const String content = FileAccess::get_file_as_string(skill_path, &err).strip_edges();
	if (err != OK || content.is_empty()) {
		r_error = "Could not read SKILL.md.";
		return false;
	}

	String display_name;
	String description;
	_parse_skill_markdown(content, display_name, description);
	if (display_name.is_empty()) {
		String fallback_path = folder_path.trim_suffix("/").trim_suffix("\\");
		display_name = fallback_path.get_file().strip_edges();
	}
	if (display_name.is_empty()) {
		r_error = "Could not infer a skill name from SKILL.md.";
		return false;
	}

	AISkillConfig skill;
	skill.display_name = display_name;
	skill.description = description;
	skill.content = content;
	skill.kind = "prompt_context";
	skill.enabled = true;

	const String skill_id = add_skill_config(skill);
	if (skill_id.is_empty()) {
		r_error = "Could not import the selected Skill.";
		return false;
	}

	if (r_skill_id) {
		*r_skill_id = skill_id;
	}
	return true;
}

bool AISkillSettings::remove_skill(const String &p_skill_id) {
	Array skills = _get_skill_storage();
	for (int i = 0; i < skills.size(); i++) {
		if (Variant(skills[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary existing = skills[i];
		if (String(existing.get("id", String())) == p_skill_id) {
			skills.remove_at(i);
			_set_skill_storage(skills);
			return true;
		}
	}
	return false;
}

bool AISkillSettings::set_skill_enabled(const String &p_skill_id, bool p_enabled) {
	Array skills = _get_skill_storage();
	for (int i = 0; i < skills.size(); i++) {
		if (Variant(skills[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary existing = skills[i];
		if (String(existing.get("id", String())) == p_skill_id) {
			existing["enabled"] = p_enabled;
			skills[i] = existing;
			_set_skill_storage(skills);
			return true;
		}
	}
	return false;
}

AISkillConfig AISkillSettings::get_skill(const String &p_skill_id) {
	Array skills = _get_skill_storage();
	for (int i = 0; i < skills.size(); i++) {
		if (Variant(skills[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		AISkillConfig skill = _skill_from_dictionary(skills[i]);
		if (skill.id == p_skill_id) {
			return skill;
		}
	}
	return AISkillConfig();
}

Vector<AISkillConfig> AISkillSettings::get_skills(bool p_enabled_only) {
	Vector<AISkillConfig> result;
	Array skills = _get_skill_storage();
	for (int i = 0; i < skills.size(); i++) {
		if (Variant(skills[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		AISkillConfig skill = _skill_from_dictionary(skills[i]);
		if (skill.id.is_empty()) {
			continue;
		}
		if (p_enabled_only && !skill.enabled) {
			continue;
		}
		result.push_back(skill);
	}
	return result;
}

Array AISkillSettings::get_skill_storage_for_test() {
	return _get_skill_storage();
}

void AISkillSettings::set_skill_storage_for_test(const Array &p_skills) {
	_set_skill_storage(p_skills);
}

void AISkillSettings::clear_skills_for_test() {
	_set_skill_storage(Array());
}
