/**************************************************************************/
/*  ai_next_task_tree.cpp                                                 */
/**************************************************************************/

#include "ai_next_task_tree.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/ai_component/ui/next/ai_next_plan_rows.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/text_edit.h"
#include "servers/text/text_server.h"

namespace {

void _add_agent_item(OptionButton *p_selector, const String &p_label, const String &p_agent_id, const String &p_selected_agent_id) {
	const int index = p_selector->get_item_count();
	p_selector->add_item(p_label);
	p_selector->set_item_metadata(index, p_agent_id);
	if (p_agent_id == p_selected_agent_id) {
		p_selector->select(index);
	}
}

} // namespace

void AINextTaskTree::_bind_methods() {
	ClassDB::bind_method(D_METHOD("refresh"), &AINextTaskTree::refresh);
}

AINextTaskTree::AINextTaskTree() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 4 * EDSCALE);
	_build_dialogs();
}

void AINextTaskTree::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
		refresh();
	}
}

void AINextTaskTree::_clear_rows() {
	for (int i = get_child_count() - 1; i >= 0; i--) {
		Node *child = get_child(i);
		if (child == task_dialog || child == delete_dialog || child == dependency_dialog) {
			continue;
		}
		remove_child(child);
		memdelete(child);
	}
}

void AINextTaskTree::set_next_session(AIAgentNextSession *p_session) {
	next_session = p_session;
	refresh();
}

void AINextTaskTree::_task_pressed(const String &p_task_id) {
	if (!next_session) {
		return;
	}
	next_session->select_task(p_task_id);
}

void AINextTaskTree::_run_task_pressed(const String &p_task_id) {
	if (!next_session) {
		return;
	}
	next_session->run_task(p_task_id);
}

void AINextTaskTree::_build_dialogs() {
	if (task_dialog) {
		return;
	}

	task_dialog = memnew(ConfirmationDialog);
	task_dialog->set_min_size(Size2(460, 340) * EDSCALE);
	task_dialog->set_ok_button_text(TTR("Save"));
	add_child(task_dialog);

	VBoxContainer *task_form = memnew(VBoxContainer);
	task_form->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	task_form->add_theme_constant_override("separation", 6 * EDSCALE);
	task_dialog->add_child(task_form);

	task_title_edit = memnew(LineEdit);
	task_title_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	task_title_edit->set_placeholder(TTR("Task title"));
	task_form->add_child(task_title_edit);
	task_dialog->register_text_enter(task_title_edit);

	task_description_edit = memnew(TextEdit);
	task_description_edit->set_custom_minimum_size(Size2(0, 110) * EDSCALE);
	task_description_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	task_description_edit->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	task_description_edit->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	task_description_edit->set_placeholder(TTR("Description"));
	task_form->add_child(task_description_edit);

	task_agent_selector = memnew(OptionButton);
	task_agent_selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	task_form->add_child(task_agent_selector);

	task_milestone_selector = memnew(OptionButton);
	task_milestone_selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	task_form->add_child(task_milestone_selector);
	task_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AINextTaskTree::_confirm_task_dialog));

	delete_dialog = memnew(ConfirmationDialog);
	delete_dialog->set_title(TTR("Delete Task"));
	delete_dialog->set_text(TTR("Delete this NEXT task?"));
	delete_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AINextTaskTree::_confirm_delete_task));
	add_child(delete_dialog);

	dependency_dialog = memnew(ConfirmationDialog);
	dependency_dialog->set_title(TTR("Task Dependencies"));
	dependency_dialog->set_min_size(Size2(420, 320) * EDSCALE);
	add_child(dependency_dialog);

	ScrollContainer *dependency_scroll = memnew(ScrollContainer);
	dependency_scroll->set_custom_minimum_size(Size2(0, 220) * EDSCALE);
	dependency_scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	dependency_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	dependency_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	dependency_dialog->add_child(dependency_scroll);

	dependency_list = memnew(VBoxContainer);
	dependency_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	dependency_list->add_theme_constant_override("separation", 3 * EDSCALE);
	dependency_scroll->add_child(dependency_list);
	dependency_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AINextTaskTree::_confirm_dependencies));
}

void AINextTaskTree::_populate_milestone_selector(const String &p_selected_milestone_id) {
	task_milestone_selector->clear();
	if (!next_session || next_session->get_project_state().is_null()) {
		return;
	}

	Array milestones = next_session->get_project_state()->get_milestones_as_array();
	for (int i = 0; i < milestones.size(); i++) {
		if (Variant(milestones[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary milestone = milestones[i];
		if (String(milestone.get("status", String())) == "locked") {
			continue;
		}
		const String milestone_id = String(milestone.get("id", String()));
		const int index = task_milestone_selector->get_item_count();
		task_milestone_selector->add_item(String(milestone.get("title", TTR("Milestone"))));
		task_milestone_selector->set_item_metadata(index, milestone_id);
		if (milestone_id == p_selected_milestone_id) {
			task_milestone_selector->select(index);
		}
	}
}

void AINextTaskTree::_add_task_pressed() {
	if (!next_session || !next_session->can_edit_plan()) {
		return;
	}
	editing_task_id.clear();
	task_title_edit->clear();
	task_description_edit->clear();
	task_agent_selector->clear();
	_add_agent_item(task_agent_selector, TTR("Script Agent"), "script_agent", "script_agent");
	_add_agent_item(task_agent_selector, TTR("Scene Agent"), "scene_agent", "script_agent");
	_add_agent_item(task_agent_selector, TTR("Shader Agent"), "shader_agent", "script_agent");
	_populate_milestone_selector(next_session->get_project_state()->get_active_milestone_id());
	task_dialog->set_title(TTR("Add Task"));
	task_dialog->popup_centered(Size2(460, 340) * EDSCALE);
}

void AINextTaskTree::_edit_task_pressed(const String &p_task_id) {
	if (!next_session || !next_session->can_edit_plan() || next_session->get_project_state().is_null()) {
		return;
	}
	Dictionary task = next_session->get_project_state()->get_task(p_task_id);
	if (task.is_empty()) {
		return;
	}
	editing_task_id = p_task_id;
	const String assigned_agent_id = String(task.get("assigned_agent_id", "script_agent"));
	task_title_edit->set_text(String(task.get("title", String())));
	task_description_edit->set_text(String(task.get("description", String())));
	task_agent_selector->clear();
	_add_agent_item(task_agent_selector, TTR("Script Agent"), "script_agent", assigned_agent_id);
	_add_agent_item(task_agent_selector, TTR("Scene Agent"), "scene_agent", assigned_agent_id);
	_add_agent_item(task_agent_selector, TTR("Shader Agent"), "shader_agent", assigned_agent_id);
	_populate_milestone_selector(next_session->get_project_state()->get_task_milestone_id(p_task_id));
	task_dialog->set_title(TTR("Edit Task"));
	task_dialog->popup_centered(Size2(460, 340) * EDSCALE);
}

void AINextTaskTree::_confirm_task_dialog() {
	if (!next_session || !task_agent_selector || !task_milestone_selector) {
		return;
	}
	const String title = task_title_edit ? task_title_edit->get_text().strip_edges() : String();
	if (title.is_empty()) {
		return;
	}
	const int agent_index = task_agent_selector->get_selected();
	const int milestone_index = task_milestone_selector->get_selected();
	const String agent_id = agent_index >= 0 ? String(task_agent_selector->get_item_metadata(agent_index)) : String("script_agent");
	const String milestone_id = milestone_index >= 0 ? String(task_milestone_selector->get_item_metadata(milestone_index)) : String();
	const String description = task_description_edit ? task_description_edit->get_text() : String();

	if (editing_task_id.is_empty()) {
		(void)next_session->create_user_task(milestone_id, title, agent_id, Array(), description);
	} else {
		const String original_milestone_id = next_session->get_project_state()->get_task_milestone_id(editing_task_id);
		if (next_session->edit_user_task(editing_task_id, title, description, agent_id) && original_milestone_id != milestone_id) {
			next_session->move_user_task(editing_task_id, milestone_id, 9999);
		}
	}
	refresh();
}

void AINextTaskTree::_delete_task_pressed(const String &p_task_id) {
	if (!next_session || !next_session->can_edit_plan()) {
		return;
	}
	pending_delete_task_id = p_task_id;
	delete_dialog->popup_centered(Size2(320, 100) * EDSCALE);
}

void AINextTaskTree::_confirm_delete_task() {
	if (next_session && !pending_delete_task_id.is_empty()) {
		next_session->delete_user_task(pending_delete_task_id);
	}
	pending_delete_task_id.clear();
	refresh();
}

void AINextTaskTree::_move_task_pressed(const String &p_task_id, int p_to_index) {
	if (!next_session || !next_session->can_edit_plan() || next_session->get_project_state().is_null()) {
		return;
	}
	next_session->move_user_task(p_task_id, next_session->get_project_state()->get_task_milestone_id(p_task_id), p_to_index);
	refresh();
}

void AINextTaskTree::_populate_dependency_dialog(const String &p_task_id) {
	dependency_task_id = p_task_id;
	dependency_checks.clear();
	while (dependency_list->get_child_count() > 0) {
		Node *child = dependency_list->get_child(0);
		dependency_list->remove_child(child);
		memdelete(child);
	}
	if (!next_session || next_session->get_project_state().is_null()) {
		return;
	}

	Dictionary edited_task = next_session->get_project_state()->get_task(p_task_id);
	Array selected_dependencies = edited_task.get("depends_on", Array());
	Array milestones = next_session->get_project_state()->get_milestones_as_array();
	for (int i = 0; i < milestones.size(); i++) {
		if (Variant(milestones[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary milestone = milestones[i];
		Array tasks = milestone.get("tasks", Array());
		for (int j = 0; j < tasks.size(); j++) {
			if (Variant(tasks[j]).get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary task = tasks[j];
			const String task_id = String(task.get("id", String()));
			if (task_id == p_task_id) {
				continue;
			}
			CheckBox *check = memnew(CheckBox);
			check->set_text(vformat("%s / %s", String(milestone.get("title", TTR("Milestone"))), String(task.get("title", TTR("Task")))));
			check->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
			check->set_meta(SNAME("task_id"), task_id);
			check->set_pressed(selected_dependencies.has(task_id));
			dependency_list->add_child(check);
			dependency_checks.push_back(check);
		}
	}
	if (dependency_checks.is_empty()) {
		Label *empty = memnew(Label);
		empty->set_text(TTR("No other tasks"));
		empty->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
		dependency_list->add_child(empty);
	}
}

void AINextTaskTree::_dependencies_pressed(const String &p_task_id) {
	if (!next_session || !next_session->can_edit_plan()) {
		return;
	}
	_populate_dependency_dialog(p_task_id);
	dependency_dialog->popup_centered(Size2(420, 320) * EDSCALE);
}

void AINextTaskTree::_confirm_dependencies() {
	if (!next_session || dependency_task_id.is_empty()) {
		return;
	}
	Array depends_on;
	for (CheckBox *check : dependency_checks) {
		if (check && check->is_pressed()) {
			depends_on.push_back(String(check->get_meta(SNAME("task_id"), String())));
		}
	}
	next_session->set_user_task_dependencies(dependency_task_id, depends_on);
	dependency_task_id.clear();
	refresh();
}

Variant AINextTaskTree::_get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	if (!next_session || !next_session->can_edit_plan() || !p_from || next_session->get_project_state().is_null()) {
		return Variant();
	}
	const String task_id = String(p_from->get_meta(SNAME("ai_next_task_id"), String()));
	const int from_index = (int)p_from->get_meta(SNAME("ai_next_index"), -1);
	Dictionary task = next_session->get_project_state()->get_task(task_id);
	const String milestone_id = next_session->get_project_state()->get_task_milestone_id(task_id);
	Dictionary milestone = next_session->get_project_state()->get_milestone(milestone_id);
	if (task.is_empty() || milestone.is_empty() || String(milestone.get("status", String())) == "locked" || from_index < 0) {
		return Variant();
	}

	Dictionary drag_data;
	drag_data["type"] = "ai_next_task";
	drag_data["task_id"] = task_id;
	drag_data["milestone_id"] = milestone_id;
	drag_data["from_index"] = from_index;
	p_from->set_drag_preview(make_ai_next_plan_drag_preview(String(task.get("title", TTR("Task")))));
	return drag_data;
}

bool AINextTaskTree::_can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	if (!next_session || !next_session->can_edit_plan() || next_session->get_project_state().is_null() || p_data.get_type() != Variant::DICTIONARY) {
		return false;
	}
	Dictionary drag_data = p_data;
	if (String(drag_data.get("type", String())) != "ai_next_task") {
		return false;
	}
	const String active_milestone_id = next_session->get_project_state()->get_active_milestone_id();
	if (String(drag_data.get("milestone_id", String())) != active_milestone_id) {
		return false;
	}
	Dictionary milestone = next_session->get_project_state()->get_milestone(active_milestone_id);
	return !milestone.is_empty() && String(milestone.get("status", String())) != "locked";
}

void AINextTaskTree::_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!_can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}
	Dictionary drag_data = p_data;
	const String task_id = String(drag_data.get("task_id", String()));
	const int from_index = (int)drag_data.get("from_index", -1);
	const int slot = _get_drop_slot(p_point, p_from);
	const int to_index = from_index >= 0 && from_index < slot ? slot - 1 : slot;
	if (from_index == to_index) {
		return;
	}
	next_session->move_user_task(task_id, next_session->get_project_state()->get_active_milestone_id(), to_index);
	refresh();
}

int AINextTaskTree::_get_drop_slot(const Point2 &p_point, const Control *p_from) const {
	if (p_from && p_from->has_meta(SNAME("ai_next_index"))) {
		const int index = (int)p_from->get_meta(SNAME("ai_next_index"), 0);
		return index + (p_point.y > p_from->get_size().y * 0.5 ? 1 : 0);
	}

	int slot = 0;
	for (int i = 0; i < get_child_count(); i++) {
		Control *row = Object::cast_to<Control>(get_child(i));
		if (!row || !String(row->get_name()).begins_with("TaskRow")) {
			continue;
		}
		if (p_point.y < row->get_position().y + row->get_size().y * 0.5) {
			return slot;
		}
		slot++;
	}
	return slot;
}

bool AINextTaskTree::can_drop_data(const Point2 &p_point, const Variant &p_data) const {
	return _can_drop_data_fw(p_point, p_data, nullptr);
}

void AINextTaskTree::drop_data(const Point2 &p_point, const Variant &p_data) {
	_drop_data_fw(p_point, p_data, nullptr);
}

void AINextTaskTree::refresh() {
	_clear_rows();
	if (!next_session || next_session->get_project_state().is_null()) {
		return;
	}

	const bool can_edit = next_session->can_edit_plan();
	HBoxContainer *toolbar = memnew(HBoxContainer);
	toolbar->set_name(SNAME("TaskToolbar"));
	toolbar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(toolbar);

	Label *spacer = memnew(Label);
	spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	toolbar->add_child(spacer);

	Button *add = memnew(Button);
	add->set_name(SNAME("AIPlanAddTaskButton"));
	setup_ai_next_plan_icon_button(this, add, SNAME("AIPlanAdd"), TTR("Add task."));
	add->set_disabled(!can_edit);
	add->connect(SceneStringName(pressed), callable_mp(this, &AINextTaskTree::_add_task_pressed));
	toolbar->add_child(add);

	const String milestone_id = next_session->get_project_state()->get_active_milestone_id();
	Dictionary milestone = next_session->get_project_state()->get_milestone(milestone_id);
	Array tasks = milestone.get("tasks", Array());
	if (tasks.is_empty()) {
		Label *empty = memnew(Label);
		empty->set_text(TTR("No tasks"));
		empty->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
		add_child(empty);
		return;
	}

	for (int i = 0; i < tasks.size(); i++) {
		if (Variant(tasks[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary task = tasks[i];
		const String task_id = String(task.get("id", String()));
		Dictionary computed_task = next_session->get_project_state()->get_task(task_id);
		const String status_text = String(computed_task.get("status", task.get("status", "pending")));
		const bool selected = task_id == next_session->get_selected_task_id();
		const bool locked = String(milestone.get("status", String())) == "locked";

		AINextTaskRow::Callbacks callbacks;
		callbacks.select = callable_mp(this, &AINextTaskTree::_task_pressed).bind(task_id);
		callbacks.edit = callable_mp(this, &AINextTaskTree::_edit_task_pressed).bind(task_id);
		callbacks.dependencies = callable_mp(this, &AINextTaskTree::_dependencies_pressed).bind(task_id);
		callbacks.move_up = callable_mp(this, &AINextTaskTree::_move_task_pressed).bind(task_id, i - 1);
		callbacks.move_down = callable_mp(this, &AINextTaskTree::_move_task_pressed).bind(task_id, i + 1);
		callbacks.remove = callable_mp(this, &AINextTaskTree::_delete_task_pressed).bind(task_id);
		callbacks.run = callable_mp(this, &AINextTaskTree::_run_task_pressed).bind(task_id);
		callbacks.get_drag_data = callable_mp(this, &AINextTaskTree::_get_drag_data_fw);
		callbacks.can_drop_data = callable_mp(this, &AINextTaskTree::_can_drop_data_fw);
		callbacks.drop_data = callable_mp(this, &AINextTaskTree::_drop_data_fw);

		AINextTaskRow *row = memnew(AINextTaskRow);
		add_child(row, true);
		row->setup(task, i, tasks.size(), status_text, selected, locked, can_edit, next_session->is_workflow_active(), next_session->can_run_task(task_id), callbacks);
	}
}
