/**************************************************************************/
/*  ai_editor_runtime_tools.h                                             */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"

class AIEditorRunSceneTool : public AITool {
	GDCLASS(AIEditorRunSceneTool, AITool);

public:
	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};

class AIEditorStopRunningSceneTool : public AITool {
	GDCLASS(AIEditorStopRunningSceneTool, AITool);

public:
	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};

class AIEditorGetTerminalErrorsTool : public AITool {
	GDCLASS(AIEditorGetTerminalErrorsTool, AITool);

public:
	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};
