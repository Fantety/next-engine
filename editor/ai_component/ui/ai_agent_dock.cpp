/**************************************************************************/
/*  ai_agent_dock.cpp                                                      */
/**************************************************************************/

#include "ai_agent_dock.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/ai_component/ui/ai_agent_settings_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/separator.h"

void AIAgentDock::_bind_methods() {
}

AIAgentDock::AIAgentDock() {
	singleton = this;

	set_name(TTRC("AI Agent"));
	set_layout_key("AI Agent");
	set_icon_name("EditorPlugin");
	set_dock_shortcut(ED_SHORTCUT_AND_COMMAND("docks/open_ai_agent", TTRC("Open AI Agent Dock")));
	set_default_slot(EditorDock::DOCK_SLOT_RIGHT_UL);

	HBoxContainer *root = memnew(HBoxContainer);
	root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	root->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	root->add_theme_constant_override("separation", 8 * EDSCALE);
	add_child(root);

	VBoxContainer *sidebar = memnew(VBoxContainer);
	sidebar->set_custom_minimum_size(Size2(180, 0) * EDSCALE);
	sidebar->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	root->add_child(sidebar);

	new_session_button = memnew(Button);
	new_session_button->set_text(TTR("New Chat"));
	new_session_button->connect(SceneStringName(pressed), callable_mp(this, &AIAgentDock::_new_session_pressed));
	sidebar->add_child(new_session_button);

	Label *history_label = memnew(Label);
	history_label->set_text(TTR("History"));
	sidebar->add_child(history_label);

	session_list = memnew(ItemList);
	session_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	session_list->set_select_mode(ItemList::SELECT_SINGLE);
	session_list->connect(SceneStringName(item_selected), callable_mp(this, &AIAgentDock::_session_selected));
	sidebar->add_child(session_list);

	VSeparator *separator = memnew(VSeparator);
	root->add_child(separator);

	VBoxContainer *main = memnew(VBoxContainer);
	main->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main->add_theme_constant_override("separation", 8 * EDSCALE);
	root->add_child(main);

	message_list = memnew(AIMessageList);
	main->add_child(message_list);

	composer = memnew(AIComposer);
	main->add_child(composer);

	composer->connect("send_requested", callable_mp(this, &AIAgentDock::_send_requested));
	composer->connect("cancel_requested", callable_mp(this, &AIAgentDock::_cancel_requested));

	if (AIAgentSettingsDialog::get_singleton()) {
		AIAgentSettingsDialog::get_singleton()->connect("ai_settings_changed", callable_mp(this, &AIAgentDock::_settings_changed));
	}

	_ensure_session();
	_refresh_session_list();
}

AIAgentDock *AIAgentDock::get_singleton() {
	return singleton;
}

void AIAgentDock::_send_requested(const String &p_message, const String &p_model) {
	_ensure_session();
	ERR_FAIL_NULL(session);

	session->configure_provider(_get_provider_config(p_model));
	session->send_user_message(p_message);
	composer->clear_input();
}

void AIAgentDock::_cancel_requested() {
	if (session) {
		session->cancel_request();
	}
}

void AIAgentDock::_message_added(const Dictionary &p_message) {
	message_list->add_message(p_message);
	_refresh_session_list();
}

void AIAgentDock::_message_updated(int p_index, const Dictionary &p_message) {
	message_list->update_message(p_index, p_message);
}

void AIAgentDock::_state_changed(int p_state) {
	composer->set_running(p_state == AI_AGENT_STATE_STREAMING || p_state == AI_AGENT_STATE_PREPARING_CONTEXT);
	if (p_state == AI_AGENT_STATE_IDLE || p_state == AI_AGENT_STATE_FAILED || p_state == AI_AGENT_STATE_CANCELLED) {
		_refresh_session_list();
	}
}

void AIAgentDock::_settings_changed() {
	composer->reload_models();
}

void AIAgentDock::_new_session_pressed() {
	_ensure_session();
	ERR_FAIL_NULL(session);

	session->start_new_session();
	message_list->clear_messages();
	_refresh_session_list();
}

void AIAgentDock::_session_selected(int p_index) {
	_ensure_session();
	ERR_FAIL_NULL(session);

	if (!session_list || p_index < 0) {
		return;
	}

	String session_id = session_list->get_item_metadata(p_index);
	if (session_id.is_empty() || session_id == session->get_session_id()) {
		return;
	}

	if (session->load_session(session_id)) {
		_reload_messages_from_session();
		_refresh_session_list();
	}
}

void AIAgentDock::_ensure_session() {
	if (session) {
		return;
	}

	session = memnew(AIAgentSession);
	add_child(session);
	session->connect("message_added", callable_mp(this, &AIAgentDock::_message_added));
	session->connect("message_updated", callable_mp(this, &AIAgentDock::_message_updated));
	session->connect("state_changed", callable_mp(this, &AIAgentDock::_state_changed));
}

void AIAgentDock::_refresh_session_list() {
	if (!session_list || !session) {
		return;
	}

	session_list->clear();
	Array sessions = session->list_sessions();
	for (int i = 0; i < sessions.size(); i++) {
		if (Variant(sessions[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary item = sessions[i];
		String session_id = item.get("id", String());
		if (session_id.is_empty()) {
			continue;
		}
		String session_title = item.get("title", TTR("New Chat"));
		if (session_title.length() > 42) {
			session_title = session_title.substr(0, 39) + "...";
		}
		int index = session_list->add_item(session_title);
		session_list->set_item_metadata(index, session_id);
	}
	_select_current_session();
}

void AIAgentDock::_select_current_session() {
	if (!session_list || !session) {
		return;
	}

	String current_id = session->get_session_id();
	for (int i = 0; i < session_list->get_item_count(); i++) {
		if (String(session_list->get_item_metadata(i)) == current_id) {
			session_list->select(i);
			return;
		}
	}
}

void AIAgentDock::_reload_messages_from_session() {
	ERR_FAIL_NULL(session);
	message_list->clear_messages();
	Array messages = session->get_messages_as_array();
	for (int i = 0; i < messages.size(); i++) {
		if (Variant(messages[i]).get_type() == Variant::DICTIONARY) {
			message_list->add_message(messages[i]);
		}
	}
}

AIProviderConfig AIAgentDock::_get_provider_config(const String &p_model) const {
	return AIModelSettings::get_provider_config(p_model);
}
