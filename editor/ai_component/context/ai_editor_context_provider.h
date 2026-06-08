/**************************************************************************/
/*  ai_editor_context_provider.h                                           */
/**************************************************************************/

#pragma once

#include "editor/ai_component/agent/ai_agent_profile.h"
#include "editor/ai_component/context/ai_context_provider.h"

class AIEditorContextProvider : public AIContextProvider {
	GDCLASS(AIEditorContextProvider, AIContextProvider);

	AIAgentProfile agent_profile;

protected:
	static void _bind_methods();

public:
	AIEditorContextProvider();

	void set_agent_profile(const AIAgentProfile &p_profile);
	AIAgentProfile get_agent_profile() const;
	Array collect_context() override;
};
