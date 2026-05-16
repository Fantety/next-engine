/**************************************************************************/
/*  ai_agent_settings_dialog.cpp                                           */
/**************************************************************************/

#include "ai_agent_settings_dialog.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/box_container.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"

void AIAgentSettingsDialog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_save_settings"), &AIAgentSettingsDialog::_save_settings);
	ADD_SIGNAL(MethodInfo("ai_settings_changed"));
}

void AIAgentSettingsDialog::_notification(int p_what) {
	if (p_what != NOTIFICATION_READY) {
		return;
	}

	EditorSettings *settings = EditorSettings::get_singleton();

	set_title(TTR("AI Agent Settings"));
	set_min_size(Size2(640, 320));

	VBoxContainer *main = memnew(VBoxContainer);
	main->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(main);

	Label *provider_label = memnew(Label);
	provider_label->set_text(TTR("DeepSeek / OpenAI-compatible"));
	main->add_child(provider_label);

	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	grid->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main->add_child(grid);

	Label *key_label = memnew(Label);
	key_label->set_text(TTR("API Key:"));
	grid->add_child(key_label);
	deepseek_api_key = memnew(LineEdit);
	deepseek_api_key->set_secret(true);
	deepseek_api_key->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	if (settings && settings->has_setting("deepseek/api_key")) {
		deepseek_api_key->set_text(settings->get("deepseek/api_key"));
	}
	grid->add_child(deepseek_api_key);

	Label *url_label = memnew(Label);
	url_label->set_text(TTR("Base URL:"));
	grid->add_child(url_label);
	deepseek_base_url = memnew(LineEdit);
	deepseek_base_url->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	deepseek_base_url->set_text((settings && settings->has_setting("deepseek/url")) ? String(settings->get("deepseek/url")) : String("https://api.deepseek.com/v1"));
	grid->add_child(deepseek_base_url);

	deepseek_chat = memnew(CheckButton);
	deepseek_chat->set_text("deepseek-chat");
	deepseek_chat->set_pressed(!settings || !settings->has_setting("deepseek/models/deepseek-chat") || bool(settings->get("deepseek/models/deepseek-chat")));
	main->add_child(deepseek_chat);

	deepseek_reasoner = memnew(CheckButton);
	deepseek_reasoner->set_text("deepseek-reasoner");
	deepseek_reasoner->set_pressed(!settings || !settings->has_setting("deepseek/models/deepseek-reasoner") || bool(settings->get("deepseek/models/deepseek-reasoner")));
	main->add_child(deepseek_reasoner);

	get_ok_button()->set_text(TTR("Save"));
	connect("confirmed", callable_mp(this, &AIAgentSettingsDialog::_save_settings));
}

AIAgentSettingsDialog::AIAgentSettingsDialog() {
	singleton = this;
}

AIAgentSettingsDialog *AIAgentSettingsDialog::get_singleton() {
	return singleton;
}

void AIAgentSettingsDialog::_save_settings() {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);

	settings->set("deepseek/api_key", deepseek_api_key->get_text().strip_edges());
	settings->set("deepseek/url", deepseek_base_url->get_text().strip_edges());
	settings->set("deepseek/models/deepseek-chat", deepseek_chat->is_pressed());
	settings->set("deepseek/models/deepseek-reasoner", deepseek_reasoner->is_pressed());
	settings->save();
	emit_signal(SNAME("ai_settings_changed"));
}
