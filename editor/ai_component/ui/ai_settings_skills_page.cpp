/**************************************************************************/
/*  ai_settings_skills_page.cpp                                            */
/**************************************************************************/

#include "ai_settings_skills_page.h"

#include "editor/ai_component/ui/ai_skill_dialog.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "servers/text/text_server.h"

namespace {

String _ai_ui_text(const char *p_text) {
	return String::utf8(p_text);
}

void _clear_children(Node *p_node) {
	ERR_FAIL_NULL(p_node);
	while (p_node->get_child_count() > 0) {
		Node *child = p_node->get_child(0);
		p_node->remove_child(child);
		memdelete(child);
	}
}

Label *_make_table_label(const String &p_text, int p_width = 0) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	label->set_clip_text(true);
	if (p_width > 0) {
		label->set_custom_minimum_size(Size2(p_width, 0) * EDSCALE);
	} else {
		label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	}
	return label;
}

} // namespace

void AISettingsSkillsPage::_bind_methods() {
	ADD_SIGNAL(MethodInfo("settings_changed"));
}

void AISettingsSkillsPage::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build_ui();
	}
}

AISettingsSkillsPage::AISettingsSkillsPage() {
}

void AISettingsSkillsPage::_build_ui() {
	if (skill_table) {
		return;
	}

	add_theme_constant_override("margin_left", 8 * EDSCALE);
	add_theme_constant_override("margin_right", 8 * EDSCALE);
	add_theme_constant_override("margin_top", 8 * EDSCALE);
	add_theme_constant_override("margin_bottom", 8 * EDSCALE);

	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(scroll);

	VBoxContainer *content = memnew(VBoxContainer);
	content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_theme_constant_override("separation", 12 * EDSCALE);
	scroll->add_child(content);

	Label *title = memnew(Label);
	title->set_text(TTR("Skills"));
	title->add_theme_font_size_override(SceneStringName(font_size), int(22 * EDSCALE));
	content->add_child(title);

	Label *section_title = memnew(Label);
	section_title->set_text(TTR("Prompt/Context Skills"));
	section_title->add_theme_font_size_override(SceneStringName(font_size), int(14 * EDSCALE));
	content->add_child(section_title);

	Label *description = memnew(Label);
	description->set_text(TTR("Current AgentSkills only provide prompt/context instructions. They are listed to the model by name and description, and the model can call the read-only agent.activate_skill tool to load full content. Skills do not execute code, launch processes, read bundled resources, or grant tool permissions."));
	description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	content->add_child(description);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	toolbar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	toolbar->add_theme_constant_override("separation", 8 * EDSCALE);
	content->add_child(toolbar);

	add_skill_button = memnew(Button);
	add_skill_button->set_text(TTR("Add Skill"));
	add_skill_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsSkillsPage::_popup_add_skill_dialog));
	toolbar->add_child(add_skill_button);

	import_skill_button = memnew(Button);
	import_skill_button->set_text(TTR("Import Skill Folder"));
	import_skill_button->set_tooltip_text(TTR("Select a local Skill folder. The editor will read only its SKILL.md as prompt/context instructions."));
	import_skill_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsSkillsPage::_popup_import_skill_dialog));
	toolbar->add_child(import_skill_button);

	status_label = memnew(Label);
	status_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	status_label->hide();
	content->add_child(status_label);

	PanelContainer *table_panel = memnew(PanelContainer);
	table_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	table_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(table_panel);

	skill_table = memnew(VBoxContainer);
	skill_table->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	skill_table->add_theme_constant_override("separation", 0);
	table_panel->add_child(skill_table);

	skill_dialog = memnew(AISkillDialog);
	skill_dialog->connect("skill_submitted", callable_mp(this, &AISettingsSkillsPage::_skill_submitted));
	add_child(skill_dialog);

	import_skill_dialog = memnew(EditorFileDialog);
	import_skill_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	import_skill_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_DIR);
	import_skill_dialog->set_title(TTR("Import Agent Skill Folder"));
	import_skill_dialog->connect("dir_selected", callable_mp(this, &AISettingsSkillsPage::_skill_folder_selected));
	add_child(import_skill_dialog);

	_refresh_skill_table();
}

void AISettingsSkillsPage::_refresh_skill_table() {
	ERR_FAIL_NULL(skill_table);

	_clear_children(skill_table);
	skill_rows.clear();

	HBoxContainer *header = memnew(HBoxContainer);
	header->set_custom_minimum_size(Size2(0, 32) * EDSCALE);
	header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header->add_theme_constant_override("separation", 8 * EDSCALE);
	skill_table->add_child(header);

	Label *enabled_header = _make_table_label(TTR("Enabled"), 82);
	enabled_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(enabled_header);

	Label *name_header = _make_table_label(TTR("Name"), 180);
	name_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(name_header);

	Label *kind_header = _make_table_label(TTR("Kind"), 140);
	kind_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(kind_header);

	Label *description_header = _make_table_label(TTR("Description"));
	description_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(description_header);

	Label *action_header = _make_table_label(TTR("Actions"), 150);
	action_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(action_header);

	HSeparator *header_separator = memnew(HSeparator);
	skill_table->add_child(header_separator);

	Vector<AISkillConfig> skills = AISkillSettings::get_skills(false);
	for (int i = 0; i < skills.size(); i++) {
		_add_skill_table_row(skills[i]);
	}

	if (skills.is_empty()) {
		MarginContainer *empty_margin = memnew(MarginContainer);
		empty_margin->add_theme_constant_override("margin_left", 28 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_bottom", 10 * EDSCALE);
		skill_table->add_child(empty_margin);

		Label *empty_label = memnew(Label);
		empty_label->set_text(TTR("No AgentSkills yet. Add a prompt/context skill to make it available to the AI Agent."));
		empty_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
		empty_margin->add_child(empty_label);
	}
}

void AISettingsSkillsPage::_add_skill_table_row(const AISkillConfig &p_skill) {
	skill_rows.push_back(p_skill);

	HBoxContainer *row = memnew(HBoxContainer);
	row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row->set_custom_minimum_size(Size2(0, 36) * EDSCALE);
	row->add_theme_constant_override("separation", 8 * EDSCALE);
	skill_table->add_child(row);

	CheckBox *enabled_check = memnew(CheckBox);
	enabled_check->set_pressed(p_skill.enabled);
	enabled_check->set_custom_minimum_size(Size2(82, 0) * EDSCALE);
	enabled_check->connect(SceneStringName(toggled), callable_mp(this, &AISettingsSkillsPage::_skill_enabled_toggled).bind(p_skill.id), CONNECT_DEFERRED);
	row->add_child(enabled_check);

	Label *name_label = _make_table_label(p_skill.display_name, 180);
	name_label->set_tooltip_text(p_skill.display_name);
	row->add_child(name_label);

	Label *kind_label = _make_table_label(p_skill.kind, 140);
	kind_label->set_tooltip_text(TTR("Only prompt_context skills are supported in the current version."));
	row->add_child(kind_label);

	Label *description_label = _make_table_label(p_skill.description);
	description_label->set_tooltip_text(p_skill.description);
	row->add_child(description_label);

	HBoxContainer *action_cell = memnew(HBoxContainer);
	action_cell->set_custom_minimum_size(Size2(150, 0) * EDSCALE);
	action_cell->add_theme_constant_override("separation", 6 * EDSCALE);
	row->add_child(action_cell);

	Button *edit_button = memnew(Button);
	edit_button->set_text(_ai_ui_text(u8"\u7f16\u8f91"));
	edit_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsSkillsPage::_edit_skill_pressed).bind(p_skill.id), CONNECT_DEFERRED);
	action_cell->add_child(edit_button);

	Button *remove_button = memnew(Button);
	remove_button->set_text(_ai_ui_text(u8"\u79fb\u9664"));
	remove_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsSkillsPage::_remove_skill_pressed).bind(p_skill.id), CONNECT_DEFERRED);
	action_cell->add_child(remove_button);

	HSeparator *row_separator = memnew(HSeparator);
	skill_table->add_child(row_separator);
}

void AISettingsSkillsPage::_popup_add_skill_dialog() {
	ERR_FAIL_NULL(skill_dialog);
	_set_status(String(), false);
	skill_dialog->popup_add_skill();
}

void AISettingsSkillsPage::_popup_import_skill_dialog() {
	ERR_FAIL_NULL(import_skill_dialog);
	_set_status(String(), false);
	import_skill_dialog->popup_centered_ratio();
}

void AISettingsSkillsPage::_skill_folder_selected(const String &p_folder_path) {
	String error;
	String skill_id;
	if (!AISkillSettings::import_skill_folder(p_folder_path, error, &skill_id)) {
		_set_status(error, true);
		return;
	}

	AISkillConfig skill = AISkillSettings::get_skill(skill_id);
	_refresh_skill_table();
	emit_signal(SNAME("settings_changed"));
	_set_status(vformat(TTR("Imported AgentSkill: %s"), skill.display_name), false);
}

void AISettingsSkillsPage::_skill_submitted() {
	ERR_FAIL_NULL(skill_dialog);

	AISkillConfig skill = skill_dialog->get_submitted_skill();
	if (skill.display_name.is_empty() || skill.content.is_empty()) {
		return;
	}

	if (skill_dialog->is_editing_skill()) {
		AISkillSettings::update_skill_config(skill);
	} else {
		AISkillSettings::add_skill_config(skill);
	}

	_refresh_skill_table();
	emit_signal(SNAME("settings_changed"));
	skill_dialog->hide();
	_set_status(String(), false);
}

void AISettingsSkillsPage::_edit_skill_pressed(const String &p_skill_id) {
	ERR_FAIL_NULL(skill_dialog);
	AISkillConfig skill = AISkillSettings::get_skill(p_skill_id);
	if (skill.id.is_empty()) {
		return;
	}
	skill_dialog->popup_edit_skill(skill);
}

void AISettingsSkillsPage::_remove_skill_pressed(const String &p_skill_id) {
	if (AISkillSettings::remove_skill(p_skill_id)) {
		_refresh_skill_table();
		emit_signal(SNAME("settings_changed"));
	}
}

void AISettingsSkillsPage::_skill_enabled_toggled(bool p_enabled, const String &p_skill_id) {
	if (AISkillSettings::set_skill_enabled(p_skill_id, p_enabled)) {
		_refresh_skill_table();
		emit_signal(SNAME("settings_changed"));
	}
}

void AISettingsSkillsPage::_set_status(const String &p_status, bool p_error) {
	if (!status_label) {
		return;
	}
	status_label->set_text(p_status);
	status_label->set_visible(!p_status.is_empty());
	status_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(p_error ? SNAME("error_color") : SNAME("disabled_font_color"), EditorStringName(Editor)));
}

void AISettingsSkillsPage::build_for_test() {
	_build_ui();
}

int AISettingsSkillsPage::get_skill_table_row_count_for_test() const {
	return skill_rows.size();
}

void AISettingsSkillsPage::add_skill_for_test(const String &p_display_name, const String &p_description, const String &p_content, bool p_enabled) {
	(void)AISkillSettings::add_skill(p_display_name, p_description, p_content, p_enabled);
	_refresh_skill_table();
}

void AISettingsSkillsPage::set_skill_enabled_for_test(const String &p_skill_id, bool p_enabled) {
	AISkillSettings::set_skill_enabled(p_skill_id, p_enabled);
	_refresh_skill_table();
}
