/**************************************************************************/
/*  ai_search_project_tool.h                                              */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"

class AISearchProjectTool : public AITool {
	GDCLASS(AISearchProjectTool, AITool);

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
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};
