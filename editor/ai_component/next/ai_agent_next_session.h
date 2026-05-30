/**************************************************************************/
/*  ai_agent_next_session.h                                               */
/**************************************************************************/

#pragma once

#include "editor/ai_component/agent/ai_agent_base.h"
#include "editor/ai_component/next/ai_next_event_log.h"
#include "editor/ai_component/next/ai_next_project_state.h"
#include "editor/ai_component/next/ai_next_project_store.h"
#include "editor/ai_component/tools/ai_tool.h"
#include "scene/main/node.h"

class AIAgentNextSession : public Node {
	GDCLASS(AIAgentNextSession, Node);

	Ref<AINextProjectState> project_state;
	Ref<AINextProjectStore> project_store;
	Ref<AINextEventLog> event_log;

	Ref<AIAgentBase> planning_agent;
	Ref<AIAgentBase> script_agent;
	Ref<AIAgentBase> scene_agent;
	Ref<AIAgentBase> shader_agent;
	Ref<AIAgentBase> review_agent;

	void _configure_agent(const Ref<AIAgentBase> &p_agent, const String &p_agent_id, const String &p_prompt, const Vector<Ref<AITool>> &p_tools);
	void _register_next_tools(const Ref<AIAgentBase> &p_agent);
	void _register_shared_read_tools(const Ref<AIAgentBase> &p_agent);
	void _register_specialist_write_tools(const Ref<AIAgentBase> &p_agent, const String &p_agent_id);
	Ref<AIAgentBase> _get_agent(const String &p_agent_id) const;

protected:
	static void _bind_methods();

public:
	AIAgentNextSession();

	Ref<AINextProjectState> get_project_state() const;
	Ref<AINextProjectStore> get_project_store() const;
	Ref<AINextEventLog> get_event_log() const;
	bool has_agent(const String &p_agent_id) const;
	Ref<AIAgentBase> get_agent_for_test(const String &p_agent_id) const;

	void set_model_profile_id(const String &p_model_profile_id);
	void submit_brief(const String &p_brief);
	void generate_plan();
	void approve_plan();
	void run_active_milestone();
	void review_active_milestone();
	void generate_feedback_tasks(const String &p_feedback);
	void accept_and_lock_active_milestone();
	void cancel_current_operation();
};
