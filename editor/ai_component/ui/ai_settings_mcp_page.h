/**************************************************************************/
/*  ai_settings_mcp_page.h                                                */
/**************************************************************************/

#pragma once

#include "editor/ai_component/providers/ai_mcp_settings.h"
#include "scene/gui/margin_container.h"

class AIMCPServerDialog;
class Button;
class VBoxContainer;

class AISettingsMCPPage : public MarginContainer {
	GDCLASS(AISettingsMCPPage, MarginContainer);

	VBoxContainer *server_table = nullptr;
	Button *add_server_button = nullptr;
	AIMCPServerDialog *server_dialog = nullptr;
	Vector<AIMCPServerConfig> server_rows;

	void _build_ui();
	void _refresh_server_table();
	void _add_server_table_row(const AIMCPServerConfig &p_server);
	void _popup_add_server_dialog();
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
