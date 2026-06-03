/**************************************************************************/
/*  ai_next_agents.cpp                                                    */
/**************************************************************************/

#include "ai_next_agents.h"

#include "editor/ai_component/next/ai_next_agent_registry.h"
#include "editor/ai_component/next/ai_next_manage_project_tool.h"
#include "editor/ai_component/next/ai_next_prompts.h"
#include "editor/ai_component/tools/ai_tool_factory.h"

namespace {

void _configure_next_agent(AIAgentBase *p_agent, const String &p_agent_id, const String &p_prompt, const String &p_profile_id) {
	if (!p_agent) {
		return;
	}
	p_agent->set_session_id("next:" + p_agent_id);
	p_agent->set_system_prompt(p_prompt);
	p_agent->set_agent_profile_id(p_profile_id);
}

void _register_next_tools(AIAgentBase *p_agent, const Ref<AINextProjectState> &p_project_state) {
	if (!p_agent || p_project_state.is_null()) {
		return;
	}

	Ref<AINextManageProjectTool> manage_project_tool;
	manage_project_tool.instantiate();
	manage_project_tool->set_project_state(p_project_state);
	p_agent->add_tool(manage_project_tool, AI_TOOL_PERMISSION_ALLOW);
}

} // namespace

void AINextPlanningAgent::_bind_methods() {
}

AINextPlanningAgent::AINextPlanningAgent() {
	const String agent_id = AINextAgentRegistry::get_planning_agent_id();
	_configure_next_agent(this, agent_id, AINextPrompts::get_planning_prompt(), AINextAgentRegistry::get_default_profile_id(agent_id));
	_rebuild_tools();
}

void AINextPlanningAgent::_rebuild_tools() {
	clear_tools();
	_register_next_tools(this, project_state);
	AIToolFactory::register_shared_project_tools(this);
}

void AINextPlanningAgent::set_project_state(const Ref<AINextProjectState> &p_project_state) {
	project_state = p_project_state;
	_rebuild_tools();
}

Ref<AINextProjectState> AINextPlanningAgent::get_project_state() const {
	return project_state;
}

void AINextScriptAgent::_bind_methods() {
}

AINextScriptAgent::AINextScriptAgent() {
	const String agent_id = AINextAgentRegistry::get_script_agent_id();
	_configure_next_agent(this, agent_id, AINextPrompts::get_script_prompt(), AINextAgentRegistry::get_default_profile_id(agent_id));
	AIToolFactory::register_shared_project_tools(this);
	AIToolFactory::register_project_write_tools(this, AI_TOOL_PERMISSION_ALLOW);
	AIToolFactory::register_script_inspection_tools(this, AI_TOOL_PERMISSION_ALLOW);
	AIToolFactory::register_script_write_tools(this, AI_TOOL_PERMISSION_ALLOW, AI_TOOL_PERMISSION_ASK);
}

void AINextSceneAgent::_bind_methods() {
}

AINextSceneAgent::AINextSceneAgent() {
	const String agent_id = AINextAgentRegistry::get_scene_agent_id();
	_configure_next_agent(this, agent_id, AINextPrompts::get_scene_prompt(), AINextAgentRegistry::get_default_profile_id(agent_id));
	AIToolFactory::register_shared_project_tools(this);
	AIToolFactory::register_scene_inspection_tools(this, AI_TOOL_PERMISSION_ALLOW);
	AIToolFactory::register_scene_write_tools(this, AI_TOOL_PERMISSION_ALLOW, AI_TOOL_PERMISSION_ASK);
}

void AINextShaderAgent::_bind_methods() {
}

AINextShaderAgent::AINextShaderAgent() {
	const String agent_id = AINextAgentRegistry::get_shader_agent_id();
	_configure_next_agent(this, agent_id, AINextPrompts::get_shader_prompt(), AINextAgentRegistry::get_default_profile_id(agent_id));
	AIToolFactory::register_shared_project_tools(this);
	AIToolFactory::register_project_write_tools(this, AI_TOOL_PERMISSION_ALLOW);
	AIToolFactory::register_shader_tools(this, AI_TOOL_PERMISSION_ALLOW, AI_TOOL_PERMISSION_ASK);
}

void AINextReviewAgent::_bind_methods() {
}

AINextReviewAgent::AINextReviewAgent() {
	const String agent_id = AINextAgentRegistry::get_review_agent_id();
	_configure_next_agent(this, agent_id, AINextPrompts::get_review_prompt(), AINextAgentRegistry::get_default_profile_id(agent_id));
	AIToolFactory::register_shared_project_tools(this);
	AIToolFactory::register_editor_runtime_tools(this, AI_TOOL_PERMISSION_ALLOW);
}

Ref<AIAgentBase> AINextAgents::create_agent(const String &p_agent_id, const Ref<AINextProjectState> &p_project_state) {
	if (p_agent_id == AINextAgentRegistry::get_planning_agent_id()) {
		Ref<AINextPlanningAgent> planning_agent;
		planning_agent.instantiate();
		planning_agent->set_project_state(p_project_state);
		return planning_agent;
	}
	if (p_agent_id == AINextAgentRegistry::get_script_agent_id()) {
		Ref<AINextScriptAgent> script_agent;
		script_agent.instantiate();
		return script_agent;
	}
	if (p_agent_id == AINextAgentRegistry::get_scene_agent_id()) {
		Ref<AINextSceneAgent> scene_agent;
		scene_agent.instantiate();
		return scene_agent;
	}
	if (p_agent_id == AINextAgentRegistry::get_shader_agent_id()) {
		Ref<AINextShaderAgent> shader_agent;
		shader_agent.instantiate();
		return shader_agent;
	}
	if (p_agent_id == AINextAgentRegistry::get_review_agent_id()) {
		Ref<AINextReviewAgent> review_agent;
		review_agent.instantiate();
		return review_agent;
	}
	return Ref<AIAgentBase>();
}
