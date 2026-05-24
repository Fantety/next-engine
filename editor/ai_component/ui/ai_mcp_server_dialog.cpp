/**************************************************************************/
/*  ai_mcp_server_dialog.cpp                                              */
/**************************************************************************/

#include "ai_mcp_server_dialog.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/check_box.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/text_edit.h"

namespace {

String _ai_ui_text(const char *p_text) {
	return String::utf8(p_text);
}

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

void AIMCPServerDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("server_submitted"));
}

void AIMCPServerDialog::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build_ui();
	}
}

AIMCPServerDialog::AIMCPServerDialog() {
	set_min_size(Size2(620, 420) * EDSCALE);
	set_ok_button_text(TTR("Save"));
	set_hide_on_ok(false);
}

void AIMCPServerDialog::_build_ui() {
	if (name_edit) {
		return;
	}

	VBoxContainer *root = memnew(VBoxContainer);
	root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	root->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	root->add_theme_constant_override("separation", 10 * EDSCALE);
	add_child(root);

	name_edit = memnew(LineEdit);
	name_edit->set_placeholder(TTR("Filesystem"));
	_make_field_row(root, _ai_ui_text(u8"\u540d\u79f0"), name_edit);

	command_edit = memnew(LineEdit);
	command_edit->set_placeholder("npx");
	_make_field_row(root, TTR("Command"), command_edit);

	arguments_edit = memnew(TextEdit);
	arguments_edit->set_custom_minimum_size(Size2(0, 74) * EDSCALE);
	arguments_edit->set_placeholder("-y @modelcontextprotocol/server-filesystem .");
	_make_field_row(root, TTR("Arguments"), arguments_edit);

	working_directory_edit = memnew(LineEdit);
	working_directory_edit->set_placeholder("res://");
	_make_field_row(root, TTR("Working Directory"), working_directory_edit);

	environment_edit = memnew(TextEdit);
	environment_edit->set_custom_minimum_size(Size2(0, 74) * EDSCALE);
	environment_edit->set_placeholder("KEY=value");
	_make_field_row(root, TTR("Environment"), environment_edit);

	enabled_check = memnew(CheckBox);
	enabled_check->set_text(TTR("Enabled"));
	root->add_child(enabled_check);

	connect(SceneStringName(confirmed), callable_mp(this, &AIMCPServerDialog::_confirmed), CONNECT_DEFERRED);
}

void AIMCPServerDialog::_reset_form() {
	_build_ui();
	editing_server_id.clear();
	name_edit->clear();
	command_edit->clear();
	arguments_edit->clear();
	working_directory_edit->clear();
	environment_edit->clear();
	enabled_check->set_pressed(true);
}

void AIMCPServerDialog::_confirmed() {
	emit_signal(SNAME("server_submitted"));
}

void AIMCPServerDialog::popup_add_server() {
	_reset_form();
	set_title(TTR("Add MCP Server"));
	popup_centered();
}

void AIMCPServerDialog::popup_edit_server(const AIMCPServerConfig &p_server) {
	_reset_form();
	editing_server_id = p_server.id;
	name_edit->set_text(p_server.display_name);
	command_edit->set_text(p_server.command);
	arguments_edit->set_text(p_server.arguments);
	working_directory_edit->set_text(p_server.working_directory);
	environment_edit->set_text(p_server.environment);
	enabled_check->set_pressed(p_server.enabled);
	set_title(TTR("Edit MCP Server"));
	popup_centered();
}

bool AIMCPServerDialog::is_editing_server() const {
	return !editing_server_id.is_empty();
}

String AIMCPServerDialog::get_editing_server_id() const {
	return editing_server_id;
}

AIMCPServerConfig AIMCPServerDialog::get_submitted_server() const {
	AIMCPServerConfig server;
	if (!name_edit || !command_edit || !arguments_edit || !working_directory_edit || !environment_edit || !enabled_check) {
		return server;
	}

	server.id = editing_server_id;
	server.display_name = name_edit->get_text().strip_edges();
	server.command = command_edit->get_text().strip_edges();
	server.arguments = arguments_edit->get_text().strip_edges();
	server.working_directory = working_directory_edit->get_text().strip_edges();
	server.environment = environment_edit->get_text().strip_edges();
	server.enabled = enabled_check->is_pressed();
	return server;
}
