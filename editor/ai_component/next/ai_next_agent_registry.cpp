/**************************************************************************/
/*  ai_next_agent_registry.cpp                                            */
/**************************************************************************/

#include "ai_next_agent_registry.h"

namespace {

AINextAgentDescriptor _make_agent_descriptor(const String &p_id, const String &p_display_name, const String &p_default_profile_id, bool p_assignable_to_tasks) {
	AINextAgentDescriptor descriptor;
	descriptor.id = p_id;
	descriptor.display_name = p_display_name;
	descriptor.default_profile_id = p_default_profile_id;
	descriptor.assignable_to_tasks = p_assignable_to_tasks;
	return descriptor;
}

} // namespace

String AINextAgentRegistry::get_planning_agent_id() {
	return "planning_agent";
}

String AINextAgentRegistry::get_script_agent_id() {
	return "script_agent";
}

String AINextAgentRegistry::get_scene_agent_id() {
	return "scene_agent";
}

String AINextAgentRegistry::get_shader_agent_id() {
	return "shader_agent";
}

String AINextAgentRegistry::get_review_agent_id() {
	return "review_agent";
}

Vector<AINextAgentDescriptor> AINextAgentRegistry::get_agent_descriptors() {
	Vector<AINextAgentDescriptor> descriptors;
	descriptors.push_back(_make_agent_descriptor(get_planning_agent_id(), "Planning Agent", "ask", false));
	descriptors.push_back(_make_agent_descriptor(get_script_agent_id(), "Script Agent", "auto", true));
	descriptors.push_back(_make_agent_descriptor(get_scene_agent_id(), "Scene Agent", "auto", true));
	descriptors.push_back(_make_agent_descriptor(get_shader_agent_id(), "Shader Agent", "auto", true));
	descriptors.push_back(_make_agent_descriptor(get_review_agent_id(), "Review Agent", "ask", false));
	return descriptors;
}

Vector<String> AINextAgentRegistry::get_agent_ids() {
	Vector<String> agent_ids;
	Vector<AINextAgentDescriptor> descriptors = get_agent_descriptors();
	for (int i = 0; i < descriptors.size(); i++) {
		agent_ids.push_back(descriptors[i].id);
	}
	return agent_ids;
}

Vector<String> AINextAgentRegistry::get_assignable_agent_ids() {
	Vector<String> agent_ids;
	Vector<AINextAgentDescriptor> descriptors = get_agent_descriptors();
	for (int i = 0; i < descriptors.size(); i++) {
		if (descriptors[i].assignable_to_tasks) {
			agent_ids.push_back(descriptors[i].id);
		}
	}
	return agent_ids;
}

String AINextAgentRegistry::get_default_task_agent_id() {
	Vector<String> assignable_agent_ids = get_assignable_agent_ids();
	return assignable_agent_ids.is_empty() ? String() : assignable_agent_ids[0];
}

bool AINextAgentRegistry::is_valid_agent_id(const String &p_agent_id) {
	return !get_agent_descriptor(p_agent_id).id.is_empty();
}

bool AINextAgentRegistry::is_assignable_agent_id(const String &p_agent_id) {
	return get_agent_descriptor(p_agent_id).assignable_to_tasks;
}

AINextAgentDescriptor AINextAgentRegistry::get_agent_descriptor(const String &p_agent_id) {
	Vector<AINextAgentDescriptor> descriptors = get_agent_descriptors();
	for (int i = 0; i < descriptors.size(); i++) {
		if (descriptors[i].id == p_agent_id) {
			return descriptors[i];
		}
	}
	return AINextAgentDescriptor();
}

String AINextAgentRegistry::get_display_name(const String &p_agent_id) {
	AINextAgentDescriptor descriptor = get_agent_descriptor(p_agent_id);
	return descriptor.display_name.is_empty() ? p_agent_id : descriptor.display_name;
}

String AINextAgentRegistry::get_default_profile_id(const String &p_agent_id) {
	return get_agent_descriptor(p_agent_id).default_profile_id;
}
