/**************************************************************************/
/*  ai_agent_dock.cpp                                                      */
/**************************************************************************/

#include "ai_agent_dock.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/io/json.h"
#include "core/io/image.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "editor/ai_component/agent/ai_mcp_service.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/ai_component/skills/ai_skill_settings.h"
#include "editor/ai_component/ui/ai_agent_next_dock.h"
#include "editor/ai_component/ui/ai_agent_settings_dialog.h"
#include "editor/gui/editor_toaster.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/item_list.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/popup.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"
#include "scene/resources/texture.h"
#include "servers/text/text_server.h"

namespace {

String _mcp_status_label(const String &p_status) {
	if (p_status == "ok") {
		return TTR("Ready");
	}
	if (p_status == "failed") {
		return TTR("Failed");
	}
	if (p_status == "disabled") {
		return TTR("Disabled");
	}
	return TTR("Checking");
}

Color _status_dot_color(const String &p_status) {
	if (p_status == "ok" || p_status == "enabled") {
		return Color(0.24, 0.78, 0.43);
	}
	if (p_status == "failed") {
		return Color(0.92, 0.25, 0.25);
	}
	return Color(0.96, 0.69, 0.20);
}

Ref<Texture2D> _make_status_dot_icon(const Color &p_color) {
	const int size = MAX(12, int(12 * EDSCALE));
	const real_t radius = size * 0.34;
	const Vector2 center((size - 1) * 0.5, (size - 1) * 0.5);

	Ref<Image> image = Image::create_empty(size, size, false, Image::FORMAT_RGBA8);
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			const real_t distance = center.distance_to(Vector2(x, y));
			const real_t alpha = CLAMP(radius + 0.75 - distance, 0.0, 1.0);
			Color pixel = p_color;
			pixel.a = alpha;
			image->set_pixel(x, y, pixel);
		}
	}
	return ImageTexture::create_from_image(image);
}

void _setup_status_item_list(ItemList *p_list) {
	ERR_FAIL_NULL(p_list);
	p_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	p_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	p_list->set_theme_type_variation("ItemListSecondary");
	p_list->set_select_mode(ItemList::SELECT_SINGLE);
	p_list->set_icon_mode(ItemList::ICON_MODE_LEFT);
	p_list->set_fixed_icon_size(Size2i(14, 14) * EDSCALE);
	p_list->set_max_columns(1);
	p_list->set_same_column_width(true);
	p_list->set_auto_height(false);
	p_list->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	p_list->set_allow_search(false);
}

} // namespace

void AIAgentDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_next_mode_enabled", "enabled"), &AIAgentDock::set_next_mode_enabled);
	ClassDB::bind_method(D_METHOD("is_next_mode_enabled"), &AIAgentDock::is_next_mode_enabled);
}

void AIAgentDock::_notification(int p_what) {
	EditorDock::_notification(p_what);

	if (p_what == NOTIFICATION_THEME_CHANGED) {
		if (new_session_button) {
			new_session_button->set_button_icon(get_editor_theme_icon(SNAME("Add")));
		}
		if (delete_session_button) {
			delete_session_button->set_button_icon(get_editor_theme_icon(SNAME("Delete")));
		}
		if (mcp_status_button) {
			mcp_status_button->set_button_icon(get_editor_theme_icon(SNAME("AIMCP")));
		}
		if (skill_status_button) {
			skill_status_button->set_button_icon(get_editor_theme_icon(SNAME("AISkill")));
		}
		_refresh_mcp_status_button();
		_refresh_skill_status_button();
	}
}

AIAgentDock::AIAgentDock() {
	singleton = this;

	set_name(TTRC("AI Agent"));
	set_layout_key("AI Agent");
	set_icon_name("EditorPlugin");
	set_dock_shortcut(ED_SHORTCUT_AND_COMMAND("docks/open_ai_agent", TTRC("Open AI Agent Dock")));
	set_default_slot(EditorDock::DOCK_SLOT_RIGHT_UL);

	chat_root = memnew(VBoxContainer);
	chat_root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	chat_root->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	chat_root->add_theme_constant_override("separation", 8 * EDSCALE);
	add_child(chat_root);

	HBoxContainer *session_bar = memnew(HBoxContainer);
	session_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	chat_root->add_child(session_bar);

	session_selector = memnew(OptionButton);
	session_selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	session_selector->set_custom_minimum_size(Size2(96, 0) * EDSCALE);
	session_selector->set_fit_to_longest_item(false);
	session_selector->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	session_selector->connect(SceneStringName(item_selected), callable_mp(this, &AIAgentDock::_session_selected));
	session_bar->add_child(session_selector);

	new_session_button = memnew(Button);
	new_session_button->set_button_icon(get_editor_theme_icon(SNAME("Add")));
	new_session_button->set_tooltip_text(TTR("Start a new AI chat session."));
	new_session_button->connect(SceneStringName(pressed), callable_mp(this, &AIAgentDock::_new_session_pressed));
	session_bar->add_child(new_session_button);

	delete_session_button = memnew(Button);
	delete_session_button->set_button_icon(get_editor_theme_icon(SNAME("Delete")));
	delete_session_button->set_tooltip_text(TTR("Delete the selected AI chat session."));
	delete_session_button->connect(SceneStringName(pressed), callable_mp(this, &AIAgentDock::_delete_session_pressed));
	session_bar->add_child(delete_session_button);

	mcp_status_button = memnew(Button);
	mcp_status_button->set_button_icon(get_editor_theme_icon(SNAME("AIMCP")));
	mcp_status_button->set_tooltip_text(TTR("MCP server status."));
	mcp_status_button->connect(SceneStringName(pressed), callable_mp(this, &AIAgentDock::_mcp_status_pressed));
	session_bar->add_child(mcp_status_button);

	skill_status_button = memnew(Button);
	skill_status_button->set_button_icon(get_editor_theme_icon(SNAME("AISkill")));
	skill_status_button->set_tooltip_text(TTR("AgentSkill status."));
	skill_status_button->connect(SceneStringName(pressed), callable_mp(this, &AIAgentDock::_skill_status_pressed));
	session_bar->add_child(skill_status_button);

	delete_session_dialog = memnew(ConfirmationDialog);
	delete_session_dialog->set_title(TTR("Delete AI Chat"));
	delete_session_dialog->set_ok_button_text(TTR("Delete"));
	delete_session_dialog->set_cancel_button_text(TTR("Cancel"));
	delete_session_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AIAgentDock::_confirm_delete_session));
	add_child(delete_session_dialog);

	tool_approval_dialog = memnew(ConfirmationDialog);
	tool_approval_dialog->set_title(TTR("Approve AI Tool"));
	tool_approval_dialog->set_ok_button_text(TTR("Approve"));
	tool_approval_dialog->set_cancel_button_text(TTR("Reject"));
	tool_approval_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AIAgentDock::_confirm_tool_approval));
	tool_approval_dialog->connect("canceled", callable_mp(this, &AIAgentDock::_reject_tool_approval));
	add_child(tool_approval_dialog);

	mcp_status_popup = memnew(PopupPanel);
	mcp_status_popup->set_min_size(Size2(380, 220) * EDSCALE);
	MarginContainer *mcp_status_margin = memnew(MarginContainer);
	mcp_status_margin->add_theme_constant_override("margin_right", 8 * EDSCALE);
	mcp_status_margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
	mcp_status_margin->add_theme_constant_override("margin_left", 8 * EDSCALE);
	mcp_status_margin->add_theme_constant_override("margin_bottom", 8 * EDSCALE);
	mcp_status_popup->add_child(mcp_status_margin);

	mcp_status_list = memnew(ItemList);
	_setup_status_item_list(mcp_status_list);
	mcp_status_margin->add_child(mcp_status_list);
	add_child(mcp_status_popup);

	skill_status_popup = memnew(PopupPanel);
	skill_status_popup->set_min_size(Size2(380, 220) * EDSCALE);
	MarginContainer *skill_status_margin = memnew(MarginContainer);
	skill_status_margin->add_theme_constant_override("margin_right", 8 * EDSCALE);
	skill_status_margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
	skill_status_margin->add_theme_constant_override("margin_left", 8 * EDSCALE);
	skill_status_margin->add_theme_constant_override("margin_bottom", 8 * EDSCALE);
	skill_status_popup->add_child(skill_status_margin);

	skill_status_list = memnew(ItemList);
	_setup_status_item_list(skill_status_list);
	skill_status_margin->add_child(skill_status_list);
	add_child(skill_status_popup);

	VBoxContainer *main = memnew(VBoxContainer);
	main->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main->add_theme_constant_override("separation", 8 * EDSCALE);
	chat_root->add_child(main);

	change_review_panel = memnew(AIChangeReviewPanel);
	main->add_child(change_review_panel);

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
		AIAgentSettingsDialog::get_singleton()->connect("ai_next_settings_changed", callable_mp(this, &AIAgentDock::_settings_changed));
		AIAgentSettingsDialog::get_singleton()->connect("ai_mcp_settings_changed", callable_mp(this, &AIAgentDock::_mcp_settings_changed));
		AIAgentSettingsDialog::get_singleton()->connect("ai_skill_settings_changed", callable_mp(this, &AIAgentDock::_skill_settings_changed));
	}
	Ref<AIMCPService> mcp_service = AIMCPService::get_singleton();
	if (mcp_service.is_valid()) {
		mcp_service->connect("status_changed", callable_mp(this, &AIAgentDock::_mcp_status_changed), CONNECT_DEFERRED);
		mcp_service->refresh();
	}

	_ensure_session();
	_reload_messages_from_session();
	_refresh_session_list();
	_refresh_mcp_status_button();
	_refresh_skill_status_button();

	next_dock = memnew(AIAgentNextDock);
	next_dock->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	next_dock->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	next_dock->hide();
	add_child(next_dock);
}

AIAgentDock *AIAgentDock::get_singleton() {
	return singleton;
}

void AIAgentDock::set_next_mode_enabled(bool p_enabled) {
	next_mode_enabled = p_enabled;
	if (chat_root) {
		chat_root->set_visible(!p_enabled);
	}
	if (next_dock) {
		next_dock->set_visible(p_enabled);
	}
	if (p_enabled) {
		make_visible();
	}
}

bool AIAgentDock::is_next_mode_enabled() const {
	return next_mode_enabled;
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
	const bool waiting_approval = p_state == AI_AGENT_STATE_WAITING_TOOL_APPROVAL;
	composer->set_running(running || waiting_approval);
	if (new_session_button) {
		new_session_button->set_disabled(running || waiting_approval);
	}
	if (delete_session_button) {
		delete_session_button->set_disabled(running || waiting_approval || !session_selector || session_selector->get_item_count() == 0 || session_selector->get_selected() < 0);
	}
	if (request_progress) {
		request_progress->set_visible(running);
	}
	if (request_status_row) {
		request_status_row->set_visible(running);
	}
	if (p_state == AI_AGENT_STATE_IDLE || p_state == AI_AGENT_STATE_FAILED || p_state == AI_AGENT_STATE_CANCELLED || p_state == AI_AGENT_STATE_WAITING_TOOL_APPROVAL) {
		_refresh_session_list();
	}
	_refresh_token_usage();
}

void AIAgentDock::_token_usage_changed(const Dictionary &p_usage) {
	(void)p_usage;
	_refresh_token_usage();
}

void AIAgentDock::_mcp_status_changed(const Array &p_statuses, const Dictionary &p_summary) {
	(void)p_statuses;
	const int failed = (int)p_summary.get("failed", 0);
	if (failed > 0 && !mcp_failure_toast_visible && EditorToaster::get_singleton()) {
		EditorToaster::get_singleton()->popup_str(vformat(TTR("%d MCP server(s) failed to initialize. Open the MCP status menu for details."), failed), EditorToaster::SEVERITY_WARNING);
		mcp_failure_toast_visible = true;
	} else if (failed == 0) {
		mcp_failure_toast_visible = false;
	}
	_refresh_mcp_status_button();
	if (mcp_status_popup && mcp_status_popup->is_visible()) {
		_refresh_mcp_status_popup();
	}
}

void AIAgentDock::_settings_changed() {
	composer->reload_models();
	if (next_dock) {
		next_dock->apply_agent_model_settings();
	}
}

void AIAgentDock::_mcp_settings_changed() {
	Ref<AIMCPService> mcp_service = AIMCPService::get_singleton();
	if (mcp_service.is_valid()) {
		mcp_service->refresh();
	}
	_refresh_mcp_status_button();
}

void AIAgentDock::_skill_settings_changed() {
	_refresh_skill_status_button();
	if (skill_status_popup && skill_status_popup->is_visible()) {
		_refresh_skill_status_popup();
	}
}

void AIAgentDock::_mcp_status_pressed() {
	_refresh_mcp_status_popup();
	if (mcp_status_popup) {
		const Size2 popup_size = Size2(420, 300) * EDSCALE;
		Rect2i popup_rect;
		if (mcp_status_button) {
			Rect2 rect = mcp_status_button->get_screen_rect();
			rect.position.y += rect.size.height;
			rect.size = popup_size;
			popup_rect = Rect2i(rect);
		}
		mcp_status_popup->popup(popup_rect);
	}
}

void AIAgentDock::_skill_status_pressed() {
	_refresh_skill_status_popup();
	if (skill_status_popup) {
		const Size2 popup_size = Size2(420, 300) * EDSCALE;
		Rect2i popup_rect;
		if (skill_status_button) {
			Rect2 rect = skill_status_button->get_screen_rect();
			rect.position.y += rect.size.height;
			rect.size = popup_size;
			popup_rect = Rect2i(rect);
		}
		skill_status_popup->popup(popup_rect);
	}
}

void AIAgentDock::_tool_approval_requested(const Dictionary &p_approval) {
	pending_tool_approval = p_approval.duplicate(true);
	if (!tool_approval_dialog) {
		return;
	}

	const String tool_name = String(pending_tool_approval.get("tool_name", ""));
	const String reason = String(pending_tool_approval.get("reason", ""));
	String text = vformat(TTR("Approve AI tool call `%s`?"), tool_name.is_empty() ? String("unknown") : tool_name);
	if (!reason.is_empty()) {
		text += "\n\n" + reason;
	}
	if (pending_tool_approval.has("arguments") && Variant(pending_tool_approval["arguments"]).get_type() == Variant::DICTIONARY) {
		text += "\n\n" + TTR("Arguments:") + "\n" + JSON::stringify(pending_tool_approval["arguments"], "\t");
	}

	tool_approval_dialog->set_text(text);
	tool_approval_dialog->popup_centered();
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

	if (session->get_state() == AI_AGENT_STATE_STREAMING || session->get_state() == AI_AGENT_STATE_PREPARING_CONTEXT || session->get_state() == AI_AGENT_STATE_WAITING_TOOL_APPROVAL) {
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

void AIAgentDock::_confirm_tool_approval() {
	if (!session) {
		pending_tool_approval.clear();
		return;
	}

	pending_tool_approval.clear();
	session->approve_pending_tool();
}

void AIAgentDock::_reject_tool_approval() {
	if (!session) {
		pending_tool_approval.clear();
		return;
	}
	if (pending_tool_approval.is_empty() && session->get_pending_tool_approval().is_empty()) {
		return;
	}

	pending_tool_approval.clear();
	session->reject_pending_tool();
}

void AIAgentDock::_session_selected(int p_index) {
	_ensure_session();
	ERR_FAIL_NULL(session);

	if (!session_selector || p_index < 0) {
		return;
	}
	if (session->get_state() == AI_AGENT_STATE_STREAMING || session->get_state() == AI_AGENT_STATE_PREPARING_CONTEXT || session->get_state() == AI_AGENT_STATE_WAITING_TOOL_APPROVAL) {
		_select_current_session();
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
	session->connect("tool_approval_requested", callable_mp(this, &AIAgentDock::_tool_approval_requested));
	Ref<AIMCPService> mcp_service = AIMCPService::get_singleton();
	if (mcp_service.is_valid()) {
		_mcp_status_changed(mcp_service->get_statuses(), mcp_service->get_status_summary());
	}
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
		delete_session_button->set_disabled(session_selector->get_item_count() == 0 || session_selector->get_selected() < 0 || session->get_state() == AI_AGENT_STATE_STREAMING || session->get_state() == AI_AGENT_STATE_PREPARING_CONTEXT || session->get_state() == AI_AGENT_STATE_WAITING_TOOL_APPROVAL);
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

void AIAgentDock::_refresh_mcp_status_button() {
	if (!mcp_status_button) {
		return;
	}

	Dictionary summary;
	Ref<AIMCPService> mcp_service = AIMCPService::get_singleton();
	if (mcp_service.is_valid()) {
		summary = mcp_service->get_status_summary();
	}
	const int total = (int)summary.get("total", 0);
	const int ok = (int)summary.get("ok", 0);
	const int failed = (int)summary.get("failed", 0);
	const int disabled = (int)summary.get("disabled", 0);
	const int checking = (int)summary.get("checking", 0);
	const int tool_count = (int)summary.get("tool_count", 0);

	if (total <= 0) {
		mcp_status_button->set_button_icon(get_editor_theme_icon(SNAME("AIMCP")));
		mcp_status_button->set_tooltip_text(TTR("No MCP servers configured."));
		return;
	}

	if (failed > 0) {
		mcp_status_button->set_button_icon(get_editor_theme_icon(SNAME("AIMCP")));
		mcp_status_button->set_tooltip_text(vformat(TTR("MCP status: %d failed, %d ready, %d disabled. Tools: %d."), failed, ok, disabled, tool_count));
		return;
	}

	if (checking > 0) {
		mcp_status_button->set_button_icon(get_editor_theme_icon(SNAME("AIMCP")));
		mcp_status_button->set_tooltip_text(vformat(TTR("MCP status: checking %d server(s)."), checking));
		return;
	}

	mcp_status_button->set_button_icon(get_editor_theme_icon(SNAME("AIMCP")));
	mcp_status_button->set_tooltip_text(vformat(TTR("MCP status: %d ready, %d disabled. Tools: %d."), ok, disabled, tool_count));
}

void AIAgentDock::_refresh_mcp_status_popup() {
	if (!mcp_status_list) {
		return;
	}

	mcp_status_list->clear();

	Array statuses;
	Ref<AIMCPService> mcp_service = AIMCPService::get_singleton();
	if (mcp_service.is_valid()) {
		statuses = mcp_service->get_statuses();
	}
	if (statuses.is_empty()) {
		const int index = mcp_status_list->add_item(TTR("No MCP servers configured."), _make_status_dot_icon(_status_dot_color("disabled")), false);
		mcp_status_list->set_item_disabled(index, true);
		return;
	}

	for (int i = 0; i < statuses.size(); i++) {
		if (Variant(statuses[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary status = statuses[i];
		const String display_name = String(status.get("display_name", TTR("Unnamed MCP")));
		const String transport = String(status.get("transport", String()));
		const String state = String(status.get("status", String()));
		const int tool_count = (int)status.get("tool_count", 0);
		const String error = String(status.get("error", String()));
		const String endpoint = String(status.get("endpoint", String()));

		String tooltip = vformat(TTR("%s\nStatus: %s\nTransport: %s"), display_name, _mcp_status_label(state), transport);
		if (state == "ok") {
			tooltip += "\n" + vformat(TTR("Tools: %d"), tool_count);
		}
		if (!endpoint.is_empty()) {
			tooltip += "\n" + endpoint;
		}
		if (!error.is_empty()) {
			tooltip += "\n" + error;
		}

		const int index = mcp_status_list->add_item(display_name, _make_status_dot_icon(_status_dot_color(state)), false);
		mcp_status_list->set_item_tooltip(index, tooltip);
	}
}

void AIAgentDock::_refresh_skill_status_button() {
	if (!skill_status_button) {
		return;
	}

	const Vector<AISkillConfig> skills = AISkillSettings::get_skills(false);
	skill_status_button->set_button_icon(get_editor_theme_icon(SNAME("AISkill")));
	if (skills.is_empty()) {
		skill_status_button->set_tooltip_text(TTR("No AgentSkills configured."));
		return;
	}

	int enabled_count = 0;
	for (int i = 0; i < skills.size(); i++) {
		if (skills[i].enabled) {
			enabled_count++;
		}
	}
	skill_status_button->set_tooltip_text(vformat(TTR("AgentSkills: %d configured, %d enabled."), skills.size(), enabled_count));
}

void AIAgentDock::_refresh_skill_status_popup() {
	if (!skill_status_list) {
		return;
	}

	skill_status_list->clear();

	const Vector<AISkillConfig> skills = AISkillSettings::get_skills(false);
	if (skills.is_empty()) {
		const int index = skill_status_list->add_item(TTR("No AgentSkills configured."), _make_status_dot_icon(_status_dot_color("disabled")), false);
		skill_status_list->set_item_disabled(index, true);
		return;
	}

	for (int i = 0; i < skills.size(); i++) {
		const AISkillConfig &skill = skills[i];
		const String state = skill.enabled ? String("enabled") : String("disabled");
		String tooltip = vformat(TTR("%s\nStatus: %s\nKind: %s"), skill.display_name, skill.enabled ? TTR("Enabled") : TTR("Disabled"), skill.kind);
		if (!skill.description.is_empty()) {
			tooltip += "\n" + skill.description;
		}

		const int index = skill_status_list->add_item(skill.display_name, _make_status_dot_icon(_status_dot_color(state)), false);
		skill_status_list->set_item_tooltip(index, tooltip);
	}
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
