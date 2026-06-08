/**************************************************************************/
/*  ai_docs_search_tool.h                                                 */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/editor/ai_documentation_service.h"

class AIDocsSearchTool : public AITool {
	GDCLASS(AIDocsSearchTool, AITool);

	Ref<AIDocumentationService> service;

public:
	AIDocsSearchTool();

	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};
