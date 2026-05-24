/**************************************************************************/
/*  ai_best_practices_context_provider.h                                  */
/**************************************************************************/

#pragma once

#include "editor/ai_component/context/ai_context_provider.h"

class AIBestPracticesContextProvider : public AIContextProvider {
	GDCLASS(AIBestPracticesContextProvider, AIContextProvider);

	int max_chars = 12000;

protected:
	static void _bind_methods();

public:
	void set_max_chars(int p_max_chars);
	int get_max_chars() const;
	Array collect_context() override;
};
