/**************************************************************************/
/*  ai_manage_plan_tool.h                                                  */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"

class AIManagePlanTool : public AITool {
	GDCLASS(AIManagePlanTool, AITool);

protected:
	static void _bind_methods();

public:
	String get_name() const override;
	String get_description() const override;
	Dictionary get_parameters_schema() const override;
	AIToolResult execute(const Dictionary &p_arguments) override;
};
