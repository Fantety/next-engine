/**************************************************************************/
/*  ai_settings_mcp_page.cpp                                              */
/**************************************************************************/

#include "ai_settings_mcp_page.h"

#include "editor/ai_component/ui/ai_mcp_server_dialog.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/editor_string_names.h"
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
	if (p_width > 0) {
		label->set_custom_minimum_size(Size2(p_width, 0) * EDSCALE);
	} else {
		label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	}
	return label;
}

} // namespace

void AISettingsMCPPage::_bind_methods() {
	ADD_SIGNAL(MethodInfo("settings_changed"));
}

void AISettingsMCPPage::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build_ui();
	}
}

AISettingsMCPPage::AISettingsMCPPage() {
}

void AISettingsMCPPage::_build_ui() {
	if (server_table) {
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
	title->set_text(TTR("MCP"));
	title->add_theme_font_size_override(SceneStringName(font_size), int(22 * EDSCALE));
	content->add_child(title);

	Label *section_title = memnew(Label);
	section_title->set_text(TTR("MCP Servers"));
	section_title->add_theme_font_size_override(SceneStringName(font_size), int(14 * EDSCALE));
	content->add_child(section_title);

	Label *description = memnew(Label);
	description->set_text(_ai_ui_text(u8"\u6dfb\u52a0 MCP Server \u914d\u7f6e\uff0c\u5e76\u9009\u62e9\u54ea\u4e9b Server \u53ef\u7528\u3002\u5df2\u542f\u7528\u7684 Server \u4f1a\u5728\u5bf9\u8bdd\u8fd0\u884c\u65f6\u53d1\u73b0\u5de5\u5177\uff0c\u5e76\u4ee5\u9700\u8981\u786e\u8ba4\u7684\u5916\u90e8\u5de5\u5177\u63d0\u4f9b\u7ed9 Agent\u3002"));
	description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	content->add_child(description);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	toolbar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(toolbar);

	add_server_button = memnew(Button);
	add_server_button->set_text(_ai_ui_text(u8"+ \u6dfb\u52a0 MCP Server"));
	add_server_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsMCPPage::_popup_add_server_dialog));
	toolbar->add_child(add_server_button);

	PanelContainer *table_panel = memnew(PanelContainer);
	table_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	table_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(table_panel);

	server_table = memnew(VBoxContainer);
	server_table->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	server_table->add_theme_constant_override("separation", 0);
	table_panel->add_child(server_table);

	server_dialog = memnew(AIMCPServerDialog);
	server_dialog->connect("server_submitted", callable_mp(this, &AISettingsMCPPage::_server_submitted));
	add_child(server_dialog);

	_refresh_server_table();
}

void AISettingsMCPPage::_refresh_server_table() {
	ERR_FAIL_NULL(server_table);

	_clear_children(server_table);
	server_rows.clear();

	HBoxContainer *header = memnew(HBoxContainer);
	header->set_custom_minimum_size(Size2(0, 32) * EDSCALE);
	header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header->add_theme_constant_override("separation", 8 * EDSCALE);
	server_table->add_child(header);

	Label *enabled_header = _make_table_label(_ai_ui_text(u8"\u542f\u7528"), 70);
	enabled_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(enabled_header);

	Label *name_header = _make_table_label(_ai_ui_text(u8"\u540d\u79f0"));
	name_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(name_header);

	Label *command_header = _make_table_label(TTR("Command"), 220);
	command_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(command_header);

	Label *action_header = _make_table_label(_ai_ui_text(u8"\u64cd\u4f5c"), 150);
	action_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(action_header);

	HSeparator *header_separator = memnew(HSeparator);
	server_table->add_child(header_separator);

	Vector<AIMCPServerConfig> servers = AIMCPSettings::get_servers(false);
	for (int i = 0; i < servers.size(); i++) {
		_add_server_table_row(servers[i]);
	}

	if (servers.is_empty()) {
		MarginContainer *empty_margin = memnew(MarginContainer);
		empty_margin->add_theme_constant_override("margin_left", 28 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_bottom", 10 * EDSCALE);
		server_table->add_child(empty_margin);

		Label *empty_label = memnew(Label);
		empty_label->set_text(_ai_ui_text(u8"\u6682\u65e0 MCP Server\uff0c\u4f60\u53ef\u4ee5\u70b9\u51fb\u6dfb\u52a0 MCP Server"));
		empty_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
		empty_margin->add_child(empty_label);
	}
}

void AISettingsMCPPage::_add_server_table_row(const AIMCPServerConfig &p_server) {
	server_rows.push_back(p_server);

	HBoxContainer *row = memnew(HBoxContainer);
	row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row->set_custom_minimum_size(Size2(0, 36) * EDSCALE);
	row->add_theme_constant_override("separation", 8 * EDSCALE);
	server_table->add_child(row);

	CheckBox *enabled_check = memnew(CheckBox);
	enabled_check->set_pressed(p_server.enabled);
	enabled_check->set_custom_minimum_size(Size2(70, 0) * EDSCALE);
	enabled_check->connect(SceneStringName(toggled), callable_mp(this, &AISettingsMCPPage::_server_enabled_toggled).bind(p_server.id), CONNECT_DEFERRED);
	row->add_child(enabled_check);

	Label *name_label = _make_table_label(p_server.display_name);
	name_label->set_tooltip_text(p_server.display_name);
	row->add_child(name_label);

	Label *command_label = _make_table_label(p_server.command, 220);
	command_label->set_tooltip_text(p_server.command + (p_server.arguments.is_empty() ? String() : " " + p_server.arguments));
	row->add_child(command_label);

	HBoxContainer *action_cell = memnew(HBoxContainer);
	action_cell->set_custom_minimum_size(Size2(150, 0) * EDSCALE);
	action_cell->add_theme_constant_override("separation", 6 * EDSCALE);
	row->add_child(action_cell);

	Button *edit_button = memnew(Button);
	edit_button->set_text(_ai_ui_text(u8"\u7f16\u8f91"));
	edit_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsMCPPage::_edit_server_pressed).bind(p_server.id), CONNECT_DEFERRED);
	action_cell->add_child(edit_button);

	Button *remove_button = memnew(Button);
	remove_button->set_text(_ai_ui_text(u8"\u79fb\u9664"));
	remove_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsMCPPage::_remove_server_pressed).bind(p_server.id), CONNECT_DEFERRED);
	action_cell->add_child(remove_button);

	HSeparator *row_separator = memnew(HSeparator);
	server_table->add_child(row_separator);
}

void AISettingsMCPPage::_popup_add_server_dialog() {
	ERR_FAIL_NULL(server_dialog);
	server_dialog->popup_add_server();
}

void AISettingsMCPPage::_server_submitted() {
	ERR_FAIL_NULL(server_dialog);

	AIMCPServerConfig server = server_dialog->get_submitted_server();
	if (server.display_name.is_empty() || server.command.is_empty()) {
		return;
	}

	if (server_dialog->is_editing_server()) {
		AIMCPSettings::update_server(server_dialog->get_editing_server_id(), server.display_name, server.command, server.arguments, server.working_directory, server.environment, server.enabled);
	} else {
		(void)AIMCPSettings::add_server(server.display_name, server.command, server.arguments, server.working_directory, server.environment, server.enabled);
	}

	_refresh_server_table();
	emit_signal(SNAME("settings_changed"));
	server_dialog->hide();
}

void AISettingsMCPPage::_edit_server_pressed(const String &p_server_id) {
	ERR_FAIL_NULL(server_dialog);
	AIMCPServerConfig server = AIMCPSettings::get_server(p_server_id);
	if (server.id.is_empty()) {
		return;
	}
	server_dialog->popup_edit_server(server);
}

void AISettingsMCPPage::_remove_server_pressed(const String &p_server_id) {
	AIMCPSettings::remove_server(p_server_id);
	_refresh_server_table();
	emit_signal(SNAME("settings_changed"));
}

void AISettingsMCPPage::_server_enabled_toggled(bool p_enabled, const String &p_server_id) {
	if (AIMCPSettings::set_server_enabled(p_server_id, p_enabled)) {
		_refresh_server_table();
		emit_signal(SNAME("settings_changed"));
	}
}

void AISettingsMCPPage::build_for_test() {
	_build_ui();
}

int AISettingsMCPPage::get_server_table_row_count_for_test() const {
	return server_rows.size();
}

void AISettingsMCPPage::add_server_for_test(const String &p_display_name, const String &p_command, bool p_enabled) {
	(void)AIMCPSettings::add_server(p_display_name, p_command, String(), String(), String(), p_enabled);
	_refresh_server_table();
}

void AISettingsMCPPage::set_server_enabled_for_test(const String &p_server_id, bool p_enabled) {
	AIMCPSettings::set_server_enabled(p_server_id, p_enabled);
	_refresh_server_table();
}
