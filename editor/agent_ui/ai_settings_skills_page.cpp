/**************************************************************************/
/*  ai_settings_skills_page.cpp                                            */
/**************************************************************************/

#include "ai_settings_skills_page.h"

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

void _clear_children(Node *p_node) {
	ERR_FAIL_NULL(p_node);
	while (p_node->get_child_count() > 0) {
		Node *child = p_node->get_child(0);
		p_node->remove_child(child);
		memdelete(child);
	}
}

Array _array_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::ARRAY) {
		return Array(p_value).duplicate(true);
	}
	return Array();
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

String _skill_id(const Dictionary &p_skill) {
	return String(p_skill.get("id", p_skill.get("name", p_skill.get("source", String())))).strip_edges();
}

String _skill_kind(const Dictionary &p_skill) {
	String kind = String(p_skill.get("kind", String())).strip_edges();
	if (kind.is_empty()) {
		kind = p_skill.has("source") && !p_skill.has("entry") ? String("source") : String("manifest");
	}
	return kind;
}

String _skill_name(const Dictionary &p_skill) {
	const String kind = _skill_kind(p_skill);
	if (kind == "source") {
		return String(p_skill.get("source", p_skill.get("id", String()))).strip_edges();
	}
	const String name = String(p_skill.get("name", p_skill.get("display_name", p_skill.get("id", String())))).strip_edges();
	return name.is_empty() ? _skill_id(p_skill) : name;
}

String _skill_description(const Dictionary &p_skill) {
	if (_skill_kind(p_skill) == "source") {
		return TTR("Skill source root scanned by agent_v1.");
	}
	const String description = String(p_skill.get("description", String())).strip_edges();
	if (!description.is_empty()) {
		return description;
	}
	return String(p_skill.get("entry", String())).strip_edges();
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

Ref<AIAgentV1UIBridge> AISettingsSkillsPage::_get_adapter() const {
	return AIAgentV1UIBridge::get_singleton();
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
	section_title->set_text(TTR("Skill Sources"));
	section_title->add_theme_font_size_override(SceneStringName(font_size), int(14 * EDSCALE));
	content->add_child(section_title);

	Label *description = memnew(Label);
	description->set_text(TTR("agent_v1 loads skills from configured source folders and discovers their manifests. Add folders here; individual skill manifests are read by the backend and shown below."));
	description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	content->add_child(description);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	toolbar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	toolbar->add_theme_constant_override("separation", 8 * EDSCALE);
	content->add_child(toolbar);

	add_skill_button = memnew(Button);
	add_skill_button->set_text(TTR("Add Skill Source"));
	add_skill_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsSkillsPage::_popup_import_skill_dialog));
	toolbar->add_child(add_skill_button);

	import_skill_button = memnew(Button);
	import_skill_button->set_text(TTR("Refresh"));
	import_skill_button->set_tooltip_text(TTR("Refresh the skill list from the agent_v1 bridge snapshot."));
	import_skill_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsSkillsPage::_refresh_skill_table));
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

	import_skill_dialog = memnew(EditorFileDialog);
	import_skill_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	import_skill_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_DIR);
	import_skill_dialog->set_title(TTR("Add Agent Skill Source"));
	import_skill_dialog->connect("dir_selected", callable_mp(this, &AISettingsSkillsPage::_skill_folder_selected));
	add_child(import_skill_dialog);

	Ref<AIAgentV1UIBridge> bridge = _get_adapter();
	if (bridge.is_valid()) {
		const Callable skill_status_changed = callable_mp(this, &AISettingsSkillsPage::_skill_status_changed);
		if (!bridge->is_connected(SNAME("skill_status_changed"), skill_status_changed)) {
			bridge->connect(SNAME("skill_status_changed"), skill_status_changed);
		}
		const Callable config_changed = callable_mp(this, &AISettingsSkillsPage::_config_changed);
		if (!bridge->is_connected(SNAME("config_changed"), config_changed)) {
			bridge->connect(SNAME("config_changed"), config_changed);
		}
	}

	_refresh_skill_table();
}

Array AISettingsSkillsPage::_get_skill_rows() const {
	Ref<AIAgentV1UIBridge> bridge = _get_adapter();
	if (bridge.is_null()) {
		return Array();
	}
	const Dictionary snapshot = bridge->get_settings_snapshot();
	return _array_from_variant(snapshot.get("skills", Array()));
}

Array AISettingsSkillsPage::_get_skill_sources() const {
	Array sources;
	const Array rows = _get_skill_rows();
	for (int i = 0; i < rows.size(); i++) {
		if (rows[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary row = rows[i];
		if (_skill_kind(row) != "source") {
			continue;
		}
		const String source = String(row.get("source", row.get("id", String()))).strip_edges();
		if (!source.is_empty()) {
			sources.push_back(source);
		}
	}
	return sources;
}

bool AISettingsSkillsPage::_patch_skill_sources(const Array &p_sources, const String &p_scope) {
	Ref<AIAgentV1UIBridge> bridge = _get_adapter();
	if (bridge.is_null()) {
		return false;
	}

	Dictionary skills_patch;
	skills_patch["sources"] = p_sources.duplicate(true);
	Dictionary patch;
	patch["skills"] = skills_patch;
	const Dictionary result = bridge->patch_settings(patch, p_scope);
	return bool(result.get("success", false));
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

	Label *enabled_header = _make_table_label(TTR("Active"), 82);
	enabled_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(enabled_header);

	Label *name_header = _make_table_label(TTR("Name / Source"), 240);
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

	const Array skills = _get_skill_rows();
	for (int i = 0; i < skills.size(); i++) {
		if (skills[i].get_type() == Variant::DICTIONARY) {
			_add_skill_table_row(skills[i]);
		}
	}

	if (skill_rows.is_empty()) {
		MarginContainer *empty_margin = memnew(MarginContainer);
		empty_margin->add_theme_constant_override("margin_left", 28 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_bottom", 10 * EDSCALE);
		skill_table->add_child(empty_margin);

		Label *empty_label = memnew(Label);
		empty_label->set_text(TTR("No skill sources yet. Add a folder to let agent_v1 discover skills."));
		empty_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
		empty_margin->add_child(empty_label);
	}
}

void AISettingsSkillsPage::_add_skill_table_row(const Dictionary &p_skill) {
	skill_rows.push_back(p_skill.duplicate(true));
	const String skill_id = _skill_id(p_skill);
	const String kind = _skill_kind(p_skill);
	const bool source_row = kind == "source";

	HBoxContainer *row = memnew(HBoxContainer);
	row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row->set_custom_minimum_size(Size2(0, 36) * EDSCALE);
	row->add_theme_constant_override("separation", 8 * EDSCALE);
	skill_table->add_child(row);

	CheckBox *enabled_check = memnew(CheckBox);
	enabled_check->set_pressed(bool(p_skill.get("enabled", true)));
	enabled_check->set_disabled(true);
	enabled_check->set_custom_minimum_size(Size2(82, 0) * EDSCALE);
	enabled_check->connect(SceneStringName(toggled), callable_mp(this, &AISettingsSkillsPage::_skill_enabled_toggled).bind(skill_id), CONNECT_DEFERRED);
	row->add_child(enabled_check);

	const String name = _skill_name(p_skill);
	Label *name_label = _make_table_label(name, 240);
	name_label->set_tooltip_text(name);
	row->add_child(name_label);

	Label *kind_label = _make_table_label(kind, 140);
	kind_label->set_tooltip_text(kind);
	row->add_child(kind_label);

	const String description = _skill_description(p_skill);
	Label *description_label = _make_table_label(description);
	description_label->set_tooltip_text(description);
	row->add_child(description_label);

	HBoxContainer *action_cell = memnew(HBoxContainer);
	action_cell->set_custom_minimum_size(Size2(150, 0) * EDSCALE);
	action_cell->add_theme_constant_override("separation", 6 * EDSCALE);
	row->add_child(action_cell);

	if (source_row) {
		Button *remove_button = memnew(Button);
		remove_button->set_text(TTR("Remove"));
		remove_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsSkillsPage::_remove_skill_pressed).bind(skill_id), CONNECT_DEFERRED);
		action_cell->add_child(remove_button);
	} else {
		Label *managed_label = _make_table_label(TTR("Managed"), 90);
		managed_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
		action_cell->add_child(managed_label);
	}

	HSeparator *row_separator = memnew(HSeparator);
	skill_table->add_child(row_separator);
}

void AISettingsSkillsPage::_popup_import_skill_dialog() {
	ERR_FAIL_NULL(import_skill_dialog);
	_set_status(String(), false);
	import_skill_dialog->popup_centered_ratio();
}

void AISettingsSkillsPage::_skill_folder_selected(const String &p_folder_path) {
	const String source = p_folder_path.strip_edges();
	if (source.is_empty()) {
		_set_status(TTR("Skill source path cannot be empty."), true);
		return;
	}

	Array sources = _get_skill_sources();
	for (int i = 0; i < sources.size(); i++) {
		if (String(sources[i]) == source) {
			_set_status(TTR("Skill source already exists."), true);
			return;
		}
	}
	sources.push_back(source);
	if (!_patch_skill_sources(sources)) {
		_set_status(TTR("Could not add skill source."), true);
		return;
	}

	_refresh_skill_table();
	emit_signal(SNAME("settings_changed"));
	_set_status(vformat(TTR("Added skill source: %s"), source), false);
}

void AISettingsSkillsPage::_remove_skill_pressed(const String &p_skill_id) {
	Array sources = _get_skill_sources();
	bool removed = false;
	for (int i = sources.size() - 1; i >= 0; i--) {
		if (String(sources[i]) == p_skill_id) {
			sources.remove_at(i);
			removed = true;
		}
	}
	if (!removed) {
		return;
	}
	if (_patch_skill_sources(sources)) {
		_refresh_skill_table();
		emit_signal(SNAME("settings_changed"));
	}
}

void AISettingsSkillsPage::_skill_enabled_toggled(bool p_enabled, const String &p_skill_id) {
	(void)p_enabled;
	(void)p_skill_id;
}

void AISettingsSkillsPage::_skill_status_changed(const Array &p_statuses, const Dictionary &p_summary) {
	(void)p_statuses;
	(void)p_summary;
	_refresh_skill_table();
}

void AISettingsSkillsPage::_config_changed(const String &p_scope, const Dictionary &p_config) {
	(void)p_scope;
	(void)p_config;
	_refresh_skill_table();
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
	(void)p_display_name;
	(void)p_content;
	(void)p_enabled;
	const String source = p_description.strip_edges();
	if (source.is_empty()) {
		return;
	}
	Array sources = _get_skill_sources();
	bool exists = false;
	for (int i = 0; i < sources.size(); i++) {
		exists = exists || String(sources[i]) == source;
	}
	if (!exists) {
		sources.push_back(source);
		(void)_patch_skill_sources(sources, "runtime");
	}
	_refresh_skill_table();
}

void AISettingsSkillsPage::set_skill_enabled_for_test(const String &p_skill_id, bool p_enabled) {
	(void)p_skill_id;
	(void)p_enabled;
	_refresh_skill_table();
}
