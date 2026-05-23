/**************************************************************************/
/*  ai_script_create_tool.h                                               */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/editor/ai_script_editing_service.h"

class AIScriptCreateTool : public AITool {
	GDCLASS(AIScriptCreateTool, AITool);

	Ref<AIScriptEditingService> service;

public:
	AIScriptCreateTool();

	String get_name() const override;
	String get_description() const override;
	Dictionary get_parameters_schema() const override;
	AIToolResult execute(const Dictionary &p_arguments) override;
};
