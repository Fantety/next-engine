/**************************************************************************/
/*  ai_skill_context_provider.cpp                                          */
/**************************************************************************/

#include "ai_skill_context_provider.h"

#include "core/object/class_db.h"

#include "editor/ai_component/context/ai_context_document.h"
#include "editor/ai_component/skills/ai_skill_settings.h"

void AISkillIndexContextProvider::_bind_methods() {
}

Array AISkillIndexContextProvider::collect_context() {
	Array result;
	Vector<AISkillConfig> skills = AISkillSettings::get_skills(true);
	if (skills.is_empty()) {
		return result;
	}

	String content;
	content += "Available Agent Skills\n";
	content += "These skills are prompt/context instructions only. They do not execute code, launch processes, read bundled resources, or grant tool permissions.\n";
	content += "Call the read-only `agent.activate_skill` tool with a `skill_id` to load the full content of a relevant enabled skill.\n\n";
	for (int i = 0; i < skills.size(); i++) {
		const AISkillConfig &skill = skills[i];
		content += "- skill_id: " + skill.id + "\n";
		content += "  name: " + skill.display_name + "\n";
		content += "  kind: " + skill.kind + "\n";
		content += "  description: " + skill.description + "\n";
	}

	AIContextDocument doc;
	doc.title = "Available Agent Skills";
	doc.source = "ai_agent/skills";
	doc.content = content;
	doc.truncated = false;
	result.push_back(doc.to_dict());
	return result;
}
