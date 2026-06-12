/**************************************************************************/
/*  ai_scene_delete_node_tool.h                                           */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"
#include "editor/agent_v1/tools/editor/ai_scene_editing_service.h"

class AIV1SceneDeleteNodeTool : public AIV1EditorTool {
	GDCLASS(AIV1SceneDeleteNodeTool, AIV1EditorTool);

	Ref<AIV1SceneEditingService> service;

public:
	AIV1SceneDeleteNodeTool();

	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIV1EditorToolResult execute_tool(const Dictionary &p_arguments) override;
};
