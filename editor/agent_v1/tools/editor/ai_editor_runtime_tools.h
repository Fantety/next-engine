/**************************************************************************/
/*  ai_editor_runtime_tools.h                                             */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

class AIV1EditorRunSceneTool : public AIV1EditorTool {
	GDCLASS(AIV1EditorRunSceneTool, AIV1EditorTool);

public:
	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIV1EditorToolResult execute_tool(const Dictionary &p_arguments) override;
};

class AIV1EditorStopRunningSceneTool : public AIV1EditorTool {
	GDCLASS(AIV1EditorStopRunningSceneTool, AIV1EditorTool);

public:
	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIV1EditorToolResult execute_tool(const Dictionary &p_arguments) override;
};

class AIV1EditorGetTerminalErrorsTool : public AIV1EditorTool {
	GDCLASS(AIV1EditorGetTerminalErrorsTool, AIV1EditorTool);

public:
	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIV1EditorToolResult execute_tool(const Dictionary &p_arguments) override;
};
