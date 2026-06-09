/**************************************************************************/
/*  ai_activate_tool_category_tool.h                                      */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"

class AIToolRegistry;

class AIActivateToolCategoryTool : public AITool {
	GDCLASS(AIActivateToolCategoryTool, AITool);

	AIToolRegistry *registry = nullptr;

protected:
	static void _bind_methods();

public:
	void setup(AIToolRegistry *p_registry);

	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};
