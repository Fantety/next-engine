/**************************************************************************/
/*  ai_mcp_server_dialog.h                                                */
/**************************************************************************/

#pragma once

#include "core/variant/dictionary.h"

#include "scene/gui/dialogs.h"

class CheckBox;
class OptionButton;
class LineEdit;
class TextEdit;

class AIMCPServerDialog : public ConfirmationDialog {
	GDCLASS(AIMCPServerDialog, ConfirmationDialog);

	String editing_server_id;
	LineEdit *name_edit = nullptr;
	OptionButton *transport_option = nullptr;
	LineEdit *command_edit = nullptr;
	TextEdit *arguments_edit = nullptr;
	LineEdit *working_directory_edit = nullptr;
	TextEdit *environment_edit = nullptr;
	LineEdit *url_edit = nullptr;
	TextEdit *headers_edit = nullptr;
	CheckBox *enabled_check = nullptr;
	Dictionary editing_server_snapshot;

	void _build_ui();
	void _reset_form();
	void _confirmed();
	void _transport_selected(int p_index);
	void _sync_transport_fields();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIMCPServerDialog();

	void popup_add_server();
	void popup_edit_server(const Dictionary &p_server);
	bool is_editing_server() const;
	String get_editing_server_id() const;
	Dictionary get_submitted_server() const;
};
