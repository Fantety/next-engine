/**************************************************************************/
/*  ai_skill_dialog.cpp                                                    */
/**************************************************************************/

#include "ai_skill_dialog.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/check_box.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/text_edit.h"
#include "servers/text/text_server.h"

namespace {

Label *_make_field_label(const String &p_text) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	label->set_custom_minimum_size(Size2(120, 0) * EDSCALE);
	label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	return label;
}

HBoxContainer *_make_field_row(VBoxContainer *p_root, const String &p_label, Control *p_editor) {
	HBoxContainer *row = memnew(HBoxContainer);
	row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row->add_theme_constant_override("separation", 8 * EDSCALE);
	p_root->add_child(row);

	row->add_child(_make_field_label(p_label));
	p_editor->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row->add_child(p_editor);
	return row;
}

} // namespace

void AISkillDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("skill_submitted"));
}

void AISkillDialog::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build_ui();
	}
}

AISkillDialog::AISkillDialog() {
	set_min_size(Size2(640, 420) * EDSCALE);
	set_ok_button_text(TTR("Save"));
	set_hide_on_ok(false);
}

void AISkillDialog::_build_ui() {
	if (name_edit) {
		return;
	}

	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_custom_minimum_size(Size2(0, 310) * EDSCALE);
	scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	add_child(scroll);

	VBoxContainer *root = memnew(VBoxContainer);
	root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	root->add_theme_constant_override("separation", 8 * EDSCALE);
	scroll->add_child(root);

	name_edit = memnew(LineEdit);
	name_edit->set_placeholder("Test Driven Development");
	_make_field_row(root, TTR("Name"), name_edit);

	description_edit = memnew(LineEdit);
	description_edit->set_placeholder("Use when implementing behavior changes.");
	_make_field_row(root, TTR("Description"), description_edit);

	Label *kind_value = memnew(Label);
	kind_value->set_text("prompt_context");
	kind_value->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	_make_field_row(root, TTR("Kind"), kind_value);

	Label *safety_label = memnew(Label);
	safety_label->set_text(TTR("Current skills only provide prompt/context instructions; they do not execute code, launch processes, or grant tools."));
	safety_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	safety_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	root->add_child(safety_label);

	content_edit = memnew(TextEdit);
	content_edit->set_custom_minimum_size(Size2(0, 170) * EDSCALE);
	content_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	content_edit->set_placeholder("# Skill Instructions\n\nDescribe when and how the agent should use this skill.");
	_make_field_row(root, TTR("Content"), content_edit);

	enabled_check = memnew(CheckBox);
	enabled_check->set_text(TTR("Enabled"));
	root->add_child(enabled_check);

	error_label = memnew(Label);
	error_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
	error_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	root->add_child(error_label);

	connect(SceneStringName(confirmed), callable_mp(this, &AISkillDialog::_confirmed), CONNECT_DEFERRED);
}

void AISkillDialog::_reset_form() {
	_build_ui();
	editing_skill_id.clear();
	name_edit->clear();
	description_edit->clear();
	content_edit->clear();
	enabled_check->set_pressed(true);
	_set_error(String());
}

void AISkillDialog::_confirmed() {
	AISkillConfig skill = get_submitted_skill();
	if (skill.display_name.is_empty()) {
		_set_error(TTR("Skill name is required."));
		return;
	}
	if (skill.content.is_empty()) {
		_set_error(TTR("Skill content is required."));
		return;
	}
	_set_error(String());
	emit_signal(SNAME("skill_submitted"));
}

void AISkillDialog::_set_error(const String &p_error) {
	if (error_label) {
		error_label->set_text(p_error);
	}
}

void AISkillDialog::popup_add_skill() {
	_build_ui();
	_reset_form();
	set_title(TTR("Add Agent Skill"));
	popup_centered(Size2(640, 420) * EDSCALE);
}

void AISkillDialog::popup_edit_skill(const AISkillConfig &p_skill) {
	_build_ui();
	_reset_form();
	editing_skill_id = p_skill.id;
	name_edit->set_text(p_skill.display_name);
	description_edit->set_text(p_skill.description);
	content_edit->set_text(p_skill.content);
	enabled_check->set_pressed(p_skill.enabled);
	set_title(TTR("Edit Agent Skill"));
	popup_centered(Size2(640, 420) * EDSCALE);
}

bool AISkillDialog::is_editing_skill() const {
	return !editing_skill_id.is_empty();
}

String AISkillDialog::get_editing_skill_id() const {
	return editing_skill_id;
}

AISkillConfig AISkillDialog::get_submitted_skill() const {
	AISkillConfig skill;
	if (!name_edit || !description_edit || !content_edit || !enabled_check) {
		return skill;
	}

	skill.id = editing_skill_id;
	skill.display_name = name_edit->get_text().strip_edges();
	skill.description = description_edit->get_text().strip_edges();
	skill.content = content_edit->get_text().strip_edges();
	skill.kind = "prompt_context";
	skill.enabled = enabled_check->is_pressed();
	return skill;
}
