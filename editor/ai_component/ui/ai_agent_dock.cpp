/**************************************************************************/
/*  ai_agent_dock.cpp                                                      */
/**************************************************************************/

#include "ai_agent_dock.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/settings/editor_settings.h"
#include "editor/ai_component/ui/ai_agent_settings_dialog.h"
#include "scene/gui/box_container.h"

void AIAgentDock::_bind_methods() {
}

AIAgentDock::AIAgentDock() {
	singleton = this;

	set_name(TTRC("AI Agent"));
	set_layout_key("AI Agent");
	set_icon_name("EditorPlugin");
	set_dock_shortcut(ED_SHORTCUT_AND_COMMAND("docks/open_ai_agent", TTRC("Open AI Agent Dock")));
	set_default_slot(EditorDock::DOCK_SLOT_RIGHT_UL);

	VBoxContainer *main = memnew(VBoxContainer);
	main->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main->add_theme_constant_override("separation", 8);
	add_child(main);

	message_list = memnew(AIMessageList);
	main->add_child(message_list);

	composer = memnew(AIComposer);
	main->add_child(composer);

	composer->connect("send_requested", callable_mp(this, &AIAgentDock::_send_requested));
	composer->connect("cancel_requested", callable_mp(this, &AIAgentDock::_cancel_requested));

	if (AIAgentSettingsDialog::get_singleton()) {
		AIAgentSettingsDialog::get_singleton()->connect("ai_settings_changed", callable_mp(this, &AIAgentDock::_settings_changed));
	}
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
}

void AIAgentDock::_message_updated(int p_index, const Dictionary &p_message) {
	message_list->update_message(p_index, p_message);
}

void AIAgentDock::_state_changed(int p_state) {
	composer->set_running(p_state == AI_AGENT_STATE_STREAMING || p_state == AI_AGENT_STATE_PREPARING_CONTEXT);
}

void AIAgentDock::_settings_changed() {
	composer->reload_models();
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

AIProviderConfig AIAgentDock::_get_provider_config(const String &p_model) const {
	AIProviderConfig config;
	config.model = p_model;
	EditorSettings *settings = EditorSettings::get_singleton();
	if (!settings) {
		config.base_url = "https://api.deepseek.com/v1";
		return config;
	}

	config.api_key = settings->has_setting("deepseek/api_key") ? String(settings->get("deepseek/api_key")) : String();
	config.base_url = settings->has_setting("deepseek/url") ? String(settings->get("deepseek/url")) : String("https://api.deepseek.com/v1");
	return config;
}
