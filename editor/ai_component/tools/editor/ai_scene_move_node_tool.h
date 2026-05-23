/**************************************************************************/
/*  ai_scene_move_node_tool.h                                             */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_editing_service.h"

class AISceneMoveNodeTool : public AITool {
	GDCLASS(AISceneMoveNodeTool, AITool);

	Ref<AISceneEditingService> service;

public:
	AISceneMoveNodeTool();

	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};
