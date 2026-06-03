/**************************************************************************/
/*  ai_next_workflow_context_builder.cpp                                  */
/**************************************************************************/

#include "ai_next_workflow_context_builder.h"

#include "editor/ai_component/context/ai_context_document.h"

Dictionary AINextWorkflowContextBuilder::_make_document(const String &p_title, const String &p_source, const String &p_content) {
	AIContextDocument doc;
	doc.title = p_title;
	doc.source = p_source;
	doc.content = p_content;
	return doc.to_dict();
}

bool AINextWorkflowContextBuilder::_has_string(const Vector<String> &p_values, const String &p_value) {
	for (const String &value : p_values) {
		if (value == p_value) {
			return true;
		}
	}
	return false;
}

bool AINextWorkflowContextBuilder::_is_result_status(const String &p_status) {
	return p_status == "completed" || p_status == "skipped" || p_status == "failed";
}

String AINextWorkflowContextBuilder::_format_string_array(const Array &p_values) {
	String text;
	for (int i = 0; i < p_values.size(); i++) {
		const String value = String(p_values[i]).strip_edges();
		if (value.is_empty()) {
			continue;
		}
		if (!text.is_empty()) {
			text += ", ";
		}
		text += value;
	}
	return text;
}

String AINextWorkflowContextBuilder::_format_task_result(const Dictionary &p_task) {
	String line = "- ";
	line += String(p_task.get("id", String()));
	line += " | ";
	line += String(p_task.get("title", String()));
	line += " | agent=" + String(p_task.get("assigned_agent_id", String()));
	line += " | status=" + String(p_task.get("status", String()));

	const String result_summary = String(p_task.get("result_summary", String())).strip_edges();
	if (!result_summary.is_empty()) {
		line += "\n  result: " + result_summary;
	}
	if (p_task.has("output_paths") && Variant(p_task["output_paths"]).get_type() == Variant::ARRAY) {
		const String output_paths = _format_string_array(p_task["output_paths"]);
		if (!output_paths.is_empty()) {
			line += "\n  outputs: " + output_paths;
		}
	}
	const String error = String(p_task.get("error", String())).strip_edges();
	if (!error.is_empty()) {
		line += "\n  error: " + error;
	}
	return line;
}

int AINextWorkflowContextBuilder::_find_milestone_index(const Array &p_milestones, const String &p_milestone_id) {
	for (int i = 0; i < p_milestones.size(); i++) {
		if (Variant(p_milestones[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary milestone = p_milestones[i];
		if (String(milestone.get("id", String())) == p_milestone_id) {
			return i;
		}
	}
	return -1;
}

int AINextWorkflowContextBuilder::_find_task_index(const Array &p_tasks, const String &p_task_id) {
	for (int i = 0; i < p_tasks.size(); i++) {
		if (Variant(p_tasks[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary task = p_tasks[i];
		if (String(task.get("id", String())) == p_task_id) {
			return i;
		}
	}
	return -1;
}

Dictionary AINextWorkflowContextBuilder::_find_task(const Array &p_milestones, const String &p_task_id) {
	for (int i = 0; i < p_milestones.size(); i++) {
		if (Variant(p_milestones[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary milestone = p_milestones[i];
		if (!milestone.has("tasks") || Variant(milestone["tasks"]).get_type() != Variant::ARRAY) {
			continue;
		}
		Array tasks = milestone["tasks"];
		for (int j = 0; j < tasks.size(); j++) {
			if (Variant(tasks[j]).get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary task = tasks[j];
			if (String(task.get("id", String())) == p_task_id) {
				return task;
			}
		}
	}
	return Dictionary();
}

void AINextWorkflowContextBuilder::_append_task_if_relevant(const Dictionary &p_task, Vector<Dictionary> &r_tasks, Vector<String> &r_task_ids) {
	const String task_id = String(p_task.get("id", String()));
	if (task_id.is_empty() || _has_string(r_task_ids, task_id)) {
		return;
	}
	if (!_is_result_status(String(p_task.get("status", String())))) {
		return;
	}
	r_task_ids.push_back(task_id);
	r_tasks.push_back(p_task);
}

String AINextWorkflowContextBuilder::_build_workflow_state(const Ref<AINextProjectState> &p_project_state, const AINextWorkflowContextOptions &p_options, const Array &p_milestones) {
	String content;
	content += "Operation: " + p_options.operation + "\n";
	content += "Brief: " + p_project_state->get_brief().strip_edges() + "\n";
	content += "Active milestone id: " + p_project_state->get_active_milestone_id() + "\n";
	if (!p_options.milestone_id.is_empty()) {
		const int milestone_index = _find_milestone_index(p_milestones, p_options.milestone_id);
		if (milestone_index >= 0) {
			Dictionary milestone = p_milestones[milestone_index];
			content += "Current milestone: " + String(milestone.get("title", String())) + "\n";
			content += "Milestone description: " + String(milestone.get("description", String())) + "\n";
			content += "Milestone status: " + String(milestone.get("status", String())) + "\n";
		}
	}
	return content.strip_edges();
}

String AINextWorkflowContextBuilder::_build_project_memory(const AINextProjectMemory &p_memory) {
	if (p_memory.is_empty()) {
		return String();
	}

	String content;
	if (!p_memory.language.strip_edges().is_empty()) {
		content += "Language preference: " + p_memory.language.strip_edges() + "\n";
	}
	if (!p_memory.renderer.strip_edges().is_empty()) {
		content += "Renderer: " + p_memory.renderer.strip_edges() + "\n";
	}

	const struct {
		const char *title;
		const Vector<String> *values;
	} sections[] = {
		{ "Architecture notes", &p_memory.architecture_notes },
		{ "Coding conventions", &p_memory.coding_conventions },
		{ "Scene conventions", &p_memory.scene_conventions },
		{ "User preferences", &p_memory.user_preferences },
	};

	for (const auto &section : sections) {
		if (section.values->is_empty()) {
			continue;
		}
		content += String(section.title) + ":\n";
		for (const String &value : *section.values) {
			const String clean_value = value.strip_edges();
			if (!clean_value.is_empty()) {
				content += "- " + clean_value + "\n";
			}
		}
	}
	return content.strip_edges();
}

String AINextWorkflowContextBuilder::_build_current_task_context(const Ref<AINextProjectState> &p_project_state, const AINextWorkflowContextOptions &p_options, const Array &p_milestones) {
	if (p_options.task_id.is_empty()) {
		return String();
	}

	Dictionary task = p_project_state->get_task(p_options.task_id);
	if (task.is_empty()) {
		return String();
	}

	String content;
	content += "Current task: " + String(task.get("id", String())) + " | " + String(task.get("title", String())) + "\n";
	content += "Assigned agent: " + String(task.get("assigned_agent_id", String())) + "\n";
	content += "Status: " + String(task.get("status", String())) + "\n";
	const String description = String(task.get("description", String())).strip_edges();
	if (!description.is_empty()) {
		content += "Description: " + description + "\n";
	}

	if (p_options.include_dependency_details && task.has("depends_on") && Variant(task["depends_on"]).get_type() == Variant::ARRAY) {
		Array depends_on = task["depends_on"];
		if (!depends_on.is_empty()) {
			content += "\nDependencies:\n";
			for (int i = 0; i < depends_on.size(); i++) {
				Dictionary dependency = _find_task(p_milestones, String(depends_on[i]));
				if (dependency.is_empty()) {
					content += "- " + String(depends_on[i]) + " | missing\n";
				} else {
					content += _format_task_result(dependency) + "\n";
				}
			}
		}
	}
	return content.strip_edges();
}

String AINextWorkflowContextBuilder::_build_prior_task_results(const Ref<AINextProjectState> &p_project_state, const AINextWorkflowContextOptions &p_options, const Array &p_milestones) {
	Vector<Dictionary> relevant_tasks;
	Vector<String> relevant_task_ids;

	Dictionary current_task = p_options.task_id.is_empty() ? Dictionary() : p_project_state->get_task(p_options.task_id);
	if (!current_task.is_empty() && current_task.has("depends_on") && Variant(current_task["depends_on"]).get_type() == Variant::ARRAY) {
		Array depends_on = current_task["depends_on"];
		for (int i = 0; i < depends_on.size(); i++) {
			_append_task_if_relevant(_find_task(p_milestones, String(depends_on[i])), relevant_tasks, relevant_task_ids);
		}
	}

	const int milestone_index = _find_milestone_index(p_milestones, p_options.milestone_id);
	if (milestone_index >= 0 && Variant(p_milestones[milestone_index]).get_type() == Variant::DICTIONARY) {
		Dictionary milestone = p_milestones[milestone_index];
		if (milestone.has("tasks") && Variant(milestone["tasks"]).get_type() == Variant::ARRAY) {
			Array tasks = milestone["tasks"];
			const int current_task_index = _find_task_index(tasks, p_options.task_id);
			const int upper_bound = current_task_index >= 0 ? current_task_index : tasks.size();
			for (int i = 0; i < upper_bound && relevant_tasks.size() < p_options.max_completed_tasks; i++) {
				if (Variant(tasks[i]).get_type() == Variant::DICTIONARY) {
					_append_task_if_relevant(tasks[i], relevant_tasks, relevant_task_ids);
				}
			}
		}
	}

	if (relevant_tasks.is_empty()) {
		return String();
	}

	String content = "Relevant prior task results are selected by dependency, then same-milestone order.\n";
	for (int i = 0; i < relevant_tasks.size(); i++) {
		content += _format_task_result(relevant_tasks[i]) + "\n";
	}
	return content.strip_edges();
}

String AINextWorkflowContextBuilder::_build_prior_milestone_results(const AINextWorkflowContextOptions &p_options, const Array &p_milestones) {
	const int current_index = _find_milestone_index(p_milestones, p_options.milestone_id);
	if (current_index <= 0) {
		return String();
	}

	const int start = MAX(0, current_index - p_options.max_previous_milestones);
	String content;
	for (int i = start; i < current_index; i++) {
		if (Variant(p_milestones[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary milestone = p_milestones[i];
		content += "- " + String(milestone.get("id", String())) + " | " + String(milestone.get("title", String())) + " | status=" + String(milestone.get("status", String())) + "\n";
		const String description = String(milestone.get("description", String())).strip_edges();
		if (!description.is_empty()) {
			content += "  description: " + description + "\n";
		}
		if (milestone.has("tasks") && Variant(milestone["tasks"]).get_type() == Variant::ARRAY) {
			Array tasks = milestone["tasks"];
			for (int j = 0; j < tasks.size(); j++) {
				if (Variant(tasks[j]).get_type() != Variant::DICTIONARY) {
					continue;
				}
				Dictionary task = tasks[j];
				if (_is_result_status(String(task.get("status", String())))) {
					content += "  " + _format_task_result(task).replace("\n", "\n  ") + "\n";
				}
			}
		}
	}
	return content.strip_edges();
}

String AINextWorkflowContextBuilder::_build_recent_events(const Array &p_events, int p_limit) {
	if (p_limit <= 0 || p_events.is_empty()) {
		return String();
	}

	String content;
	int added = 0;
	for (int i = p_events.size() - 1; i >= 0 && added < p_limit; i--) {
		if (Variant(p_events[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary event = p_events[i];
		const String event_type = String(event.get("event_type", String()));
		if (event_type != "task_completed" && event_type != "task_failed" && event_type != "milestone_run_completed" && event_type != "review_completed" && event_type != "feedback_tasks_generated") {
			continue;
		}
		content += "- " + event_type + " | milestone=" + String(event.get("milestone_id", String())) + " | task=" + String(event.get("task_id", String())) + " | agent=" + String(event.get("agent_id", String())) + "\n";
		const String message = String(event.get("message", String())).strip_edges();
		if (!message.is_empty()) {
			content += "  message: " + message + "\n";
		}
		added++;
	}
	return content.strip_edges();
}

Array AINextWorkflowContextBuilder::build_context(const Ref<AINextProjectState> &p_project_state, const Array &p_events, const AINextWorkflowContextOptions &p_options) {
	Array context;
	if (p_project_state.is_null()) {
		return context;
	}

	const Array milestones = p_project_state->get_milestones_as_array();
	if (p_options.has_project_memory) {
		const String project_memory = _build_project_memory(p_options.project_memory);
		if (!project_memory.is_empty()) {
			context.push_back(_make_document("NEXT Project Memory", "ai_next/project_memory", project_memory));
		}
	}

	const String workflow_state = _build_workflow_state(p_project_state, p_options, milestones);
	if (!workflow_state.is_empty()) {
		context.push_back(_make_document("NEXT Workflow State", "ai_next/workflow_state", workflow_state));
	}

	const String current_task = _build_current_task_context(p_project_state, p_options, milestones);
	if (!current_task.is_empty()) {
		context.push_back(_make_document("NEXT Current Task Context", "ai_next/current_task", current_task));
	}

	const String prior_tasks = _build_prior_task_results(p_project_state, p_options, milestones);
	if (!prior_tasks.is_empty()) {
		context.push_back(_make_document("NEXT Prior Task Results", "ai_next/prior_tasks", prior_tasks));
	}

	const String prior_milestones = _build_prior_milestone_results(p_options, milestones);
	if (!prior_milestones.is_empty()) {
		context.push_back(_make_document("NEXT Prior Milestone Results", "ai_next/prior_milestones", prior_milestones));
	}

	const String recent_events = _build_recent_events(p_events, p_options.max_recent_events);
	if (!recent_events.is_empty()) {
		context.push_back(_make_document("NEXT Recent Events", "ai_next/event_log", recent_events));
	}

	return context;
}
