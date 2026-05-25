/**************************************************************************/
/*  ai_activate_skill_tool.cpp                                             */
/**************************************************************************/

#include "ai_activate_skill_tool.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

#include "editor/ai_component/skills/ai_skill_settings.h"

void AIActivateSkillTool::_bind_methods() {
}

String AIActivateSkillTool::get_name() const {
	return "agent.activate_skill";
}

String AIActivateSkillTool::get_description() const {
	return "Loads the full content of an enabled prompt/context AgentSkill by skill_id. This is read-only and does not execute code or grant tool permissions.";
}

Dictionary AIActivateSkillTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary skill_id_property;
	skill_id_property["type"] = "string";
	skill_id_property["description"] = "The enabled AgentSkill id from the Available Agent Skills context.";
	properties["skill_id"] = skill_id_property;

	Array required;
	required.push_back("skill_id");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIActivateSkillTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String skill_id = String(p_arguments.get("skill_id", String())).strip_edges();
	if (skill_id.is_empty()) {
		result.error = "Missing required skill_id.";
		return result;
	}

	AISkillConfig skill = AISkillSettings::get_skill(skill_id);
	if (skill.id.is_empty()) {
		result.error = "AgentSkill does not exist.";
		return result;
	}
	if (!skill.enabled) {
		result.error = "AgentSkill is disabled.";
		return result;
	}
	if (!AISkillSettings::is_supported_kind(skill.kind)) {
		result.error = "AgentSkill kind is not supported by the current prompt/context-only runtime.";
		return result;
	}

	result.content = "Activated AgentSkill: " + skill.display_name + "\n";
	result.content += "Kind: " + skill.kind + "\n";
	result.content += "Safety: This skill provides prompt/context instructions only. It does not execute code, launch processes, read bundled resources, or grant tool permissions.\n\n";
	result.content += skill.content;
	result.metadata["skill_id"] = skill.id;
	result.metadata["skill_name"] = skill.display_name;
	result.metadata["skill_kind"] = skill.kind;
	result.metadata["tool_origin"] = "agent_skill";
	return result;
}
