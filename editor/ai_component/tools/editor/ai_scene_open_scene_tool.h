/**************************************************************************/
/*  ai_scene_open_scene_tool.h                                            */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_editing_service.h"

class AISceneOpenSceneTool : public AITool {
	GDCLASS(AISceneOpenSceneTool, AITool);

	Ref<AISceneEditingService> service;

public:
	AISceneOpenSceneTool();

	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};
