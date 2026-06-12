/**************************************************************************/
/*  ai_settings_mcp_page.h                                                */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_bridge.h"

#include "scene/gui/margin_container.h"

class AIMCPServerDialog;
class Button;
class AcceptDialog;
class TextEdit;
class VBoxContainer;

class AISettingsMCPPage : public MarginContainer {
	GDCLASS(AISettingsMCPPage, MarginContainer);

	VBoxContainer *server_table = nullptr;
	Button *add_server_button = nullptr;
	Button *import_json_button = nullptr;
	AIMCPServerDialog *server_dialog = nullptr;
	AcceptDialog *json_import_dialog = nullptr;
	AcceptDialog *error_dialog = nullptr;
	TextEdit *json_import_edit = nullptr;
	Vector<Dictionary> server_rows;

	Ref<AIAgentV1UIBridge> _get_adapter() const;
	void _build_ui();
	void _refresh_server_table();
	void _add_server_table_row(const Dictionary &p_server, const Dictionary &p_status);
	Dictionary _get_server_status(const String &p_server_id) const;
	Array _get_mcp_servers() const;
	bool _patch_mcp_server(Dictionary p_server, const String &p_scope = "project");
	bool _remove_mcp_server(const String &p_server_id, const String &p_scope = "project");
	void _popup_add_server_dialog();
	void _popup_import_json_dialog();
	void _json_import_confirmed();
	void _mcp_status_changed(const Array &p_statuses, const Dictionary &p_summary);
	void _config_changed(const String &p_scope, const Dictionary &p_config);
	void _server_submitted();
	void _edit_server_pressed(const String &p_server_id);
	void _remove_server_pressed(const String &p_server_id);
	void _server_enabled_toggled(bool p_enabled, const String &p_server_id);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AISettingsMCPPage();

	void build_for_test();
	int get_server_table_row_count_for_test() const;
	void add_server_for_test(const String &p_display_name, const String &p_command, bool p_enabled = true);
	void set_server_enabled_for_test(const String &p_server_id, bool p_enabled);
};
