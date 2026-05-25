/**************************************************************************/
/*  ai_plan_panel.cpp                                                      */
/**************************************************************************/

#include "ai_plan_panel.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/ai_component/planning/ai_plan_manager.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/label.h"
#include "servers/text/text_server.h"

void AIPlanPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("refresh_plan"), &AIPlanPanel::refresh_plan);
}

AIPlanPanel::AIPlanPanel() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hide();
}

void AIPlanPanel::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build_ui();
		Ref<AIPlanManager> manager = AIPlanManager::get_singleton();
		manager->connect("plan_changed", callable_mp(this, &AIPlanPanel::refresh_plan).unbind(1), CONNECT_DEFERRED);
		manager->connect("plan_archived", callable_mp(this, &AIPlanPanel::refresh_plan).unbind(1), CONNECT_DEFERRED);
		_refresh();
	} else if (p_what == NOTIFICATION_THEME_CHANGED) {
		_refresh();
	}
}

void AIPlanPanel::_build_ui() {
	if (task_list) {
		return;
	}

	VBoxContainer *root = memnew(VBoxContainer);
	root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	root->add_theme_constant_override("separation", 6 * EDSCALE);
	add_child(root);

	HBoxContainer *header = memnew(HBoxContainer);
	header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header->add_theme_constant_override("separation", 6 * EDSCALE);
	root->add_child(header);

	toggle_button = memnew(Button);
	toggle_button->set_flat(true);
	toggle_button->set_tooltip_text(TTR("Expand or collapse the active plan."));
	toggle_button->connect(SceneStringName(pressed), callable_mp(this, &AIPlanPanel::_toggle_pressed));
	header->add_child(toggle_button);

	title_label = memnew(Label);
	title_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	title_label->add_theme_font_size_override(SceneStringName(font_size), int(13 * EDSCALE));
	header->add_child(title_label);

	task_list = memnew(VBoxContainer);
	task_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	task_list->add_theme_constant_override("separation", 4 * EDSCALE);
	root->add_child(task_list);
}

void AIPlanPanel::_clear_tasks() {
	ERR_FAIL_NULL(task_list);
	while (task_list->get_child_count() > 0) {
		Node *child = task_list->get_child(0);
		task_list->remove_child(child);
		memdelete(child);
	}
}

String AIPlanPanel::_status_label(const String &p_status) const {
	if (p_status == "completed") {
		return TTR("Completed");
	}
	if (p_status == "in_progress") {
		return TTR("In Progress");
	}
	return TTR("Pending");
}

void AIPlanPanel::_refresh() {
	_build_ui();

	Ref<AIPlanManager> manager = AIPlanManager::get_singleton();
	const Dictionary plan = manager->get_active_plan();
	if (plan.is_empty()) {
		hide();
		_clear_tasks();
		return;
	}

	show();
	if (toggle_button) {
		toggle_button->set_button_icon(get_editor_theme_icon(expanded ? SNAME("GuiTreeArrowDown") : SNAME("GuiTreeArrowRight")));
	}

	Array tasks = plan.get("tasks", Array());
	int completed_count = 0;
	for (int i = 0; i < tasks.size(); i++) {
		if (Variant(tasks[i]).get_type() == Variant::DICTIONARY) {
			Dictionary task = tasks[i];
			if (String(task.get("status", "pending")) == "completed") {
				completed_count++;
			}
		}
	}

	title_label->set_text(vformat(TTR("%s  %d/%d"), String(plan.get("title", TTR("Plan"))), completed_count, tasks.size()));
	task_list->set_visible(expanded);
	_clear_tasks();
	if (!expanded) {
		return;
	}

	for (int i = 0; i < tasks.size(); i++) {
		if (Variant(tasks[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary task = tasks[i];
		const String status = String(task.get("status", "pending"));

		HBoxContainer *row = memnew(HBoxContainer);
		row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_theme_constant_override("separation", 6 * EDSCALE);
		task_list->add_child(row);

		ColorRect *dot = memnew(ColorRect);
		dot->set_custom_minimum_size(Size2(8, 8) * EDSCALE);
		if (status == "completed") {
			dot->set_color(get_theme_color(SNAME("success_color"), EditorStringName(Editor)));
		} else if (status == "in_progress") {
			dot->set_color(get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
		} else {
			dot->set_color(get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
		}
		row->add_child(dot);

		Label *task_label = memnew(Label);
		task_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		task_label->set_text(String(task.get("title", String())));
		task_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		task_label->set_tooltip_text(vformat(TTR("%s\nStatus: %s\nTask ID: %s"), String(task.get("title", String())), _status_label(status), String(task.get("id", String()))));
		row->add_child(task_label);

		Label *status_label = memnew(Label);
		status_label->set_text(_status_label(status));
		status_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
		row->add_child(status_label);
	}
}

void AIPlanPanel::_toggle_pressed() {
	expanded = !expanded;
	_refresh();
}

void AIPlanPanel::refresh_plan() {
	_refresh();
}
