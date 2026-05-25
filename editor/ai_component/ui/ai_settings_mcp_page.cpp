/**************************************************************************/
/*  ai_settings_mcp_page.cpp                                              */
/**************************************************************************/

#include "ai_settings_mcp_page.h"

#include "editor/ai_component/agent/ai_mcp_service.h"
#include "editor/ai_component/ui/ai_mcp_server_dialog.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/text_edit.h"
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

Color _mcp_status_color(const String &p_status, bool p_enabled) {
	if (!p_enabled || p_status == "disabled") {
		return Color(0.45, 0.45, 0.45, 1.0);
	}
	if (p_status == "ok") {
		return Color(0.25, 0.78, 0.42, 1.0);
	}
	if (p_status == "failed") {
		return Color(0.95, 0.26, 0.22, 1.0);
	}
	return Color(0.95, 0.69, 0.18, 1.0);
}

String _mcp_status_tooltip(const Dictionary &p_status, bool p_enabled) {
	if (!p_enabled) {
		return TTR("MCP server is disabled.");
	}
	const String state = String(p_status.get("status", "checking"));
	if (state == "ok") {
		return vformat(TTR("MCP server is available. Tools: %d."), (int)p_status.get("tool_count", 0));
	}
	if (state == "failed") {
		const String error = String(p_status.get("error", String()));
		return error.is_empty() ? TTR("MCP server is unavailable.") : error;
	}
	return TTR("MCP server is being checked.");
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
	description->set_text(TTR("Add MCP server configurations and choose which servers are available. Enabled servers discover tools at runtime and expose them to the Agent as external tools that require confirmation."));
	description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	content->add_child(description);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	toolbar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(toolbar);

	add_server_button = memnew(Button);
	add_server_button->set_text(TTR("Add MCP Server"));
	add_server_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsMCPPage::_popup_add_server_dialog));
	toolbar->add_child(add_server_button);

	import_json_button = memnew(Button);
	import_json_button->set_text(TTR("Import JSON"));
	import_json_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsMCPPage::_popup_import_json_dialog));
	toolbar->add_child(import_json_button);

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

	json_import_dialog = memnew(AcceptDialog);
	json_import_dialog->set_title(TTR("Import MCP Servers JSON"));
	json_import_dialog->set_min_size(Size2(720, 500) * EDSCALE);
	json_import_dialog->set_ok_button_text(TTR("Import"));
	json_import_dialog->set_hide_on_ok(false);
	json_import_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AISettingsMCPPage::_json_import_confirmed), CONNECT_DEFERRED);
	add_child(json_import_dialog);

	json_import_edit = memnew(TextEdit);
	json_import_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	json_import_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	json_import_edit->set_placeholder("{\n  \"mcpServers\": {\n    \"filesystem\": { \"command\": \"npx\", \"args\": [\"-y\", \"@modelcontextprotocol/server-filesystem\", \".\"] },\n    \"remote\": { \"type\": \"streamable_http\", \"url\": \"https://example.com/mcp\" }\n  }\n}");
	json_import_dialog->add_child(json_import_edit);

	error_dialog = memnew(AcceptDialog);
	error_dialog->set_title(TTR("MCP Import Error"));
	error_dialog->set_ok_button_text(TTR("Close"));
	add_child(error_dialog);

	Ref<AIMCPService> mcp_service = AIMCPService::get_singleton();
	if (mcp_service.is_valid()) {
		mcp_service->connect("status_changed", callable_mp(this, &AISettingsMCPPage::_mcp_status_changed), CONNECT_DEFERRED);
	}

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

	Label *status_header = _make_table_label(TTR("Status"), 62);
	status_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(status_header);

	Label *enabled_header = _make_table_label(TTR("Enabled"), 70);
	enabled_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(enabled_header);

	Label *name_header = _make_table_label(TTR("Name"));
	name_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(name_header);

	Label *transport_header = _make_table_label(TTR("Transport"), 130);
	transport_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(transport_header);

	Label *command_header = _make_table_label(TTR("Endpoint"), 220);
	command_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(command_header);

	Label *action_header = _make_table_label(TTR("Actions"), 150);
	action_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(action_header);

	HSeparator *header_separator = memnew(HSeparator);
	server_table->add_child(header_separator);

	Vector<AIMCPServerConfig> servers = AIMCPSettings::get_servers(false);
	for (int i = 0; i < servers.size(); i++) {
		_add_server_table_row(servers[i], _get_server_status(servers[i].id));
	}

	if (servers.is_empty()) {
		MarginContainer *empty_margin = memnew(MarginContainer);
		empty_margin->add_theme_constant_override("margin_left", 28 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_bottom", 10 * EDSCALE);
		server_table->add_child(empty_margin);

		Label *empty_label = memnew(Label);
		empty_label->set_text(TTR("No MCP servers yet. Add a server to make external tools available to the AI Agent."));
		empty_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
		empty_margin->add_child(empty_label);
	}
}

void AISettingsMCPPage::_add_server_table_row(const AIMCPServerConfig &p_server, const Dictionary &p_status) {
	server_rows.push_back(p_server);

	HBoxContainer *row = memnew(HBoxContainer);
	row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row->set_custom_minimum_size(Size2(0, 36) * EDSCALE);
	row->add_theme_constant_override("separation", 8 * EDSCALE);
	server_table->add_child(row);

	MarginContainer *status_margin = memnew(MarginContainer);
	status_margin->set_custom_minimum_size(Size2(62, 0) * EDSCALE);
	status_margin->add_theme_constant_override("margin_left", 20 * EDSCALE);
	status_margin->add_theme_constant_override("margin_right", 24 * EDSCALE);
	status_margin->add_theme_constant_override("margin_top", 12 * EDSCALE);
	status_margin->add_theme_constant_override("margin_bottom", 12 * EDSCALE);
	row->add_child(status_margin);

	ColorRect *status_indicator = memnew(ColorRect);
	status_indicator->set_custom_minimum_size(Size2(10, 10) * EDSCALE);
	status_indicator->set_color(_mcp_status_color(String(p_status.get("status", "checking")), p_server.enabled));
	status_indicator->set_tooltip_text(_mcp_status_tooltip(p_status, p_server.enabled));
	status_margin->add_child(status_indicator);

	CheckBox *enabled_check = memnew(CheckBox);
	enabled_check->set_pressed(p_server.enabled);
	enabled_check->set_custom_minimum_size(Size2(70, 0) * EDSCALE);
	enabled_check->connect(SceneStringName(toggled), callable_mp(this, &AISettingsMCPPage::_server_enabled_toggled).bind(p_server.id), CONNECT_DEFERRED);
	row->add_child(enabled_check);

	Label *name_label = _make_table_label(p_server.display_name);
	name_label->set_tooltip_text(p_server.display_name);
	row->add_child(name_label);

	Label *transport_label = _make_table_label(p_server.transport, 130);
	transport_label->set_tooltip_text(p_server.transport);
	row->add_child(transport_label);

	const String endpoint_text = p_server.transport == "stdio" ? p_server.command : p_server.url;
	const String endpoint_tooltip = p_server.transport == "stdio" ? p_server.command + (p_server.arguments.is_empty() ? String() : " " + p_server.arguments) : p_server.url;
	Label *command_label = _make_table_label(endpoint_text, 220);
	command_label->set_tooltip_text(endpoint_tooltip);
	row->add_child(command_label);

	HBoxContainer *action_cell = memnew(HBoxContainer);
	action_cell->set_custom_minimum_size(Size2(150, 0) * EDSCALE);
	action_cell->add_theme_constant_override("separation", 6 * EDSCALE);
	row->add_child(action_cell);

	Button *edit_button = memnew(Button);
	edit_button->set_text(TTR("Edit"));
	edit_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsMCPPage::_edit_server_pressed).bind(p_server.id), CONNECT_DEFERRED);
	action_cell->add_child(edit_button);

	Button *remove_button = memnew(Button);
	remove_button->set_text(TTR("Remove"));
	remove_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsMCPPage::_remove_server_pressed).bind(p_server.id), CONNECT_DEFERRED);
	action_cell->add_child(remove_button);

	HSeparator *row_separator = memnew(HSeparator);
	server_table->add_child(row_separator);
}

Dictionary AISettingsMCPPage::_get_server_status(const String &p_server_id) const {
	Ref<AIMCPService> mcp_service = AIMCPService::get_singleton();
	if (mcp_service.is_null()) {
		return Dictionary();
	}

	Array statuses = mcp_service->get_statuses();
	for (int i = 0; i < statuses.size(); i++) {
		if (Variant(statuses[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary status = statuses[i];
		if (String(status.get("server_id", String())) == p_server_id) {
			return status;
		}
	}
	return Dictionary();
}

void AISettingsMCPPage::_popup_add_server_dialog() {
	ERR_FAIL_NULL(server_dialog);
	server_dialog->popup_add_server();
}

void AISettingsMCPPage::_popup_import_json_dialog() {
	ERR_FAIL_NULL(json_import_dialog);
	ERR_FAIL_NULL(json_import_edit);
	json_import_edit->clear();
	json_import_dialog->popup_centered();
}

void AISettingsMCPPage::_json_import_confirmed() {
	ERR_FAIL_NULL(json_import_edit);
	String error;
	if (!AIMCPSettings::import_servers_from_json(json_import_edit->get_text(), error)) {
		WARN_PRINT("Failed to import MCP JSON: " + error);
		if (error_dialog) {
			error_dialog->set_text(error);
			error_dialog->popup_centered();
		}
		return;
	}

	_refresh_server_table();
	emit_signal(SNAME("settings_changed"));
	if (json_import_dialog) {
		json_import_dialog->hide();
	}
}

void AISettingsMCPPage::_mcp_status_changed(const Array &p_statuses, const Dictionary &p_summary) {
	(void)p_statuses;
	(void)p_summary;
	_refresh_server_table();
}

void AISettingsMCPPage::_server_submitted() {
	ERR_FAIL_NULL(server_dialog);

	AIMCPServerConfig server = server_dialog->get_submitted_server();
	if (server.display_name.is_empty()) {
		return;
	}
	if (server.transport == "stdio" && server.command.is_empty()) {
		return;
	}
	if (server.transport != "stdio" && server.url.is_empty()) {
		return;
	}

	if (server_dialog->is_editing_server()) {
		AIMCPSettings::update_server_config(server);
	} else {
		(void)AIMCPSettings::add_server_config(server);
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
