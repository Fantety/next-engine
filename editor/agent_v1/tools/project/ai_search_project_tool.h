/**************************************************************************/
/*  ai_search_project_tool.h                                              */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

class AIV1SearchProjectTool : public AIV1EditorTool {
	GDCLASS(AIV1SearchProjectTool, AIV1EditorTool);

	int result_count = 0;
	int max_results = 50;
	int max_chars = 4096;
	bool truncated = false;
	String query;

	void _search_dir(String &r_output, const String &p_path);
	void _search_file(String &r_output, const String &p_path);

public:
	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIV1EditorToolResult execute_tool(const Dictionary &p_arguments) override;
};
