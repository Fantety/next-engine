/**************************************************************************/
/*  ai_rules_context_provider.cpp                                          */
/**************************************************************************/

#include "ai_rules_context_provider.h"

#include "core/object/class_db.h"

#include "editor/ai_component/context/ai_context_document.h"
#include "editor/ai_component/rules/ai_rule_settings.h"

void AIRulesContextProvider::_bind_methods() {
}

Array AIRulesContextProvider::collect_context() {
	Array result;
	Vector<AIRuleConfig> rules = AIRuleSettings::get_rules(true);
	if (rules.is_empty()) {
		return result;
	}

	String content;
	content += "User Rules\n";
	content += "Follow these user-configured rules while answering and using tools:\n";
	for (int i = 0; i < rules.size(); i++) {
		content += "- " + rules[i].content + "\n";
	}

	AIContextDocument doc;
	doc.title = "User Rules";
	doc.source = "ai_agent/rules";
	doc.content = content;
	doc.truncated = false;
	result.push_back(doc.to_dict());
	return result;
}
