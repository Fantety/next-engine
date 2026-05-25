/**************************************************************************/
/*  ai_manage_plan_tool.cpp                                                */
/**************************************************************************/

#include "ai_manage_plan_tool.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"
#include "editor/ai_component/planning/ai_plan_manager.h"

void AIManagePlanTool::_bind_methods() {
}

static Array _make_plan_action_enum() {
	Array values;
	values.push_back("create");
	values.push_back("set_task_status");
	values.push_back("archive");
	return values;
}

static Array _make_plan_status_enum() {
	Array values;
	values.push_back("pending");
	values.push_back("in_progress");
	values.push_back("completed");
	return values;
}

static String _format_plan_summary(const Dictionary &p_plan) {
	if (p_plan.is_empty()) {
		return "No active plan.";
	}

	String summary = "Plan: " + String(p_plan.get("title", "Plan")) + "\n";
	Array tasks = p_plan.get("tasks", Array());
	for (int i = 0; i < tasks.size(); i++) {
		if (Variant(tasks[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary task = tasks[i];
		summary += vformat("- %s [%s] %s\n", String(task.get("id", String())), String(task.get("status", "pending")), String(task.get("title", String())));
	}
	return summary.strip_edges();
}

String AIManagePlanTool::get_name() const {
	return "agent.manage_plan";
}

String AIManagePlanTool::get_description() const {
	return "Creates and updates the single visible Agent task plan for complex work.";
}

Dictionary AIManagePlanTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary action;
	action["type"] = "string";
	action["enum"] = _make_plan_action_enum();
	action["description"] = "create starts a new plan, set_task_status updates one task, archive releases the active plan.";
	properties["action"] = action;

	Dictionary title;
	title["type"] = "string";
	title["description"] = "Short plan title for create.";
	properties["title"] = title;

	Dictionary tasks;
	tasks["type"] = "array";
	tasks["description"] = "Tasks for create. Each item can be a string or an object with title and optional id/status.";
	Dictionary task_items;
	task_items["type"] = "object";
	Dictionary task_properties;
	Dictionary task_title;
	task_title["type"] = "string";
	task_properties["title"] = task_title;
	Dictionary task_id;
	task_id["type"] = "string";
	task_properties["id"] = task_id;
	Dictionary task_status;
	task_status["type"] = "string";
	task_status["enum"] = _make_plan_status_enum();
	task_properties["status"] = task_status;
	task_items["properties"] = task_properties;
	tasks["items"] = task_items;
	properties["tasks"] = tasks;

	Dictionary task_id_property;
	task_id_property["type"] = "string";
	task_id_property["description"] = "Task id to update for set_task_status.";
	properties["task_id"] = task_id_property;

	Dictionary status;
	status["type"] = "string";
	status["enum"] = _make_plan_status_enum();
	status["description"] = "New task status for set_task_status.";
	properties["status"] = status;

	schema["properties"] = properties;
	Array required;
	required.push_back("action");
	schema["required"] = required;
	return schema;
}

AIToolResult AIManagePlanTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	Ref<AIPlanManager> manager = AIPlanManager::get_singleton();
	const String action = String(p_arguments.get("action", String())).strip_edges().to_lower();

	String error;
	if (action == "create") {
		Array tasks;
		if (p_arguments.has("tasks") && Variant(p_arguments["tasks"]).get_type() == Variant::ARRAY) {
			tasks = p_arguments["tasks"];
		}
		if (!manager->create_plan(String(p_arguments.get("title", "Plan")), tasks, error)) {
			result.error = error;
			return result;
		}
		result.metadata["plan_action"] = "create";
		Dictionary active_plan = manager->get_active_plan();
		result.metadata["active_plan"] = active_plan;
		result.content = "Created active plan.\n" + _format_plan_summary(active_plan);
		return result;
	}

	if (action == "set_task_status") {
		if (!p_arguments.has("task_id") || String(p_arguments.get("task_id", String())).strip_edges().is_empty()) {
			result.error = "Missing required task_id.";
			return result;
		}
		if (!p_arguments.has("status") || String(p_arguments.get("status", String())).strip_edges().is_empty()) {
			result.error = "Missing required status.";
			return result;
		}
		if (!manager->update_task(String(p_arguments.get("task_id", String())), String(p_arguments.get("status", "pending")), error)) {
			result.error = error;
			return result;
		}
		result.metadata["plan_action"] = "set_task_status";
		Dictionary active_plan = manager->get_active_plan();
		Dictionary archived_plan = manager->get_archived_plan();
		result.metadata["active_plan"] = active_plan;
		result.metadata["archived_plan"] = archived_plan;
		if (manager->has_active_plan()) {
			result.content = "Updated plan task.\n" + _format_plan_summary(active_plan);
		} else {
			result.content = "Updated final plan task and archived the completed plan.\n" + _format_plan_summary(archived_plan);
		}
		return result;
	}

	if (action == "archive") {
		if (!manager->archive_plan(error)) {
			result.error = error;
			return result;
		}
		result.metadata["plan_action"] = "archive";
		Dictionary archived_plan = manager->get_archived_plan();
		result.metadata["archived_plan"] = archived_plan;
		result.content = "Archived active plan.\n" + _format_plan_summary(archived_plan);
		return result;
	}

	result.error = "Unsupported plan action.";
	return result;
}
