/**************************************************************************/
/*  ai_list_project_tool.h                                                */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

class AIV1ListProjectTool : public AIV1EditorTool {
	GDCLASS(AIV1ListProjectTool, AIV1EditorTool);

	int entry_count = 0;
	int max_depth = 4;
	int max_entries = 400;
	int max_chars = 16000;
	bool truncated = false;

	void _append_tree(String &r_output, const String &p_path, int p_depth);

public:
	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIV1EditorToolResult execute_tool(const Dictionary &p_arguments) override;
};
