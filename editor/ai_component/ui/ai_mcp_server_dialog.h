/**************************************************************************/
/*  ai_mcp_server_dialog.h                                                */
/**************************************************************************/

#pragma once

#include "editor/ai_component/providers/ai_mcp_settings.h"
#include "scene/gui/dialogs.h"

class CheckBox;
class LineEdit;
class TextEdit;

class AIMCPServerDialog : public ConfirmationDialog {
	GDCLASS(AIMCPServerDialog, ConfirmationDialog);

	String editing_server_id;
	LineEdit *name_edit = nullptr;
	LineEdit *command_edit = nullptr;
	TextEdit *arguments_edit = nullptr;
	LineEdit *working_directory_edit = nullptr;
	TextEdit *environment_edit = nullptr;
	CheckBox *enabled_check = nullptr;

	void _build_ui();
	void _reset_form();
	void _confirmed();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIMCPServerDialog();

	void popup_add_server();
	void popup_edit_server(const AIMCPServerConfig &p_server);
	bool is_editing_server() const;
	String get_editing_server_id() const;
	AIMCPServerConfig get_submitted_server() const;
};
