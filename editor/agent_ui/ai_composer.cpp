/**************************************************************************/
/*  ai_composer.cpp                                                        */
/**************************************************************************/

#include "ai_composer.h"

#include "core/input/input_event.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_bridge.h"
#include "editor/agent_ui/component/ai_composer_input.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/label.h"
#include "scene/gui/text_edit.h"
#include "servers/display/display_server.h"
#include "servers/text/text_server.h"

namespace {

enum ReferenceMenuId {
	REFERENCE_MENU_CLIPBOARD = 1,
	REFERENCE_MENU_FILE = 2,
	REFERENCE_MENU_CANVAS = 3,
};

} // namespace

void AIComposer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("reload_models"), &AIComposer::reload_models);
	ADD_SIGNAL(MethodInfo("send_requested", PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::STRING, "model_id"), PropertyInfo(Variant::STRING, "agent_profile_id"), PropertyInfo(Variant::ARRAY, "attachments")));
	ADD_SIGNAL(MethodInfo("agent_profile_selected", PropertyInfo(Variant::STRING, "agent_profile_id")));
	ADD_SIGNAL(MethodInfo("cancel_requested"));
}

void AIComposer::_notification(int p_what) {
	if (p_what == NOTIFICATION_THEME_CHANGED) {
		_update_action_button();
	}
}

AIComposer::AIComposer() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_SHRINK_END);

	input = memnew(AIComposerInput);
	input->set_changed_callback(callable_mp(this, &AIComposer::_input_text_changed));
	input->get_text_edit()->connect("gui_input", callable_mp(this, &AIComposer::_input_gui_input));
	add_child(input);

	HBoxContainer *bar = memnew(HBoxContainer);
	bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(bar);

	Label *mode_label = memnew(Label);
	mode_label->set_text(TTR("Mode:"));
	bar->add_child(mode_label);

	mode_selector = memnew(OptionButton);
	mode_selector->set_custom_minimum_size(Size2(80, 0) * EDSCALE);
	mode_selector->add_item(TTR("Auto"));
	mode_selector->set_item_metadata(0, "auto");
	mode_selector->add_item(TTR("Ask"));
	mode_selector->set_item_metadata(1, "ask");
	mode_selector->select(0);
	mode_selector->connect("item_selected", callable_mp(this, &AIComposer::_mode_selected));
	bar->add_child(mode_selector);

	Label *model_label = memnew(Label);
	model_label->set_text(TTR("Model:"));
	bar->add_child(model_label);

	model_selector = memnew(OptionButton);
	model_selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	model_selector->set_custom_minimum_size(Size2(96, 0) * EDSCALE);
	model_selector->set_fit_to_longest_item(false);
	model_selector->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	bar->add_child(model_selector);

	send_button = memnew(Button);
	send_button->set_button_icon(get_editor_theme_icon(SNAME("Send")));
	send_button->set_tooltip_text(TTR("Send"));
	send_button->connect("pressed", callable_mp(this, &AIComposer::_action_pressed));
	bar->add_child(send_button);

	reference_menu = memnew(PopupMenu);
	reference_menu->add_item(TTR("@clipboard"), REFERENCE_MENU_CLIPBOARD);
	reference_menu->add_item(TTR("@canvas"), REFERENCE_MENU_CANVAS);
	reference_menu->add_item(TTR("Reference File..."), REFERENCE_MENU_FILE);
	reference_menu->connect("id_pressed", callable_mp(this, &AIComposer::_reference_menu_id_pressed));
	add_child(reference_menu);

	reference_file_dialog = memnew(EditorFileDialog);
	reference_file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	reference_file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	reference_file_dialog->add_filter("*", TTR("All Files"));
	reference_file_dialog->connect("file_selected", callable_mp(this, &AIComposer::_reference_file_selected));
	reference_file_dialog->connect("canceled", callable_mp(this, &AIComposer::_reference_file_dialog_canceled));
	add_child(reference_file_dialog);

	reload_models();
	set_running(false);
}

String AIComposer::get_input_text() const {
	return input->get_text();
}

String AIComposer::get_selected_model() const {
	if (!has_model || model_selector->get_item_count() == 0) {
		return String();
	}
	return String(model_selector->get_item_metadata(model_selector->get_selected()));
}

String AIComposer::get_selected_agent_profile_id() const {
	if (!mode_selector || mode_selector->get_item_count() == 0 || mode_selector->get_selected() < 0) {
		return "ask";
	}
	return String(mode_selector->get_item_metadata(mode_selector->get_selected()));
}

Array AIComposer::get_attachments_for_send() const {
	return input->get_attachments_for_send();
}

void AIComposer::_mode_selected(int p_index) {
	(void)p_index;
	emit_signal(SNAME("agent_profile_selected"), get_selected_agent_profile_id());
}

void AIComposer::clear_input() {
	input->clear();
	_update_action_button();
}

void AIComposer::set_running(bool p_running) {
	running = p_running;
	_update_action_button();
	if (mode_selector) {
		mode_selector->set_disabled(p_running);
	}
	if (model_selector) {
		model_selector->set_disabled(p_running || !has_model);
	}
}

void AIComposer::reload_models() {
	model_selector->clear();
	has_model = false;

	Ref<AIAgentV1UIBridge> bridge = AIAgentV1UIBridge::get_singleton();
	const Array profiles = bridge->list_model_profiles(true);
	for (int i = 0; i < profiles.size(); i++) {
		if (profiles[i].get_type() != Variant::DICTIONARY) {
			continue;
		}

		const Dictionary profile = profiles[i];
		const String profile_id = String(profile.get("id", String())).strip_edges();
		const String model = String(profile.get("model", String())).strip_edges();
		if (profile_id.is_empty() || model.is_empty()) {
			continue;
		}

		int item_index = model_selector->get_item_count();
		String label = String(profile.get("display_name", String())).strip_edges();
		if (label.is_empty()) {
			label = String(profile.get("provider_name", profile.get("provider_id", String()))).strip_edges();
			label = label.is_empty() ? model : label + " / " + model;
		} else if (!label.contains(model)) {
			label += " / " + model;
		}
		model_selector->add_item(label);
		model_selector->set_item_metadata(item_index, profile_id);
	}

	if (model_selector->get_item_count() == 0) {
		model_selector->add_item(TTR("No model configured"));
		model_selector->set_disabled(true);
		_update_action_button();
		return;
	}

	has_model = true;
	model_selector->set_disabled(false);
	_update_action_button();
}

void AIComposer::_input_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> key_event = p_event;
	if (key_event.is_null() || !key_event->is_pressed()) {
		return;
	}
	TextEdit *text_edit = input ? input->get_text_edit() : nullptr;
	if (!text_edit) {
		return;
	}
	if (!running && key_event->get_unicode() == '@' && !key_event->is_ctrl_pressed() && !key_event->is_alt_pressed() && !key_event->is_meta_pressed()) {
		reference_trigger_line = text_edit->get_caret_line();
		reference_trigger_column = text_edit->get_caret_column();
		callable_mp(this, &AIComposer::_show_reference_menu).call_deferred();
		return;
	}
	if (!running && key_event->is_action("ui_paste", true)) {
		DisplayServer *display_server = DisplayServer::get_singleton();
		if (display_server && display_server->clipboard_has_image()) {
			text_edit->accept_event();
			_add_clipboard_reference();
			_update_action_button();
			return;
		}
	}
	if (key_event->get_keycode() != Key::ENTER && key_event->get_keycode() != Key::KP_ENTER) {
		return;
	}
	// Enter alone sends; Shift+Enter or Ctrl+Enter inserts a newline.
	if (key_event->is_shift_pressed() || key_event->is_ctrl_pressed()) {
		return;
	}
	text_edit->accept_event();
	_action_pressed();
}

void AIComposer::_action_pressed() {
	if (running) {
		emit_signal(SNAME("cancel_requested"));
		return;
	}

	const String raw_message = get_input_text();
	String message = raw_message.strip_edges();
	const Array attachments = get_attachments_for_send();
	if (!has_model || (message.is_empty() && attachments.is_empty())) {
		return;
	}
	if (message.is_empty()) {
		message = TTR("Please review the attached references.");
	} else {
		message = raw_message;
	}
	emit_signal(SNAME("send_requested"), message, get_selected_model(), get_selected_agent_profile_id(), attachments);
}

void AIComposer::_input_text_changed() {
	_update_action_button();
}

void AIComposer::_reference_menu_id_pressed(int p_id) {
	switch (p_id) {
		case REFERENCE_MENU_CLIPBOARD: {
			_add_clipboard_reference();
		} break;
		case REFERENCE_MENU_CANVAS: {
			_add_canvas_reference();
		} break;
		case REFERENCE_MENU_FILE: {
			if (reference_file_dialog) {
				reference_file_dialog->popup_file_dialog();
			} else {
				_clear_reference_trigger();
			}
		} break;
		default:
			_clear_reference_trigger();
			break;
	}

	_update_action_button();
}

void AIComposer::_reference_file_selected(const String &p_path) {
	_add_reference_path(p_path);
}

void AIComposer::_reference_file_dialog_canceled() {
	_clear_reference_trigger();
}

void AIComposer::_show_reference_menu() {
	if (running || !reference_menu || !input) {
		return;
	}

	TextEdit *text_edit = input->get_text_edit();
	if (!text_edit) {
		return;
	}
	reference_menu->set_position(text_edit->get_screen_position() + text_edit->get_caret_draw_pos());
	reference_menu->reset_size();
	reference_menu->popup();
}

void AIComposer::_clear_reference_trigger() {
	reference_trigger_line = -1;
	reference_trigger_column = -1;
}

void AIComposer::_remove_reference_trigger() {
	TextEdit *text_edit = input ? input->get_text_edit() : nullptr;
	if (!text_edit) {
		_clear_reference_trigger();
		return;
	}

	if (reference_trigger_line >= 0 && reference_trigger_column >= 0 && reference_trigger_line < text_edit->get_line_count()) {
		const String line = text_edit->get_line(reference_trigger_line);
		if (reference_trigger_column < line.length() && line[reference_trigger_column] == '@') {
			text_edit->remove_text(reference_trigger_line, reference_trigger_column, reference_trigger_line, reference_trigger_column + 1);
			text_edit->set_caret_line(reference_trigger_line, false, true);
			text_edit->set_caret_column(reference_trigger_column, false);
		}
	}

	_clear_reference_trigger();
}

void AIComposer::_add_clipboard_reference() {
	if (!input) {
		_clear_reference_trigger();
		return;
	}
	_remove_reference_trigger();
	input->add_clipboard_reference();
	input->get_text_edit()->grab_focus();
	_update_action_button();
}

void AIComposer::_add_canvas_reference() {
	if (!input) {
		_clear_reference_trigger();
		return;
	}
	_remove_reference_trigger();
	input->add_canvas_reference();
	input->get_text_edit()->grab_focus();
	_update_action_button();
}

void AIComposer::_add_reference_path(const String &p_path) {
	if (!input) {
		_clear_reference_trigger();
		return;
	}
	_remove_reference_trigger();
	input->add_reference_path(p_path);
	input->get_text_edit()->grab_focus();
	_update_action_button();
}

void AIComposer::_update_action_button() {
	if (!send_button) {
		return;
	}

	if (running) {
		send_button->set_button_icon(get_editor_theme_icon(SNAME("Stop")));
		send_button->set_tooltip_text(TTR("Cancel"));
		send_button->set_disabled(false);
		return;
	}

	send_button->set_button_icon(get_editor_theme_icon(SNAME("Send")));
	send_button->set_tooltip_text(TTR("Send"));
	send_button->set_disabled(!has_model || (get_input_text().strip_edges().is_empty() && get_attachments_for_send().is_empty()));
}
