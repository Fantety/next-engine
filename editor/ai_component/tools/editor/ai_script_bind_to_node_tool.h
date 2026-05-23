/**************************************************************************/
/*  ai_script_bind_to_node_tool.h                                         */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/editor/ai_script_editing_service.h"

class AIScriptBindToNodeTool : public AITool {
	GDCLASS(AIScriptBindToNodeTool, AITool);

	Ref<AIScriptEditingService> service;

public:
	AIScriptBindToNodeTool();

	String get_name() const override;
	String get_description() const override;
	Dictionary get_parameters_schema() const override;
	AIToolResult execute(const Dictionary &p_arguments) override;
};
