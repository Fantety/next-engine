/**************************************************************************/
/*  ai_rule_settings.cpp                                                   */
/**************************************************************************/

#include "ai_rule_settings.h"

#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "core/variant/variant.h"
#include "editor/settings/editor_settings.h"

namespace {

bool _is_stale_sample_rule(const Dictionary &p_rule) {
	const String content = String(p_rule.get("content", String())).strip_edges();
	return content == "Prefer concise answers." || content == "Disabled rule should not appear.";
}

Array _remove_stale_sample_rules(const Array &p_rules, bool &r_changed) {
	Array filtered_rules;
	r_changed = false;
	for (int i = 0; i < p_rules.size(); i++) {
		const Variant rule_value = p_rules[i];
		if (rule_value.get_type() == Variant::DICTIONARY && _is_stale_sample_rule(rule_value)) {
			r_changed = true;
			continue;
		}
		filtered_rules.push_back(rule_value);
	}
	return filtered_rules;
}

} // namespace

String AIRuleSettings::_get_rules_path() {
	return "ai_agent/rules";
}

Array AIRuleSettings::_get_rule_storage() {
	EditorSettings *settings = EditorSettings::get_singleton();
	if (!settings) {
		return Array();
	}

	const String path = _get_rules_path();
	if (!settings->has_setting(path)) {
		return Array();
	}

	Variant value = settings->get(path);
	if (value.get_type() != Variant::ARRAY) {
		return Array();
	}

	bool changed = false;
	Array rules = _remove_stale_sample_rules(value, changed);
	if (changed) {
		_set_rule_storage(rules);
	}
	return rules;
}

void AIRuleSettings::_set_rule_storage(const Array &p_rules) {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);
	settings->set(_get_rules_path(), p_rules);
}

AIRuleConfig AIRuleSettings::_rule_from_dictionary(const Dictionary &p_rule) {
	AIRuleConfig rule;
	rule.id = String(p_rule.get("id", String()));
	rule.content = normalize_rule_content(String(p_rule.get("content", String())));
	rule.enabled = bool(p_rule.get("enabled", true));
	return rule;
}

Dictionary AIRuleSettings::_rule_to_dictionary(const AIRuleConfig &p_rule) {
	Dictionary rule;
	rule["id"] = p_rule.id;
	rule["content"] = normalize_rule_content(p_rule.content);
	rule["enabled"] = p_rule.enabled;
	return rule;
}

String AIRuleSettings::_make_rule_id() {
	return "rule:" + String::num_uint64(OS::get_singleton()->get_ticks_usec()) + ":" + itos(Math::rand());
}

String AIRuleSettings::normalize_rule_content(const String &p_content) {
	String content = p_content.strip_edges();
	content = content.replace("\r\n", " ").replace("\n", " ").replace("\t", " ");
	while (content.contains("  ")) {
		content = content.replace("  ", " ");
	}
	if (content.length() > MAX_RULE_CHARS) {
		content = content.substr(0, MAX_RULE_CHARS);
	}
	return content.strip_edges();
}

String AIRuleSettings::add_rule(const String &p_content, bool p_enabled) {
	AIRuleConfig rule;
	rule.content = p_content;
	rule.enabled = p_enabled;
	return add_rule_config(rule);
}

String AIRuleSettings::add_rule_config(const AIRuleConfig &p_rule) {
	AIRuleConfig rule = p_rule;
	rule.content = normalize_rule_content(rule.content);
	if (rule.content.is_empty()) {
		return String();
	}
	if (rule.id.is_empty()) {
		rule.id = _make_rule_id();
	}

	Array rules = _get_rule_storage();
	rules.push_back(_rule_to_dictionary(rule));
	_set_rule_storage(rules);
	return rule.id;
}

bool AIRuleSettings::update_rule(const String &p_rule_id, const String &p_content, bool p_enabled) {
	AIRuleConfig rule;
	rule.id = p_rule_id;
	rule.content = p_content;
	rule.enabled = p_enabled;
	return update_rule_config(rule);
}

bool AIRuleSettings::update_rule_config(const AIRuleConfig &p_rule) {
	if (p_rule.id.is_empty()) {
		return false;
	}

	AIRuleConfig rule = p_rule;
	rule.content = normalize_rule_content(rule.content);
	if (rule.content.is_empty()) {
		return false;
	}

	Array rules = _get_rule_storage();
	for (int i = 0; i < rules.size(); i++) {
		if (Variant(rules[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary existing = rules[i];
		if (String(existing.get("id", String())) == rule.id) {
			rules[i] = _rule_to_dictionary(rule);
			_set_rule_storage(rules);
			return true;
		}
	}
	return false;
}

bool AIRuleSettings::remove_rule(const String &p_rule_id) {
	Array rules = _get_rule_storage();
	for (int i = 0; i < rules.size(); i++) {
		if (Variant(rules[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary existing = rules[i];
		if (String(existing.get("id", String())) == p_rule_id) {
			rules.remove_at(i);
			_set_rule_storage(rules);
			return true;
		}
	}
	return false;
}

bool AIRuleSettings::set_rule_enabled(const String &p_rule_id, bool p_enabled) {
	Array rules = _get_rule_storage();
	for (int i = 0; i < rules.size(); i++) {
		if (Variant(rules[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary existing = rules[i];
		if (String(existing.get("id", String())) == p_rule_id) {
			existing["enabled"] = p_enabled;
			rules[i] = existing;
			_set_rule_storage(rules);
			return true;
		}
	}
	return false;
}

AIRuleConfig AIRuleSettings::get_rule(const String &p_rule_id) {
	Array rules = _get_rule_storage();
	for (int i = 0; i < rules.size(); i++) {
		if (Variant(rules[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		AIRuleConfig rule = _rule_from_dictionary(rules[i]);
		if (rule.id == p_rule_id) {
			return rule;
		}
	}
	return AIRuleConfig();
}

Vector<AIRuleConfig> AIRuleSettings::get_rules(bool p_enabled_only) {
	Vector<AIRuleConfig> result;
	Array rules = _get_rule_storage();
	for (int i = 0; i < rules.size(); i++) {
		if (Variant(rules[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		AIRuleConfig rule = _rule_from_dictionary(rules[i]);
		if (rule.id.is_empty() || rule.content.is_empty()) {
			continue;
		}
		if (p_enabled_only && !rule.enabled) {
			continue;
		}
		result.push_back(rule);
	}
	return result;
}

Array AIRuleSettings::get_rule_storage_for_test() {
	return _get_rule_storage();
}

void AIRuleSettings::set_rule_storage_for_test(const Array &p_rules) {
	_set_rule_storage(p_rules);
}

void AIRuleSettings::clear_rules_for_test() {
	_set_rule_storage(Array());
}
