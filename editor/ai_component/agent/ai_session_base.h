/**************************************************************************/
/*  ai_session_base.h                                                     */
/**************************************************************************/

#pragma once

#include "scene/main/node.h"

#include "editor/ai_component/agent/ai_agent_runtime_runner.h"

class AISessionBase : public Node {
	GDCLASS(AISessionBase, Node);

protected:
	static void _bind_methods();

	String _get_project_scope_key() const;
	String _make_unique_id(const String &p_prefix = String()) const;
	void _connect_runtime_signals(const Ref<AIAgentRuntimeRunner> &p_runner, const Callable &p_finished_callable, const Callable &p_message_added_callable, const Callable &p_message_updated_callable) const;
};
