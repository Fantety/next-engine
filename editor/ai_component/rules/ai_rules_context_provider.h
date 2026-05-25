/**************************************************************************/
/*  ai_rules_context_provider.h                                            */
/**************************************************************************/

#pragma once

#include "editor/ai_component/context/ai_context_provider.h"

class AIRulesContextProvider : public AIContextProvider {
	GDCLASS(AIRulesContextProvider, AIContextProvider);

protected:
	static void _bind_methods();

public:
	Array collect_context() override;
};
