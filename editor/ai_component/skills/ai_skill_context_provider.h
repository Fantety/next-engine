/**************************************************************************/
/*  ai_skill_context_provider.h                                            */
/**************************************************************************/

#pragma once

#include "editor/ai_component/context/ai_context_provider.h"

class AISkillIndexContextProvider : public AIContextProvider {
	GDCLASS(AISkillIndexContextProvider, AIContextProvider);

protected:
	static void _bind_methods();

public:
	Array collect_context() override;
};
