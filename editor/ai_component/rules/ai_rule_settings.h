/**************************************************************************/
/*  ai_rule_settings.h                                                     */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

struct AIRuleConfig {
	String id;
	String content;
	bool enabled = true;
};

class AIRuleSettings {
	static String _get_rules_path();
	static Array _get_rule_storage();
	static void _set_rule_storage(const Array &p_rules);
	static AIRuleConfig _rule_from_dictionary(const Dictionary &p_rule);
	static Dictionary _rule_to_dictionary(const AIRuleConfig &p_rule);
	static String _make_rule_id();

public:
	static constexpr int MAX_RULE_CHARS = 100;

	static String normalize_rule_content(const String &p_content);
	static String add_rule(const String &p_content, bool p_enabled = true);
	static String add_rule_config(const AIRuleConfig &p_rule);
	static bool update_rule(const String &p_rule_id, const String &p_content, bool p_enabled);
	static bool update_rule_config(const AIRuleConfig &p_rule);
	static bool remove_rule(const String &p_rule_id);
	static bool set_rule_enabled(const String &p_rule_id, bool p_enabled);
	static AIRuleConfig get_rule(const String &p_rule_id);
	static Vector<AIRuleConfig> get_rules(bool p_enabled_only = false);
	static Array get_rule_storage_for_test();
	static void set_rule_storage_for_test(const Array &p_rules);
	static void clear_rules_for_test();
};
