/**************************************************************************/
/*  ai_settings_mcp_page.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "ai_settings_mcp_page.h"

#include "core/io/json.h"
#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "editor/agent_ui/component/ai_mcp_server_dialog.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
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

Dictionary _dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return Dictionary(p_value).duplicate(true);
	}
	return Dictionary();
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
	if (p_width > 0) {
		label->set_custom_minimum_size(Size2(p_width, 0) * EDSCALE);
	} else {
		label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	}
	return label;
}

String _normalize_transport(const String &p_transport) {
	String transport = p_transport.strip_edges().to_lower().replace("-", "_");
	if (transport == "http") {
		transport = "streamable_http";
	}
	return transport.is_empty() ? String("stdio") : transport;
}

String _transport_from_server(const Dictionary &p_server) {
	const Variant transport_value = p_server.get("transport", p_server.get("transport_type", String("stdio")));
	if (transport_value.get_type() == Variant::DICTIONARY) {
		return _normalize_transport(String(Dictionary(transport_value).get("type", "stdio")));
	}
	return _normalize_transport(String(transport_value));
}

String _server_id(const Dictionary &p_server) {
	return String(p_server.get("id", String())).strip_edges();
}

String _server_name(const Dictionary &p_server) {
	const String name = String(p_server.get("name", p_server.get("display_name", p_server.get("displayName", String())))).strip_edges();
	if (!name.is_empty()) {
		return name;
	}
	return _server_id(p_server);
}

bool _server_enabled(const Dictionary &p_server) {
	return bool(p_server.get("enabled", true));
}

String _server_command(const Dictionary &p_server) {
	const Dictionary transport = _dictionary_from_variant(p_server.get("transport", Variant()));
	if (transport.has("command")) {
		const Variant command = transport["command"];
		if (command.get_type() == Variant::ARRAY) {
			const Array command_array = command;
			return command_array.is_empty() ? String() : String(command_array[0]).strip_edges();
		}
		return String(command).strip_edges();
	}
	return String(p_server.get("command", String())).strip_edges();
}

String _arguments_from_array(const Array &p_array) {
	Vector<String> parts;
	for (int i = 0; i < p_array.size(); i++) {
		parts.push_back(String(p_array[i]));
	}
	return String(" ").join(parts);
}

String _server_arguments(const Dictionary &p_server) {
	const Dictionary transport = _dictionary_from_variant(p_server.get("transport", Variant()));
	if (transport.has("command") && transport["command"].get_type() == Variant::ARRAY) {
		const Array command_array = transport["command"];
		Vector<String> parts;
		for (int i = 1; i < command_array.size(); i++) {
			parts.push_back(String(command_array[i]));
		}
		return String(" ").join(parts);
	}
	if (transport.has("arguments")) {
		return String(transport["arguments"]).strip_edges();
	}
	if (p_server.get("args", Variant()).get_type() == Variant::ARRAY) {
		return _arguments_from_array(p_server["args"]);
	}
	return String(p_server.get("arguments", String())).strip_edges();
}

String _server_url(const Dictionary &p_server) {
	const Dictionary transport = _dictionary_from_variant(p_server.get("transport", Variant()));
	if (transport.has("url")) {
		return String(transport["url"]).strip_edges();
	}
	return String(p_server.get("url", String())).strip_edges();
}

String _dictionary_to_lines(const Dictionary &p_dictionary) {
	Vector<String> lines;
	Array keys = p_dictionary.keys();
	for (int i = 0; i < keys.size(); i++) {
		const String key = String(keys[i]).strip_edges();
		if (key.is_empty()) {
			continue;
		}
		lines.push_back(key + "=" + String(p_dictionary[keys[i]]));
	}
	return String("\n").join(lines);
}

String _text_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return _dictionary_to_lines(p_value);
	}
	return String(p_value).strip_edges();
}

String _make_server_id(const String &p_display_name) {
	String name = p_display_name.strip_edges().validate_node_name().replace(" ", "_").to_lower();
	if (name.is_empty()) {
		name = "server";
	}
	return "mcp:" + name + ":" + String::num_uint64(OS::get_singleton()->get_ticks_usec()) + ":" + itos(Math::rand());
}

Dictionary _normalize_server_for_patch(Dictionary p_server, bool p_assign_id) {
	const String transport = _transport_from_server(p_server);
	const String name = _server_name(p_server).strip_edges();
	String id = _server_id(p_server);
	if (id.is_empty() && p_assign_id) {
		id = _make_server_id(name);
	}

	p_server["id"] = id;
	p_server["name"] = name;
	p_server["display_name"] = name;
	p_server["transport"] = transport;
	p_server["transport_type"] = transport;
	p_server["enabled"] = _server_enabled(p_server);
	p_server["command"] = _server_command(p_server);
	p_server["arguments"] = _server_arguments(p_server);
	p_server["url"] = _server_url(p_server);
	p_server["working_directory"] = String(p_server.get("working_directory", p_server.get("cwd", String()))).strip_edges();
	p_server["environment"] = _text_from_variant(p_server.get("environment", p_server.get("env", Variant())));
	p_server["headers"] = _text_from_variant(p_server.get("headers", Variant()));
	return p_server;
}

Color _mcp_status_color(const String &p_state, bool p_enabled) {
	if (!p_enabled || p_state == "disabled") {
		return Color(0.45, 0.45, 0.45, 1.0);
	}
	if (p_state == "ready" || p_state == "ok") {
		return Color(0.25, 0.78, 0.42, 1.0);
	}
	if (p_state == "failed") {
		return Color(0.95, 0.26, 0.22, 1.0);
	}
	return Color(0.95, 0.69, 0.18, 1.0);
}

String _mcp_status_tooltip(const Dictionary &p_status, bool p_enabled) {
	if (!p_enabled) {
		return TTR("MCP server is disabled.");
	}
	const String state = String(p_status.get("state", p_status.get("status", "checking")));
	if (state == "ready" || state == "ok") {
		return vformat(TTR("MCP server is available. Tools: %d."), (int)p_status.get("tool_count", 0));
	}
	if (state == "failed") {
		const String error = String(p_status.get("last_error", p_status.get("error", String())));
		return error.is_empty() ? TTR("MCP server is unavailable.") : error;
	}
	return TTR("MCP server is being checked.");
}

bool _append_server_from_json_source(const String &p_name, const Dictionary &p_source, Array &r_servers, String &r_error) {
	Dictionary server = p_source.duplicate(true);
	String display_name = String(p_source.get("name", p_source.get("display_name", p_source.get("displayName", p_name)))).strip_edges();
	if (display_name.is_empty()) {
		display_name = p_name.strip_edges();
	}

	const Variant transport_value = p_source.get("transport", p_source.get("type", Variant()));
	String transport;
	if (transport_value.get_type() == Variant::NIL && p_source.has("url")) {
		transport = "streamable_http";
	} else if (transport_value.get_type() == Variant::DICTIONARY) {
		transport = _normalize_transport(String(Dictionary(transport_value).get("type", "stdio")));
	} else {
		transport = _normalize_transport(transport_value.get_type() == Variant::NIL ? String("stdio") : String(transport_value));
	}

	server["name"] = display_name;
	server["display_name"] = display_name;
	server["transport"] = transport;
	server["transport_type"] = transport;
	server["enabled"] = bool(p_source.get("enabled", true));
	if (String(server.get("id", String())).strip_edges().is_empty()) {
		server["id"] = _make_server_id(display_name);
	}

	if (transport == "stdio") {
		const String command = _server_command(server);
		if (command.is_empty()) {
			r_error = "MCP stdio server `" + display_name + "` is missing command.";
			return false;
		}
		server["command"] = command;
		server["arguments"] = _server_arguments(server);
		server["working_directory"] = String(p_source.get("working_directory", p_source.get("cwd", String()))).strip_edges();
		server["environment"] = _text_from_variant(p_source.get("environment", p_source.get("env", Variant())));
	} else {
		const String url = _server_url(server);
		if (url.is_empty()) {
			r_error = "MCP HTTP server `" + display_name + "` is missing url.";
			return false;
		}
		server["url"] = url;
		server["headers"] = _text_from_variant(p_source.get("headers", Variant()));
	}

	r_servers.push_back(_normalize_server_for_patch(server, true));
	return true;
}

bool _parse_mcp_json_servers(const String &p_json, Array &r_servers, String &r_error) {
	r_error.clear();
	r_servers.clear();

	Ref<JSON> parser;
	parser.instantiate();
	Error err = parser->parse(p_json);
	if (err != OK || (parser->get_data().get_type() != Variant::DICTIONARY && parser->get_data().get_type() != Variant::ARRAY)) {
		r_error = "MCP JSON configuration is invalid.";
		return false;
	}

	if (parser->get_data().get_type() == Variant::ARRAY) {
		const Array source_array = parser->get_data();
		for (int i = 0; i < source_array.size(); i++) {
			if (source_array[i].get_type() == Variant::DICTIONARY && !_append_server_from_json_source(String(), source_array[i], r_servers, r_error)) {
				return false;
			}
		}
	} else {
		const Dictionary root = parser->get_data();
		const Variant servers_value = root.has("mcpServers") ? root["mcpServers"] : root.get("servers", Variant());
		if (servers_value.get_type() == Variant::DICTIONARY) {
			const Dictionary server_map = servers_value;
			Array keys = server_map.keys();
			for (int i = 0; i < keys.size(); i++) {
				if (server_map[keys[i]].get_type() == Variant::DICTIONARY && !_append_server_from_json_source(String(keys[i]), server_map[keys[i]], r_servers, r_error)) {
					return false;
				}
			}
		} else if (servers_value.get_type() == Variant::ARRAY) {
			const Array server_array = servers_value;
			for (int i = 0; i < server_array.size(); i++) {
				if (server_array[i].get_type() == Variant::DICTIONARY && !_append_server_from_json_source(String(), server_array[i], r_servers, r_error)) {
					return false;
				}
			}
		} else if (!_append_server_from_json_source(String(), root, r_servers, r_error)) {
			return false;
		}
	}

	if (r_servers.is_empty()) {
		r_error = "MCP JSON configuration did not contain any usable servers.";
		return false;
	}
	return true;
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

Ref<AIAgentV1UIBridge> AISettingsMCPPage::_get_adapter() const {
	return AIAgentV1UIBridge::get_singleton();
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
	description->set_text(TTR("Configure the MCP servers that agent_v1 can import as external tools. Tool discovery and runtime status are handled by the agent backend through the bridge."));
	description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
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

	Ref<AIAgentV1UIBridge> bridge = _get_adapter();
	if (bridge.is_valid()) {
		const Callable mcp_status_changed = callable_mp(this, &AISettingsMCPPage::_mcp_status_changed);
		if (!bridge->is_connected(SNAME("mcp_status_changed"), mcp_status_changed)) {
			bridge->connect(SNAME("mcp_status_changed"), mcp_status_changed);
		}
		const Callable config_changed = callable_mp(this, &AISettingsMCPPage::_config_changed);
		if (!bridge->is_connected(SNAME("config_changed"), config_changed)) {
			bridge->connect(SNAME("config_changed"), config_changed);
		}
	}

	_refresh_server_table();
}

Array AISettingsMCPPage::_get_mcp_servers() const {
	Ref<AIAgentV1UIBridge> bridge = _get_adapter();
	if (bridge.is_null()) {
		return Array();
	}
	const Dictionary snapshot = bridge->get_settings_snapshot();
	return _array_from_variant(snapshot.get("mcp_servers", Array()));
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
	status_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	header->add_child(status_header);

	Label *enabled_header = _make_table_label(TTR("Enabled"), 70);
	enabled_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	header->add_child(enabled_header);

	Label *name_header = _make_table_label(TTR("Name"));
	name_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	header->add_child(name_header);

	Label *transport_header = _make_table_label(TTR("Transport"), 130);
	transport_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	header->add_child(transport_header);

	Label *command_header = _make_table_label(TTR("Endpoint"), 220);
	command_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	header->add_child(command_header);

	Label *action_header = _make_table_label(TTR("Actions"), 150);
	action_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	header->add_child(action_header);

	HSeparator *header_separator = memnew(HSeparator);
	server_table->add_child(header_separator);

	const Array servers = _get_mcp_servers();
	for (int i = 0; i < servers.size(); i++) {
		if (servers[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary server = servers[i];
		_add_server_table_row(server, _get_server_status(_server_id(server)));
	}

	if (server_rows.is_empty()) {
		MarginContainer *empty_margin = memnew(MarginContainer);
		empty_margin->add_theme_constant_override("margin_left", 28 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_bottom", 10 * EDSCALE);
		server_table->add_child(empty_margin);

		Label *empty_label = memnew(Label);
		empty_label->set_text(TTR("No MCP servers yet. Add a server to make external tools available to the AI Agent."));
		empty_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
		empty_margin->add_child(empty_label);
	}
}

void AISettingsMCPPage::_add_server_table_row(const Dictionary &p_server, const Dictionary &p_status) {
	server_rows.push_back(p_server.duplicate(true));
	const String server_id = _server_id(p_server);
	const bool enabled = _server_enabled(p_server);
	const String transport = _transport_from_server(p_server);

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
	status_indicator->set_color(_mcp_status_color(String(p_status.get("state", p_status.get("status", "checking"))), enabled));
	status_indicator->set_tooltip_text(_mcp_status_tooltip(p_status, enabled));
	status_margin->add_child(status_indicator);

	CheckBox *enabled_check = memnew(CheckBox);
	enabled_check->set_pressed(enabled);
	enabled_check->set_custom_minimum_size(Size2(70, 0) * EDSCALE);
	enabled_check->connect(SceneStringName(toggled), callable_mp(this, &AISettingsMCPPage::_server_enabled_toggled).bind(server_id), CONNECT_DEFERRED);
	row->add_child(enabled_check);

	const String name = _server_name(p_server);
	Label *name_label = _make_table_label(name);
	name_label->set_tooltip_text(name);
	row->add_child(name_label);

	Label *transport_label = _make_table_label(transport, 130);
	transport_label->set_tooltip_text(transport);
	row->add_child(transport_label);

	const String command = _server_command(p_server);
	const String arguments = _server_arguments(p_server);
	const String url = _server_url(p_server);
	const String endpoint_text = transport == "stdio" ? command : url;
	const String endpoint_tooltip = transport == "stdio" ? command + (arguments.is_empty() ? String() : " " + arguments) : url;
	Label *command_label = _make_table_label(endpoint_text, 220);
	command_label->set_tooltip_text(endpoint_tooltip);
	row->add_child(command_label);

	HBoxContainer *action_cell = memnew(HBoxContainer);
	action_cell->set_custom_minimum_size(Size2(150, 0) * EDSCALE);
	action_cell->add_theme_constant_override("separation", 6 * EDSCALE);
	row->add_child(action_cell);

	Button *edit_button = memnew(Button);
	edit_button->set_text(TTR("Edit"));
	edit_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsMCPPage::_edit_server_pressed).bind(server_id), CONNECT_DEFERRED);
	action_cell->add_child(edit_button);

	Button *remove_button = memnew(Button);
	remove_button->set_text(TTR("Remove"));
	remove_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsMCPPage::_remove_server_pressed).bind(server_id), CONNECT_DEFERRED);
	action_cell->add_child(remove_button);

	HSeparator *row_separator = memnew(HSeparator);
	server_table->add_child(row_separator);
}

Dictionary AISettingsMCPPage::_get_server_status(const String &p_server_id) const {
	Ref<AIAgentV1UIBridge> bridge = _get_adapter();
	if (bridge.is_null()) {
		return Dictionary();
	}

	const Dictionary snapshot = bridge->get_settings_snapshot();
	const Array statuses = _array_from_variant(snapshot.get("mcp_statuses", Array()));
	for (int i = 0; i < statuses.size(); i++) {
		if (statuses[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary status = statuses[i];
		if (String(status.get("server_id", String())) == p_server_id) {
			return status;
		}
	}
	return Dictionary();
}

bool AISettingsMCPPage::_patch_mcp_server(Dictionary p_server, const String &p_scope) {
	Ref<AIAgentV1UIBridge> bridge = _get_adapter();
	if (bridge.is_null()) {
		return false;
	}

	p_server = _normalize_server_for_patch(p_server, true);
	const String id = _server_id(p_server);
	if (id.is_empty()) {
		return false;
	}

	Dictionary servers_patch;
	servers_patch[id] = p_server;
	Dictionary mcp_patch;
	mcp_patch["servers"] = servers_patch;
	Dictionary patch;
	patch["mcp"] = mcp_patch;
	const Dictionary result = bridge->patch_settings(patch, p_scope);
	return bool(result.get("success", false));
}

bool AISettingsMCPPage::_remove_mcp_server(const String &p_server_id, const String &p_scope) {
	Ref<AIAgentV1UIBridge> bridge = _get_adapter();
	if (bridge.is_null() || p_server_id.strip_edges().is_empty()) {
		return false;
	}

	Dictionary servers_patch;
	servers_patch[p_server_id] = Variant();
	Dictionary mcp_patch;
	mcp_patch["servers"] = servers_patch;
	Dictionary patch;
	patch["mcp"] = mcp_patch;
	const Dictionary result = bridge->patch_settings(patch, p_scope);
	return bool(result.get("success", false));
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
	Array imported;
	if (!_parse_mcp_json_servers(json_import_edit->get_text(), imported, error)) {
		WARN_PRINT("Failed to import MCP JSON: " + error);
		if (error_dialog) {
			error_dialog->set_text(error);
			error_dialog->popup_centered();
		}
		return;
	}

	Ref<AIAgentV1UIBridge> bridge = _get_adapter();
	if (bridge.is_null()) {
		return;
	}

	Dictionary servers_patch;
	for (int i = 0; i < imported.size(); i++) {
		if (imported[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary server = imported[i];
		servers_patch[_server_id(server)] = server;
	}

	Dictionary mcp_patch;
	mcp_patch["servers"] = servers_patch;
	Dictionary patch;
	patch["mcp"] = mcp_patch;
	const Dictionary result = bridge->patch_settings(patch, "project");
	if (!bool(result.get("success", false))) {
		const Dictionary error_result = result.get("error", Dictionary());
		const String message = String(error_result.get("message", TTR("Could not import MCP servers.")));
		WARN_PRINT("Failed to import MCP JSON: " + message);
		if (error_dialog) {
			error_dialog->set_text(message);
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

void AISettingsMCPPage::_config_changed(const String &p_scope, const Dictionary &p_config) {
	(void)p_scope;
	(void)p_config;
	_refresh_server_table();
}

void AISettingsMCPPage::_server_submitted() {
	ERR_FAIL_NULL(server_dialog);

	Dictionary server = server_dialog->get_submitted_server();
	server = _normalize_server_for_patch(server, !server_dialog->is_editing_server());
	const String name = _server_name(server);
	const String transport = _transport_from_server(server);
	if (name.is_empty()) {
		return;
	}
	if (transport == "stdio" && _server_command(server).is_empty()) {
		return;
	}
	if (transport != "stdio" && _server_url(server).is_empty()) {
		return;
	}

	if (_patch_mcp_server(server)) {
		_refresh_server_table();
		emit_signal(SNAME("settings_changed"));
		server_dialog->hide();
	}
}

void AISettingsMCPPage::_edit_server_pressed(const String &p_server_id) {
	ERR_FAIL_NULL(server_dialog);
	const Array servers = _get_mcp_servers();
	for (int i = 0; i < servers.size(); i++) {
		if (servers[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary server = servers[i];
		if (_server_id(server) == p_server_id) {
			server_dialog->popup_edit_server(server);
			return;
		}
	}
}

void AISettingsMCPPage::_remove_server_pressed(const String &p_server_id) {
	if (_remove_mcp_server(p_server_id)) {
		_refresh_server_table();
		emit_signal(SNAME("settings_changed"));
	}
}

void AISettingsMCPPage::_server_enabled_toggled(bool p_enabled, const String &p_server_id) {
	const Array servers = _get_mcp_servers();
	for (int i = 0; i < servers.size(); i++) {
		if (servers[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary server = servers[i];
		if (_server_id(server) != p_server_id) {
			continue;
		}
		server["enabled"] = p_enabled;
		if (_patch_mcp_server(server)) {
			_refresh_server_table();
			emit_signal(SNAME("settings_changed"));
		}
		return;
	}
}

void AISettingsMCPPage::build_for_test() {
	_build_ui();
}

int AISettingsMCPPage::get_server_table_row_count_for_test() const {
	return server_rows.size();
}

void AISettingsMCPPage::add_server_for_test(const String &p_display_name, const String &p_command, bool p_enabled) {
	Dictionary server;
	server["name"] = p_display_name.strip_edges();
	server["display_name"] = p_display_name.strip_edges();
	server["transport"] = "stdio";
	server["transport_type"] = "stdio";
	server["command"] = p_command.strip_edges();
	server["enabled"] = p_enabled;
	(void)_patch_mcp_server(server, "runtime");
	_refresh_server_table();
}

void AISettingsMCPPage::set_server_enabled_for_test(const String &p_server_id, bool p_enabled) {
	const Array servers = _get_mcp_servers();
	for (int i = 0; i < servers.size(); i++) {
		if (servers[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary server = servers[i];
		if (_server_id(server) != p_server_id) {
			continue;
		}
		server["enabled"] = p_enabled;
		(void)_patch_mcp_server(server, "runtime");
		_refresh_server_table();
		return;
	}
}
