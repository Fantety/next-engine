/**************************************************************************/
/*  ai_next_manage_project_tool.h                                         */
/**************************************************************************/

#pragma once

#include "editor/ai_component/next/ai_next_project_state.h"
#include "editor/ai_component/tools/ai_tool.h"

class AINextManageProjectTool : public AITool {
	GDCLASS(AINextManageProjectTool, AITool);

	Ref<AINextProjectState> project_state;

	bool _validate_replace_plan(const Array &p_milestones, String &r_error) const;
	bool _validate_append_tasks(const String &p_milestone_id, const Array &p_tasks, String &r_error) const;

protected:
	static void _bind_methods();

public:
	void set_project_state(const Ref<AINextProjectState> &p_project_state);
	Ref<AINextProjectState> get_project_state() const;

	String get_name() const override;
	String get_description() const override;
	Dictionary get_parameters_schema() const override;
	AIToolResult execute(const Dictionary &p_arguments) override;
};
