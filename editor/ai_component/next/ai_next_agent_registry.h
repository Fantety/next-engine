/**************************************************************************/
/*  ai_next_agent_registry.h                                              */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"

struct AINextAgentDescriptor {
	String id;
	String display_name;
	String default_profile_id;
	bool assignable_to_tasks = false;
};

class AINextAgentRegistry {
public:
	static String get_planning_agent_id();
	static String get_script_agent_id();
	static String get_scene_agent_id();
	static String get_shader_agent_id();
	static String get_review_agent_id();

	static Vector<AINextAgentDescriptor> get_agent_descriptors();
	static Vector<String> get_agent_ids();
	static Vector<String> get_assignable_agent_ids();
	static String get_default_task_agent_id();

	static bool is_valid_agent_id(const String &p_agent_id);
	static bool is_assignable_agent_id(const String &p_agent_id);
	static AINextAgentDescriptor get_agent_descriptor(const String &p_agent_id);
	static String get_display_name(const String &p_agent_id);
	static String get_default_profile_id(const String &p_agent_id);
};
