/**************************************************************************/
/*  ai_next_workflow_context_builder.h                                    */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "editor/ai_component/next/ai_next_project_memory_store.h"
#include "editor/ai_component/next/ai_next_project_state.h"

struct AINextWorkflowContextOptions {
	String operation;
	String milestone_id;
	String task_id;
	bool has_project_memory = false;
	AINextProjectMemory project_memory;
	int max_completed_tasks = 8;
	int max_previous_milestones = 2;
	int max_recent_events = 8;
	bool include_dependency_details = true;
};

class AINextWorkflowContextBuilder {
	static Dictionary _make_document(const String &p_title, const String &p_source, const String &p_content);
	static bool _has_string(const Vector<String> &p_values, const String &p_value);
	static bool _is_result_status(const String &p_status);
	static String _format_string_array(const Array &p_values);
	static String _format_task_result(const Dictionary &p_task);
	static int _find_milestone_index(const Array &p_milestones, const String &p_milestone_id);
	static int _find_task_index(const Array &p_tasks, const String &p_task_id);
	static Dictionary _find_task(const Array &p_milestones, const String &p_task_id);
	static void _append_task_if_relevant(const Dictionary &p_task, Vector<Dictionary> &r_tasks, Vector<String> &r_task_ids);
	static String _build_workflow_state(const Ref<AINextProjectState> &p_project_state, const AINextWorkflowContextOptions &p_options, const Array &p_milestones);
	static String _build_project_memory(const AINextProjectMemory &p_memory);
	static String _build_current_task_context(const Ref<AINextProjectState> &p_project_state, const AINextWorkflowContextOptions &p_options, const Array &p_milestones);
	static String _build_prior_task_results(const Ref<AINextProjectState> &p_project_state, const AINextWorkflowContextOptions &p_options, const Array &p_milestones);
	static String _build_prior_milestone_results(const AINextWorkflowContextOptions &p_options, const Array &p_milestones);
	static String _build_recent_events(const Array &p_events, int p_limit);

public:
	static Array build_context(const Ref<AINextProjectState> &p_project_state, const Array &p_events, const AINextWorkflowContextOptions &p_options);
};
