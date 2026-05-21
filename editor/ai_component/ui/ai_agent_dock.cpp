/**************************************************************************/
/*  ai_agent_dock.cpp                                                      */
/**************************************************************************/

#include "ai_agent_dock.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/ai_component/ui/ai_agent_settings_dialog.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/dialogs.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"
#include "servers/text/text_server.h"

void AIAgentDock::_bind_methods() {
}

AIAgentDock::AIAgentDock() {
	singleton = this;

	set_name(TTRC("AI Agent"));
	set_layout_key("AI Agent");
	set_icon_name("EditorPlugin");
	set_dock_shortcut(ED_SHORTCUT_AND_COMMAND("docks/open_ai_agent", TTRC("Open AI Agent Dock")));
	set_default_slot(EditorDock::DOCK_SLOT_RIGHT_UL);

	VBoxContainer *root = memnew(VBoxContainer);
	root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	root->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	root->add_theme_constant_override("separation", 8 * EDSCALE);
	add_child(root);

	HBoxContainer *session_bar = memnew(HBoxContainer);
	session_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	root->add_child(session_bar);

	session_selector = memnew(OptionButton);
	session_selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	session_selector->set_custom_minimum_size(Size2(96, 0) * EDSCALE);
	session_selector->set_fit_to_longest_item(false);
	session_selector->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	session_selector->connect(SceneStringName(item_selected), callable_mp(this, &AIAgentDock::_session_selected));
	session_bar->add_child(session_selector);

	new_session_button = memnew(Button);
	new_session_button->set_text(TTR("New"));
	new_session_button->set_tooltip_text(TTR("Start a new AI chat session."));
	new_session_button->connect(SceneStringName(pressed), callable_mp(this, &AIAgentDock::_new_session_pressed));
	session_bar->add_child(new_session_button);

	delete_session_button = memnew(Button);
	delete_session_button->set_text(TTR("Delete"));
	delete_session_button->set_tooltip_text(TTR("Delete the selected AI chat session."));
	delete_session_button->connect(SceneStringName(pressed), callable_mp(this, &AIAgentDock::_delete_session_pressed));
	session_bar->add_child(delete_session_button);

	delete_session_dialog = memnew(ConfirmationDialog);
	delete_session_dialog->set_title(TTR("Delete AI Chat"));
	delete_session_dialog->set_ok_button_text(TTR("Delete"));
	delete_session_dialog->set_cancel_button_text(TTR("Cancel"));
	delete_session_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AIAgentDock::_confirm_delete_session));
	add_child(delete_session_dialog);

	VBoxContainer *main = memnew(VBoxContainer);
	main->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main->add_theme_constant_override("separation", 8 * EDSCALE);
	root->add_child(main);

	message_list = memnew(AIMessageList);
	main->add_child(message_list);

	request_status_row = memnew(HBoxContainer);
	request_status_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	request_status_row->set_custom_minimum_size(Size2(0, 6) * EDSCALE);
	request_status_row->add_theme_constant_override("separation", 0);
	request_status_row->hide();
	main->add_child(request_status_row);

	Ref<Shader> request_progress_shader;
	request_progress_shader.instantiate();
	request_progress_shader->set_code(R"(
shader_type canvas_item;
render_mode unshaded;

uniform vec4 color_a : source_color = vec4(0.09, 0.68, 1.0, 1.0);
uniform vec4 color_b : source_color = vec4(0.53, 0.36, 1.0, 1.0);
uniform vec4 color_c : source_color = vec4(1.0, 0.33, 0.67, 1.0);
uniform vec4 color_d : source_color = vec4(0.18, 0.92, 0.78, 1.0);
uniform float speed = 0.95;

vec3 palette(float t) {
	vec3 a = color_a.rgb;
	vec3 b = color_b.rgb;
	vec3 c = color_c.rgb;
	vec3 d = color_d.rgb;
	return a + b * cos(TAU * (c * t + d));
}

void fragment() {
	float phase = TIME * speed;
	float wave = UV.x * 2.4 - phase;
	vec3 color = palette(wave);
	color += 0.08 * palette(wave + 0.17);
	color += 0.05 * palette(wave + 0.41);
	float edge = smoothstep(0.0, 0.08, min(UV.y, 1.0 - UV.y));
	COLOR = vec4(clamp(color, 0.0, 1.0), edge);
}
)");

	Ref<ShaderMaterial> request_progress_material;
	request_progress_material.instantiate();
	request_progress_material->set_shader(request_progress_shader);

	request_progress = memnew(ColorRect);
	request_progress->set_color(Color(1, 1, 1, 1));
	request_progress->set_material(request_progress_material);
	request_progress->set_custom_minimum_size(Size2(0, 4) * EDSCALE);
	request_progress->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	request_progress->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	request_status_row->add_child(request_progress);

	token_usage_label = memnew(Label);
	token_usage_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	token_usage_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	token_usage_label->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
	token_usage_label->set_tooltip_text(TTR("Current AI chat token usage. Provider tokens come from the model API; context tokens are estimated from prompt characters."));
	main->add_child(token_usage_label);

	composer = memnew(AIComposer);
	main->add_child(composer);

	composer->connect("send_requested", callable_mp(this, &AIAgentDock::_send_requested));
	composer->connect("cancel_requested", callable_mp(this, &AIAgentDock::_cancel_requested));

	if (AIAgentSettingsDialog::get_singleton()) {
		AIAgentSettingsDialog::get_singleton()->connect("ai_settings_changed", callable_mp(this, &AIAgentDock::_settings_changed));
	}

	_ensure_session();
	_reload_messages_from_session();
	_refresh_session_list();
}

AIAgentDock *AIAgentDock::get_singleton() {
	return singleton;
}

void AIAgentDock::_send_requested(const String &p_message, const String &p_model, const String &p_agent_profile_id) {
	_ensure_session();
	ERR_FAIL_NULL(session);

	session->configure_provider(_get_provider_config(p_model));
	session->set_agent_profile_id(p_agent_profile_id);
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
	_refresh_token_usage();
	_refresh_session_list();
}

void AIAgentDock::_message_updated(int p_index, const Dictionary &p_message) {
	message_list->update_message(p_index, p_message);
	_refresh_token_usage();
}

void AIAgentDock::_message_removed(int p_index) {
	message_list->remove_message(p_index);
	_refresh_token_usage();
}

void AIAgentDock::_state_changed(int p_state) {
	const bool running = p_state == AI_AGENT_STATE_STREAMING || p_state == AI_AGENT_STATE_PREPARING_CONTEXT;
	composer->set_running(running);
	if (new_session_button) {
		new_session_button->set_disabled(running);
	}
	if (delete_session_button) {
		delete_session_button->set_disabled(running || !session_selector || session_selector->get_item_count() == 0 || session_selector->get_selected() < 0);
	}
	if (request_progress) {
		request_progress->set_visible(running);
	}
	if (request_status_row) {
		request_status_row->set_visible(running);
	}
	if (p_state == AI_AGENT_STATE_IDLE || p_state == AI_AGENT_STATE_FAILED || p_state == AI_AGENT_STATE_CANCELLED) {
		_refresh_session_list();
	}
	_refresh_token_usage();
}

void AIAgentDock::_token_usage_changed(const Dictionary &p_usage) {
	(void)p_usage;
	_refresh_token_usage();
}

void AIAgentDock::_settings_changed() {
	composer->reload_models();
}

void AIAgentDock::_new_session_pressed() {
	_ensure_session();
	ERR_FAIL_NULL(session);

	session->start_new_session();
	message_list->clear_messages();
	_refresh_token_usage();
	_refresh_session_list();
}

void AIAgentDock::_delete_session_pressed() {
	_ensure_session();
	ERR_FAIL_NULL(session);

	const String session_id = _get_selected_session_id();
	if (session_id.is_empty()) {
		return;
	}

	if (session->get_state() == AI_AGENT_STATE_STREAMING || session->get_state() == AI_AGENT_STATE_PREPARING_CONTEXT) {
		return;
	}

	pending_delete_session_id = session_id;
	String session_title = session_selector->get_item_text(session_selector->get_selected());
	if (session_title.is_empty()) {
		session_title = TTR("New Chat");
	}

	delete_session_dialog->set_text(vformat(TTR("Delete AI chat \"%s\"?\n\nThis action cannot be undone."), session_title));
	delete_session_dialog->popup_centered();
}

void AIAgentDock::_confirm_delete_session() {
	_ensure_session();
	ERR_FAIL_NULL(session);

	const String session_id = pending_delete_session_id;
	pending_delete_session_id.clear();
	if (session_id.is_empty()) {
		return;
	}

	const bool deleting_current = session_id == session->get_session_id();
	if (!session->delete_session(session_id)) {
		_refresh_session_list();
		return;
	}

	if (deleting_current) {
		_reload_messages_from_session();
	}
	_refresh_session_list();
}

void AIAgentDock::_session_selected(int p_index) {
	_ensure_session();
	ERR_FAIL_NULL(session);

	if (!session_selector || p_index < 0) {
		return;
	}

	String session_id = session_selector->get_item_metadata(p_index);
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
	session->connect("message_removed", callable_mp(this, &AIAgentDock::_message_removed));
	session->connect("state_changed", callable_mp(this, &AIAgentDock::_state_changed));
	session->connect("token_usage_changed", callable_mp(this, &AIAgentDock::_token_usage_changed));
}

String AIAgentDock::_get_selected_session_id() const {
	if (!session_selector || session_selector->get_selected() < 0) {
		return String();
	}

	return session_selector->get_item_metadata(session_selector->get_selected());
}

void AIAgentDock::_refresh_session_list() {
	if (!session_selector || !session) {
		return;
	}

	session_selector->clear();
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
		const int64_t updated_at = int64_t(item.get("updated_at", 0));
		if (updated_at > 0) {
			OS::TimeZoneInfo tz = OS::get_singleton()->get_time_zone_info();
			session_title += "  " + Time::get_singleton()->get_datetime_string_from_unix_time(updated_at + tz.bias * 60, true);
		}
		int index = session_selector->get_item_count();
		session_selector->add_item(session_title);
		session_selector->set_item_metadata(index, session_id);
	}
	_select_current_session();
	if (delete_session_button) {
		delete_session_button->set_disabled(session_selector->get_item_count() == 0 || session_selector->get_selected() < 0 || session->get_state() == AI_AGENT_STATE_STREAMING || session->get_state() == AI_AGENT_STATE_PREPARING_CONTEXT);
	}
}

void AIAgentDock::_select_current_session() {
	if (!session_selector || !session) {
		return;
	}

	String current_id = session->get_session_id();
	for (int i = 0; i < session_selector->get_item_count(); i++) {
		if (String(session_selector->get_item_metadata(i)) == current_id) {
			session_selector->select(i);
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
	message_list->scroll_to_bottom();
	_refresh_token_usage();
}

void AIAgentDock::_refresh_token_usage() {
	if (!token_usage_label || !session) {
		return;
	}

	Dictionary usage = session->get_token_usage();
	const int prompt_tokens = (int)usage.get("prompt_tokens", 0);
	const int completion_tokens = (int)usage.get("completion_tokens", 0);
	const int total_tokens = (int)usage.get("total_tokens", 0);
	const int estimated_input_tokens = (int)usage.get("estimated_input_tokens", 0);

	String text = vformat(TTR("Tokens  In %s  Out %s  Total %s"), _format_token_count(prompt_tokens), _format_token_count(completion_tokens), _format_token_count(total_tokens));
	if (estimated_input_tokens > 0) {
		text += vformat(TTR("  Context ~%s"), _format_token_count(estimated_input_tokens));
	}
	token_usage_label->set_text(text);
}

String AIAgentDock::_format_token_count(int p_tokens) const {
	if (p_tokens >= 1000000) {
		return rtos((double)p_tokens / 1000000.0).pad_decimals(1) + "M";
	}
	if (p_tokens >= 1000) {
		return rtos((double)p_tokens / 1000.0).pad_decimals(1) + "k";
	}
	return itos(MAX(0, p_tokens));
}

AIProviderConfig AIAgentDock::_get_provider_config(const String &p_model) const {
	return AIModelSettings::get_provider_config(p_model);
}
