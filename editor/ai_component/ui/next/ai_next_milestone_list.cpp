/**************************************************************************/
/*  ai_next_milestone_list.cpp                                            */
/**************************************************************************/

#include "ai_next_milestone_list.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/ai_component/ui/next/ai_next_plan_rows.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/text_edit.h"
#include "servers/text/text_server.h"

void AINextMilestoneList::_bind_methods() {
	ClassDB::bind_method(D_METHOD("refresh"), &AINextMilestoneList::refresh);
}

AINextMilestoneList::AINextMilestoneList() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 4 * EDSCALE);
	_build_dialogs();
}

void AINextMilestoneList::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
		refresh();
	}
}

void AINextMilestoneList::_clear_rows() {
	for (int i = get_child_count() - 1; i >= 0; i--) {
		Node *child = get_child(i);
		if (child == milestone_dialog || child == delete_dialog || child == merge_dialog) {
			continue;
		}
		remove_child(child);
		memdelete(child);
	}
}

void AINextMilestoneList::set_next_session(AIAgentNextSession *p_session) {
	next_session = p_session;
	refresh();
}

void AINextMilestoneList::_build_dialogs() {
	if (milestone_dialog) {
		return;
	}

	milestone_dialog = memnew(ConfirmationDialog);
	milestone_dialog->set_min_size(Size2(420, 260) * EDSCALE);
	milestone_dialog->set_ok_button_text(TTR("Save"));
	milestone_dialog->set_hide_on_ok(true);
	add_child(milestone_dialog);

	VBoxContainer *milestone_form = memnew(VBoxContainer);
	milestone_form->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	milestone_form->add_theme_constant_override("separation", 6 * EDSCALE);
	milestone_dialog->add_child(milestone_form);

	milestone_title_edit = memnew(LineEdit);
	milestone_title_edit->set_placeholder(TTR("Milestone title"));
	milestone_title_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	milestone_form->add_child(milestone_title_edit);
	milestone_dialog->register_text_enter(milestone_title_edit);

	milestone_description_edit = memnew(TextEdit);
	milestone_description_edit->set_custom_minimum_size(Size2(0, 110) * EDSCALE);
	milestone_description_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	milestone_description_edit->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	milestone_description_edit->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	milestone_description_edit->set_placeholder(TTR("Description"));
	milestone_form->add_child(milestone_description_edit);
	milestone_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AINextMilestoneList::_confirm_milestone_dialog));

	delete_dialog = memnew(ConfirmationDialog);
	delete_dialog->set_title(TTR("Delete Milestone"));
	delete_dialog->set_text(TTR("Delete this NEXT milestone and its tasks?"));
	delete_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AINextMilestoneList::_confirm_delete_milestone));
	add_child(delete_dialog);

	merge_dialog = memnew(ConfirmationDialog);
	merge_dialog->set_title(TTR("Merge Milestone"));
	merge_dialog->set_min_size(Size2(360, 120) * EDSCALE);
	add_child(merge_dialog);

	VBoxContainer *merge_form = memnew(VBoxContainer);
	merge_form->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	merge_form->add_theme_constant_override("separation", 6 * EDSCALE);
	merge_dialog->add_child(merge_form);

	Label *merge_label = memnew(Label);
	merge_label->set_text(TTR("Move this milestone's tasks into:"));
	merge_form->add_child(merge_label);

	merge_target_selector = memnew(OptionButton);
	merge_target_selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	merge_form->add_child(merge_target_selector);
	merge_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AINextMilestoneList::_confirm_merge_milestone));
}

void AINextMilestoneList::_milestone_pressed(const String &p_milestone_id) {
	if (!next_session) {
		return;
	}
	next_session->select_milestone(p_milestone_id);
}

void AINextMilestoneList::_add_milestone_pressed() {
	if (!next_session || !next_session->can_edit_plan()) {
		return;
	}
	editing_milestone_id.clear();
	milestone_title_edit->clear();
	milestone_description_edit->clear();
	milestone_dialog->set_title(TTR("Add Milestone"));
	milestone_dialog->popup_centered(Size2(420, 260) * EDSCALE);
}

void AINextMilestoneList::_edit_milestone_pressed(const String &p_milestone_id) {
	if (!next_session || !next_session->can_edit_plan()) {
		return;
	}
	Dictionary milestone = next_session->get_project_state()->get_milestone(p_milestone_id);
	if (milestone.is_empty() || String(milestone.get("status", String())) == "locked") {
		return;
	}
	editing_milestone_id = p_milestone_id;
	milestone_title_edit->set_text(String(milestone.get("title", String())));
	milestone_description_edit->set_text(String(milestone.get("description", String())));
	milestone_dialog->set_title(TTR("Edit Milestone"));
	milestone_dialog->popup_centered(Size2(420, 260) * EDSCALE);
}

void AINextMilestoneList::_confirm_milestone_dialog() {
	if (!next_session) {
		return;
	}
	const String title = milestone_title_edit ? milestone_title_edit->get_text().strip_edges() : String();
	const String description = milestone_description_edit ? milestone_description_edit->get_text() : String();
	if (title.is_empty()) {
		return;
	}
	if (editing_milestone_id.is_empty()) {
		(void)next_session->create_user_milestone(title, description);
	} else {
		next_session->edit_user_milestone(editing_milestone_id, title, description);
	}
	refresh();
}

void AINextMilestoneList::_delete_milestone_pressed(const String &p_milestone_id) {
	if (!next_session || !next_session->can_edit_plan()) {
		return;
	}
	pending_delete_milestone_id = p_milestone_id;
	delete_dialog->popup_centered(Size2(360, 100) * EDSCALE);
}

void AINextMilestoneList::_confirm_delete_milestone() {
	if (next_session && !pending_delete_milestone_id.is_empty()) {
		next_session->delete_user_milestone(pending_delete_milestone_id);
	}
	pending_delete_milestone_id.clear();
	refresh();
}

void AINextMilestoneList::_move_milestone_pressed(const String &p_milestone_id, int p_to_index) {
	if (!next_session || !next_session->can_edit_plan()) {
		return;
	}
	next_session->move_user_milestone(p_milestone_id, p_to_index);
	refresh();
}

void AINextMilestoneList::_merge_milestone_pressed(const String &p_milestone_id) {
	if (!next_session || !next_session->can_edit_plan() || next_session->get_project_state().is_null()) {
		return;
	}

	pending_merge_source_milestone_id = p_milestone_id;
	merge_target_selector->clear();
	Array milestones = next_session->get_project_state()->get_milestones_as_array();
	for (int i = 0; i < milestones.size(); i++) {
		if (Variant(milestones[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary milestone = milestones[i];
		const String milestone_id = String(milestone.get("id", String()));
		if (milestone_id == p_milestone_id || String(milestone.get("status", String())) == "locked") {
			continue;
		}
		const int index = merge_target_selector->get_item_count();
		merge_target_selector->add_item(String(milestone.get("title", TTR("Milestone"))));
		merge_target_selector->set_item_metadata(index, milestone_id);
	}
	if (merge_target_selector->get_item_count() == 0) {
		return;
	}
	merge_dialog->popup_centered(Size2(360, 120) * EDSCALE);
}

void AINextMilestoneList::_confirm_merge_milestone() {
	if (!next_session || !merge_target_selector || pending_merge_source_milestone_id.is_empty()) {
		return;
	}
	const int selected = merge_target_selector->get_selected();
	if (selected >= 0) {
		next_session->merge_user_milestones(String(merge_target_selector->get_item_metadata(selected)), pending_merge_source_milestone_id);
	}
	pending_merge_source_milestone_id.clear();
	refresh();
}

Variant AINextMilestoneList::_get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	if (!next_session || !next_session->can_edit_plan() || !p_from || next_session->get_project_state().is_null()) {
		return Variant();
	}
	const String milestone_id = String(p_from->get_meta(SNAME("ai_next_milestone_id"), String()));
	const int from_index = (int)p_from->get_meta(SNAME("ai_next_index"), -1);
	Dictionary milestone = next_session->get_project_state()->get_milestone(milestone_id);
	if (milestone.is_empty() || String(milestone.get("status", String())) == "locked" || from_index < 0) {
		return Variant();
	}

	Dictionary drag_data;
	drag_data["type"] = "ai_next_milestone";
	drag_data["milestone_id"] = milestone_id;
	drag_data["from_index"] = from_index;
	p_from->set_drag_preview(make_ai_next_plan_drag_preview(String(milestone.get("title", TTR("Milestone")))));
	return drag_data;
}

bool AINextMilestoneList::_can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	if (!next_session || !next_session->can_edit_plan() || next_session->get_project_state().is_null() || p_data.get_type() != Variant::DICTIONARY) {
		return false;
	}
	Dictionary drag_data = p_data;
	if (String(drag_data.get("type", String())) != "ai_next_milestone") {
		return false;
	}
	Dictionary milestone = next_session->get_project_state()->get_milestone(String(drag_data.get("milestone_id", String())));
	return !milestone.is_empty() && String(milestone.get("status", String())) != "locked";
}

void AINextMilestoneList::_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!_can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}
	Dictionary drag_data = p_data;
	const String milestone_id = String(drag_data.get("milestone_id", String()));
	const int from_index = (int)drag_data.get("from_index", -1);
	const int slot = _get_drop_slot(p_point, p_from);
	const int to_index = from_index >= 0 && from_index < slot ? slot - 1 : slot;
	if (from_index == to_index) {
		return;
	}
	next_session->move_user_milestone(milestone_id, to_index);
	refresh();
}

int AINextMilestoneList::_get_drop_slot(const Point2 &p_point, const Control *p_from) const {
	if (p_from && p_from->has_meta(SNAME("ai_next_index"))) {
		const int index = (int)p_from->get_meta(SNAME("ai_next_index"), 0);
		return index + (p_point.y > p_from->get_size().y * 0.5 ? 1 : 0);
	}

	int slot = 0;
	for (int i = 0; i < get_child_count(); i++) {
		Control *row = Object::cast_to<Control>(get_child(i));
		if (!row || !String(row->get_name()).begins_with("MilestoneRow")) {
			continue;
		}
		if (p_point.y < row->get_position().y + row->get_size().y * 0.5) {
			return slot;
		}
		slot++;
	}
	return slot;
}

bool AINextMilestoneList::can_drop_data(const Point2 &p_point, const Variant &p_data) const {
	return _can_drop_data_fw(p_point, p_data, nullptr);
}

void AINextMilestoneList::drop_data(const Point2 &p_point, const Variant &p_data) {
	_drop_data_fw(p_point, p_data, nullptr);
}

void AINextMilestoneList::refresh() {
	_clear_rows();
	if (!next_session || next_session->get_project_state().is_null()) {
		return;
	}

	const bool can_edit = next_session->can_edit_plan();
	HBoxContainer *toolbar = memnew(HBoxContainer);
	toolbar->set_name(SNAME("MilestoneToolbar"));
	toolbar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(toolbar);

	Label *spacer = memnew(Label);
	spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	toolbar->add_child(spacer);

	Button *add = memnew(Button);
	add->set_name(SNAME("AIPlanAddMilestoneButton"));
	setup_ai_next_plan_icon_button(this, add, SNAME("AIPlanAdd"), TTR("Add milestone."));
	add->set_disabled(!can_edit);
	add->connect(SceneStringName(pressed), callable_mp(this, &AINextMilestoneList::_add_milestone_pressed));
	toolbar->add_child(add);

	const String active_milestone_id = next_session->get_project_state()->get_active_milestone_id();
	Array milestones = next_session->get_project_state()->get_milestones_as_array();
	if (milestones.is_empty()) {
		Label *empty = memnew(Label);
		empty->set_text(TTR("No milestones"));
		empty->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
		add_child(empty);
		return;
	}

	for (int i = 0; i < milestones.size(); i++) {
		if (Variant(milestones[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary milestone = milestones[i];
		const String milestone_id = String(milestone.get("id", String()));
		const bool active = milestone_id == active_milestone_id;
		AINextMilestoneRow::Callbacks callbacks;
		callbacks.select = callable_mp(this, &AINextMilestoneList::_milestone_pressed).bind(milestone_id);
		callbacks.edit = callable_mp(this, &AINextMilestoneList::_edit_milestone_pressed).bind(milestone_id);
		callbacks.merge = callable_mp(this, &AINextMilestoneList::_merge_milestone_pressed).bind(milestone_id);
		callbacks.move_up = callable_mp(this, &AINextMilestoneList::_move_milestone_pressed).bind(milestone_id, i - 1);
		callbacks.move_down = callable_mp(this, &AINextMilestoneList::_move_milestone_pressed).bind(milestone_id, i + 1);
		callbacks.remove = callable_mp(this, &AINextMilestoneList::_delete_milestone_pressed).bind(milestone_id);
		callbacks.get_drag_data = callable_mp(this, &AINextMilestoneList::_get_drag_data_fw);
		callbacks.can_drop_data = callable_mp(this, &AINextMilestoneList::_can_drop_data_fw);
		callbacks.drop_data = callable_mp(this, &AINextMilestoneList::_drop_data_fw);

		AINextMilestoneRow *row = memnew(AINextMilestoneRow);
		add_child(row, true);
		row->setup(milestone, i, milestones.size(), active, can_edit, next_session->is_workflow_active(), callbacks);
	}
}
