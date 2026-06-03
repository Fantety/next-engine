/**************************************************************************/
/*  ai_next_manage_project_tool.cpp                                       */
/**************************************************************************/

#include "ai_next_manage_project_tool.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"
#include "editor/ai_component/next/ai_next_agent_registry.h"

namespace {

Array _make_action_enum() {
	Array values;
	values.push_back("replace_plan");
	values.push_back("update_milestone");
	values.push_back("update_task");
	values.push_back("append_tasks");
	values.push_back("register_asset");
	values.push_back("mark_feedback_iteration");
	return values;
}

Array _make_agent_enum() {
	Array values;
	Vector<String> agent_ids = AINextAgentRegistry::get_assignable_agent_ids();
	for (int i = 0; i < agent_ids.size(); i++) {
		values.push_back(agent_ids[i]);
	}
	return values;
}

bool _is_allowed_agent_id(const String &p_agent_id) {
	const String agent_id = p_agent_id.strip_edges();
	return AINextAgentRegistry::is_assignable_agent_id(agent_id);
}

bool _has_string(const Vector<String> &p_values, const String &p_value) {
	for (const String &value : p_values) {
		if (value == p_value) {
			return true;
		}
	}
	return false;
}

bool _collect_task_ids(const Array &p_tasks, Vector<String> &r_task_ids, String &r_error) {
	for (int i = 0; i < p_tasks.size(); i++) {
		if (Variant(p_tasks[i]).get_type() != Variant::DICTIONARY) {
			r_error = "NEXT task entries must be dictionaries.";
			return false;
		}
		Dictionary task = p_tasks[i];
		const String task_id = String(task.get("id", String())).strip_edges();
		if (!task_id.is_empty()) {
			if (_has_string(r_task_ids, task_id)) {
				r_error = "Duplicate NEXT task id.";
				return false;
			}
			r_task_ids.push_back(task_id);
		}
	}
	return true;
}

bool _validate_task_payload(const Dictionary &p_task, String &r_error) {
	if (String(p_task.get("title", String())).strip_edges().is_empty()) {
		r_error = "NEXT task title is required.";
		return false;
	}
	const String agent_id = String(p_task.get("assigned_agent_id", String())).strip_edges();
	if (!_is_allowed_agent_id(agent_id)) {
		r_error = "NEXT task assigned_agent_id is unknown.";
		return false;
	}
	if (p_task.has("depends_on") && Variant(p_task["depends_on"]).get_type() != Variant::ARRAY) {
		r_error = "NEXT task depends_on must be an array.";
		return false;
	}
	if (p_task.has("asset_refs") && Variant(p_task["asset_refs"]).get_type() != Variant::ARRAY) {
		r_error = "NEXT task asset_refs must be an array.";
		return false;
	}
	if (p_task.has("output_paths") && Variant(p_task["output_paths"]).get_type() != Variant::ARRAY) {
		r_error = "NEXT task output_paths must be an array.";
		return false;
	}
	return true;
}

bool _validate_task_dependencies(const Dictionary &p_task, const Vector<String> &p_new_task_ids, const Ref<AINextProjectState> &p_existing_state, String &r_error) {
	if (!p_task.has("depends_on")) {
		return true;
	}
	Array depends_on = p_task["depends_on"];
	for (int i = 0; i < depends_on.size(); i++) {
		const String dependency_id = String(depends_on[i]).strip_edges();
		if (dependency_id.is_empty()) {
			r_error = "NEXT task dependency id is empty.";
			return false;
		}
		if (!_has_string(p_new_task_ids, dependency_id) && (p_existing_state.is_null() || !p_existing_state->has_task(dependency_id))) {
			r_error = "NEXT task depends on an unknown task.";
			return false;
		}
	}
	return true;
}

Dictionary _make_string_property(const String &p_description = String()) {
	Dictionary property;
	property["type"] = "string";
	if (!p_description.is_empty()) {
		property["description"] = p_description;
	}
	return property;
}

Dictionary _make_string_array_property() {
	Dictionary property;
	property["type"] = "array";
	Dictionary items;
	items["type"] = "string";
	property["items"] = items;
	return property;
}

} // namespace

void AINextManageProjectTool::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_project_state", "project_state"), &AINextManageProjectTool::set_project_state);
	ClassDB::bind_method(D_METHOD("get_project_state"), &AINextManageProjectTool::get_project_state);
}

void AINextManageProjectTool::set_project_state(const Ref<AINextProjectState> &p_project_state) {
	project_state = p_project_state;
}

Ref<AINextProjectState> AINextManageProjectTool::get_project_state() const {
	return project_state;
}

String AINextManageProjectTool::get_name() const {
	return "ai_next.manage_project";
}

String AINextManageProjectTool::get_description() const {
	return "Writes structured NEXT milestones, tasks, assets, feedback iterations, and task status updates. Use it instead of free-text plan parsing.";
}

Dictionary AINextManageProjectTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary action;
	action["type"] = "string";
	action["enum"] = _make_action_enum();
	properties["action"] = action;

	properties["milestone_id"] = _make_string_property("Milestone id for milestone or task operations.");
	properties["task_id"] = _make_string_property("Task id for update_task.");
	properties["path"] = _make_string_property("Asset path for register_asset.");
	properties["source"] = _make_string_property("Asset source for register_asset.");
	properties["parent_asset_id"] = _make_string_property();
	properties["baseline_milestone_id"] = _make_string_property();

	Dictionary protected_property;
	protected_property["type"] = "boolean";
	properties["protected_from_agent_edits"] = protected_property;

	Dictionary feedback_iteration;
	feedback_iteration["type"] = "integer";
	properties["feedback_iteration"] = feedback_iteration;

	Dictionary milestone_patch;
	milestone_patch["type"] = "object";
	properties["milestone"] = milestone_patch;

	Dictionary task_patch;
	task_patch["type"] = "object";
	properties["task"] = task_patch;

	Dictionary task;
	task["type"] = "object";
	Dictionary task_properties;
	task_properties["id"] = _make_string_property();
	task_properties["title"] = _make_string_property();
	task_properties["description"] = _make_string_property();
	Dictionary assigned_agent_id = _make_string_property();
	assigned_agent_id["enum"] = _make_agent_enum();
	task_properties["assigned_agent_id"] = assigned_agent_id;
	task_properties["depends_on"] = _make_string_array_property();
	task_properties["asset_refs"] = _make_string_array_property();
	task_properties["output_paths"] = _make_string_array_property();
	task["properties"] = task_properties;

	Dictionary tasks;
	tasks["type"] = "array";
	tasks["items"] = task;
	properties["tasks"] = tasks;

	Dictionary milestone;
	milestone["type"] = "object";
	Dictionary milestone_properties;
	milestone_properties["id"] = _make_string_property();
	milestone_properties["title"] = _make_string_property();
	milestone_properties["description"] = _make_string_property();
	milestone_properties["tasks"] = tasks;
	milestone["properties"] = milestone_properties;

	Dictionary milestones;
	milestones["type"] = "array";
	milestones["items"] = milestone;
	properties["milestones"] = milestones;

	schema["properties"] = properties;
	Array required;
	required.push_back("action");
	schema["required"] = required;
	return schema;
}

bool AINextManageProjectTool::_validate_replace_plan(const Array &p_milestones, String &r_error) const {
	Vector<String> milestone_ids;
	Vector<String> task_ids;

	for (int i = 0; i < p_milestones.size(); i++) {
		if (Variant(p_milestones[i]).get_type() != Variant::DICTIONARY) {
			r_error = "NEXT milestone entries must be dictionaries.";
			return false;
		}
		Dictionary milestone = p_milestones[i];
		const String milestone_id = String(milestone.get("id", String())).strip_edges();
		if (!milestone_id.is_empty()) {
			if (_has_string(milestone_ids, milestone_id)) {
				r_error = "Duplicate NEXT milestone id.";
				return false;
			}
			milestone_ids.push_back(milestone_id);
		}
		if (String(milestone.get("title", String())).strip_edges().is_empty()) {
			r_error = "NEXT milestone title is required.";
			return false;
		}
		if (milestone.has("tasks") && Variant(milestone["tasks"]).get_type() != Variant::ARRAY) {
			r_error = "NEXT milestone tasks must be an array.";
			return false;
		}
		Array tasks = milestone.get("tasks", Array());
		if (!_collect_task_ids(tasks, task_ids, r_error)) {
			return false;
		}
	}

	for (int i = 0; i < p_milestones.size(); i++) {
		Dictionary milestone = p_milestones[i];
		Array tasks = milestone.get("tasks", Array());
		for (int j = 0; j < tasks.size(); j++) {
			Dictionary task = tasks[j];
			if (!_validate_task_payload(task, r_error)) {
				return false;
			}
			if (!_validate_task_dependencies(task, task_ids, Ref<AINextProjectState>(), r_error)) {
				return false;
			}
		}
	}

	Ref<AINextProjectState> temp_state;
	temp_state.instantiate();
	if (!temp_state->replace_from_milestones_array(p_milestones, r_error)) {
		return false;
	}
	if (temp_state->has_dependency_cycle(r_error)) {
		return false;
	}
	return true;
}

bool AINextManageProjectTool::_validate_append_tasks(const String &p_milestone_id, const Array &p_tasks, String &r_error) const {
	if (project_state.is_null()) {
		r_error = "NEXT project state is not configured.";
		return false;
	}
	if (project_state->get_milestone(p_milestone_id).is_empty()) {
		r_error = "Unknown NEXT milestone.";
		return false;
	}

	Vector<String> task_ids;
	if (!_collect_task_ids(p_tasks, task_ids, r_error)) {
		return false;
	}
	for (const String &task_id : task_ids) {
		if (project_state->has_task(task_id)) {
			r_error = "Duplicate NEXT task id.";
			return false;
		}
	}
	for (int i = 0; i < p_tasks.size(); i++) {
		Dictionary task = p_tasks[i];
		if (!_validate_task_payload(task, r_error)) {
			return false;
		}
		if (!_validate_task_dependencies(task, task_ids, project_state, r_error)) {
			return false;
		}
	}

	Ref<AINextProjectState> temp_state;
	temp_state.instantiate();
	temp_state->load_from_dict(project_state->to_dict());
	if (!temp_state->append_tasks(p_milestone_id, p_tasks, r_error)) {
		return false;
	}
	if (temp_state->has_dependency_cycle(r_error)) {
		return false;
	}
	return true;
}

AIToolResult AINextManageProjectTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	if (project_state.is_null()) {
		result.error = "NEXT project state is not configured.";
		return result;
	}

	const String action = String(p_arguments.get("action", String())).strip_edges().to_lower();
	if (action.is_empty()) {
		result.error = "Missing required action.";
		return result;
	}

	String error;
	if (action == "replace_plan") {
		if (!p_arguments.has("milestones") || Variant(p_arguments["milestones"]).get_type() != Variant::ARRAY) {
			result.error = "replace_plan requires milestones array.";
			return result;
		}
		Array milestones = p_arguments["milestones"];
		if (!_validate_replace_plan(milestones, error)) {
			result.error = error;
			return result;
		}
		if (!project_state->replace_from_milestones_array(milestones, error)) {
			result.error = error;
			return result;
		}
		result.content = "Replaced NEXT project plan.";
		result.metadata["action"] = action;
		result.metadata["milestone_count"] = project_state->get_milestone_count();
		return result;
	}

	if (action == "update_milestone") {
		const String milestone_id = String(p_arguments.get("milestone_id", String())).strip_edges();
		if (milestone_id.is_empty() || !p_arguments.has("milestone") || Variant(p_arguments["milestone"]).get_type() != Variant::DICTIONARY) {
			result.error = "update_milestone requires milestone_id and milestone patch.";
			return result;
		}
		if (!project_state->update_milestone(milestone_id, p_arguments["milestone"], error)) {
			result.error = error;
			return result;
		}
		result.content = "Updated NEXT milestone.";
		result.metadata["action"] = action;
		result.metadata["milestone_id"] = milestone_id;
		return result;
	}

	if (action == "update_task") {
		const String task_id = String(p_arguments.get("task_id", String())).strip_edges();
		if (task_id.is_empty() || !p_arguments.has("task") || Variant(p_arguments["task"]).get_type() != Variant::DICTIONARY) {
			result.error = "update_task requires task_id and task patch.";
			return result;
		}
		Dictionary task_patch = p_arguments["task"];
		if (task_patch.has("assigned_agent_id") && !_is_allowed_agent_id(String(task_patch["assigned_agent_id"]))) {
			result.error = "NEXT task assigned_agent_id is unknown.";
			return result;
		}
		if (task_patch.has("depends_on") && Variant(task_patch["depends_on"]).get_type() != Variant::ARRAY) {
			result.error = "NEXT task depends_on must be an array.";
			return result;
		}
		if (task_patch.has("asset_refs") && Variant(task_patch["asset_refs"]).get_type() != Variant::ARRAY) {
			result.error = "NEXT task asset_refs must be an array.";
			return result;
		}
		if (task_patch.has("output_paths") && Variant(task_patch["output_paths"]).get_type() != Variant::ARRAY) {
			result.error = "NEXT task output_paths must be an array.";
			return result;
		}
		if (task_patch.has("depends_on")) {
			Vector<String> no_new_task_ids;
			if (!_validate_task_dependencies(task_patch, no_new_task_ids, project_state, error)) {
				result.error = error;
				return result;
			}
		}

		Ref<AINextProjectState> temp_state;
		temp_state.instantiate();
		temp_state->load_from_dict(project_state->to_dict());
		if (!temp_state->update_task(task_id, task_patch, error)) {
			result.error = error;
			return result;
		}
		if (temp_state->has_dependency_cycle(error)) {
			result.error = error;
			return result;
		}
		if (!project_state->update_task(task_id, task_patch, error)) {
			result.error = error;
			return result;
		}
		result.content = "Updated NEXT task.";
		result.metadata["action"] = action;
		result.metadata["task_id"] = task_id;
		return result;
	}

	if (action == "append_tasks") {
		const String milestone_id = String(p_arguments.get("milestone_id", String())).strip_edges();
		if (milestone_id.is_empty() || !p_arguments.has("tasks") || Variant(p_arguments["tasks"]).get_type() != Variant::ARRAY) {
			result.error = "append_tasks requires milestone_id and tasks array.";
			return result;
		}
		Array tasks = p_arguments["tasks"];
		if (!_validate_append_tasks(milestone_id, tasks, error)) {
			result.error = error;
			return result;
		}
		if (!project_state->append_tasks(milestone_id, tasks, error)) {
			result.error = error;
			return result;
		}
		result.content = "Appended NEXT tasks.";
		result.metadata["action"] = action;
		result.metadata["milestone_id"] = milestone_id;
		return result;
	}

	if (action == "register_asset") {
		const String path = String(p_arguments.get("path", String())).strip_edges();
		if (path.is_empty()) {
			result.error = "register_asset requires path.";
			return result;
		}
		const String asset_id = project_state->register_asset(
				path,
				String(p_arguments.get("source", "agent_generated")),
				bool(p_arguments.get("protected_from_agent_edits", false)),
				String(p_arguments.get("parent_asset_id", String())),
				String(p_arguments.get("baseline_milestone_id", String())));
		if (asset_id.is_empty()) {
			result.error = project_state->get_last_error();
			return result;
		}
		result.content = "Registered NEXT asset.";
		result.metadata["action"] = action;
		result.metadata["asset_id"] = asset_id;
		return result;
	}

	if (action == "mark_feedback_iteration") {
		const String milestone_id = String(p_arguments.get("milestone_id", String())).strip_edges();
		if (milestone_id.is_empty()) {
			result.error = "mark_feedback_iteration requires milestone_id.";
			return result;
		}
		Dictionary patch;
		patch["feedback_iteration"] = (int)p_arguments.get("feedback_iteration", 0);
		if (!project_state->update_milestone(milestone_id, patch, error)) {
			result.error = error;
			return result;
		}
		result.content = "Updated NEXT feedback iteration.";
		result.metadata["action"] = action;
		result.metadata["milestone_id"] = milestone_id;
		return result;
	}

	result.error = "Unsupported NEXT project action.";
	return result;
}
