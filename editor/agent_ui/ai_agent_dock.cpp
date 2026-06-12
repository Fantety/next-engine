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
#include "editor/agent_ui/ai_agent_settings_dialog.h"
#include "editor/agent_ui/component/ai_next_marquee_settings.h"
#include "editor/agent_ui/component/ai_requirement_form_dialog.h"
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
	if (p_status == "ok" || p_status == "ready") {
		return TTR("Ready");
	}
	if (p_status == "failed") {
		return TTR("Failed");
	}
	if (p_status == "disabled") {
		return TTR("Disabled");
	}
	if (p_status == "permission_pending") {
		return TTR("Waiting Approval");
	}
	return TTR("Checking");
}

Color _status_dot_color(const String &p_status) {
	if (p_status == "ok" || p_status == "ready" || p_status == "enabled") {
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

Ref<ShaderMaterial> _make_request_progress_material() {
	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(AINextMarqueeSettings::get_effective_shader_code());

	Ref<ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);
	return material;
}

Array _array_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::ARRAY) {
		return Array(p_value).duplicate(true);
	}
	return Array();
}

Dictionary _dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return Dictionary(p_value).duplicate(true);
	}
	return Dictionary();
}

Dictionary _settings_snapshot_from_bridge(const Ref<AIAgentV1UIBridge> &p_bridge) {
	if (p_bridge.is_null()) {
		return Dictionary();
	}
	return p_bridge->get_settings_snapshot();
}

int _mcp_total_tool_count(const Array &p_statuses, const Dictionary &p_summary) {
	int tool_count = int(p_summary.get("tool_count", 0));
	if (tool_count > 0) {
		return tool_count;
	}
	for (int i = 0; i < p_statuses.size(); i++) {
		if (p_statuses[i].get_type() == Variant::DICTIONARY) {
			tool_count += int(Dictionary(p_statuses[i]).get("tool_count", 0));
		}
	}
	return tool_count;
}

bool _is_requirement_form_tool_name(const String &p_tool_name) {
	const String tool_name = p_tool_name.strip_edges();
	return tool_name == "agent.collect_requirements" || tool_name == "agent_collect_requirements";
}

String _skill_kind(const Dictionary &p_skill) {
	String kind = String(p_skill.get("kind", String())).strip_edges();
	if (kind.is_empty()) {
		kind = p_skill.has("source") && !p_skill.has("entry") ? String("source") : String("manifest");
	}
	return kind;
}

String _skill_display_name(const Dictionary &p_skill) {
	if (_skill_kind(p_skill) == "source") {
		return String(p_skill.get("source", p_skill.get("id", TTR("Unnamed Skill Source")))).strip_edges();
	}
	const String name = String(p_skill.get("display_name", p_skill.get("name", p_skill.get("id", String())))).strip_edges();
	return name.is_empty() ? String(TTR("Unnamed AgentSkill")) : name;
}

String _skill_description(const Dictionary &p_skill) {
	if (_skill_kind(p_skill) == "source") {
		return TTR("Skill source root scanned by agent_v1.");
	}
	const String description = String(p_skill.get("description", String())).strip_edges();
	if (!description.is_empty()) {
		return description;
	}
	return String(p_skill.get("entry", String())).strip_edges();
}

} // namespace

void AIAgentDock::_bind_methods() {
}

void AIAgentDock::_notification(int p_what) {
	EditorDock::_notification(p_what);

	if (p_what == NOTIFICATION_EXIT_TREE || p_what == NOTIFICATION_PREDELETE) {
		_clear_request_progress_material();
	}

	if (p_what == NOTIFICATION_PREDELETE) {
		if (singleton == this) {
			singleton = nullptr;
		}
	}

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

	normal_panel = memnew(VBoxContainer);
	normal_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	normal_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	normal_panel->add_theme_constant_override("separation", 8 * EDSCALE);
	add_child(normal_panel);

	HBoxContainer *session_bar = memnew(HBoxContainer);
	session_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	normal_panel->add_child(session_bar);

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

	requirement_form_dialog = memnew(AIRequirementFormDialog);
	requirement_form_dialog->connect("form_submitted", callable_mp(this, &AIAgentDock::_requirement_form_submitted));
	requirement_form_dialog->connect("canceled", callable_mp(this, &AIAgentDock::_reject_tool_approval));
	add_child(requirement_form_dialog);

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
	normal_panel->add_child(main);

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

	request_progress = memnew(ColorRect);
	request_progress->set_color(Color(1, 1, 1, 1));
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
	composer->connect("agent_profile_selected", callable_mp(this, &AIAgentDock::_agent_profile_selected));
	composer->connect("cancel_requested", callable_mp(this, &AIAgentDock::_cancel_requested));

	if (AIAgentSettingsDialog::get_singleton()) {
		AIAgentSettingsDialog::get_singleton()->connect("ai_settings_changed", callable_mp(this, &AIAgentDock::_settings_changed));
		AIAgentSettingsDialog::get_singleton()->connect("ai_next_marquee_settings_changed", callable_mp(this, &AIAgentDock::_next_marquee_settings_changed));
		AIAgentSettingsDialog::get_singleton()->connect("ai_mcp_settings_changed", callable_mp(this, &AIAgentDock::_mcp_settings_changed));
		AIAgentSettingsDialog::get_singleton()->connect("ai_skill_settings_changed", callable_mp(this, &AIAgentDock::_skill_settings_changed));
	}

	_ensure_session();
	_mcp_settings_changed();
	_sync_composer_agent_profile();
	_reload_messages_from_session();
	_refresh_session_list();
	_refresh_mcp_status_button();
	_refresh_skill_status_button();
}

AIAgentDock::~AIAgentDock() {
	_clear_request_progress_material();
	if (singleton == this) {
		singleton = nullptr;
	}
}

AIAgentDock *AIAgentDock::get_singleton() {
	return singleton;
}

Ref<AIAgentV1UIBridge> AIAgentDock::_get_adapter() {
	if (bridge.is_null()) {
		bridge = AIAgentV1UIBridge::get_singleton();
	}
	return bridge;
}

String AIAgentDock::_normalize_agent_profile_id(const String &p_agent_profile_id) const {
	const String profile_id = p_agent_profile_id.strip_edges();
	if (profile_id.is_empty() || profile_id == "auto" || profile_id == "ask") {
		return "main";
	}
	return profile_id;
}

bool AIAgentDock::_is_run_busy() const {
	if (bridge.is_null()) {
		return false;
	}
	const Dictionary state = bridge->get_run_state();
	return bool(state.get("busy", false));
}

void AIAgentDock::_send_to_agent_v1(const String &p_message, const String &p_model, const String &p_agent_profile_id, const Array &p_attachments, bool p_resume) {
	_ensure_session();

	const Dictionary result = _get_adapter()->send_message(p_message, p_model, _normalize_agent_profile_id(p_agent_profile_id), p_attachments, p_resume);
	if (bool(result.get("success", false))) {
		if (composer) {
			composer->clear_input();
		}
		_reload_messages_from_session();
		_refresh_session_list();
	}
}

void AIAgentDock::_send_requested(const String &p_message, const String &p_model, const String &p_agent_profile_id, const Array &p_attachments) {
	_send_to_agent_v1(p_message, p_model, p_agent_profile_id, p_attachments, true);
}

void AIAgentDock::_agent_profile_selected(const String &p_agent_profile_id) {
	_ensure_session();
	if (_is_run_busy()) {
		return;
	}
	(void)p_agent_profile_id;
}

void AIAgentDock::_cancel_requested() {
	(void)_get_adapter()->cancel_active_run(TTR("User cancelled."));
}

void AIAgentDock::_sessions_changed(const Array &p_sessions) {
	(void)p_sessions;
	_refresh_session_list();
}

void AIAgentDock::_active_session_changed(const Dictionary &p_session) {
	(void)p_session;
	_reload_messages_from_session();
	_refresh_session_list();
}

void AIAgentDock::_messages_changed(const String &p_session_id, const Array &p_messages) {
	if (p_session_id != _get_adapter()->get_active_session_id()) {
		return;
	}
	if (message_list) {
		message_list->set_messages(p_messages);
		message_list->scroll_to_bottom();
	}
	_refresh_token_usage();
	_queue_refresh_session_list();
}

void AIAgentDock::_run_state_changed(const Dictionary &p_state) {
	const String state = String(p_state.get("state", String()));
	const bool running = bool(p_state.get("busy", false));
	const bool waiting_approval = state == "waiting_permission";
	if (composer) {
		composer->set_running(running || waiting_approval);
	}
	if (new_session_button) {
		new_session_button->set_disabled(running || waiting_approval);
	}
	if (delete_session_button) {
		delete_session_button->set_disabled(running || waiting_approval || !session_selector || session_selector->get_item_count() == 0 || session_selector->get_selected() < 0);
	}
	if (request_progress) {
		if (running) {
			_ensure_request_progress_material();
		} else {
			_clear_request_progress_material();
		}
		request_progress->set_visible(running);
	}
	if (request_status_row) {
		request_status_row->set_visible(running);
	}
	if (!running || waiting_approval) {
		_queue_refresh_session_list();
	}
	_refresh_token_usage();
}

void AIAgentDock::_permission_requested(const Dictionary &p_request) {
	_tool_approval_requested(p_request);
}

void AIAgentDock::_permission_resolved(const Dictionary &p_reply) {
	(void)p_reply;
	pending_tool_approval.clear();
	_reload_messages_from_session();
	_run_state_changed(_get_adapter()->get_run_state());
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

void AIAgentDock::_skill_status_changed(const Array &p_statuses, const Dictionary &p_summary) {
	(void)p_statuses;
	(void)p_summary;
	_refresh_skill_status_button();
	if (skill_status_popup && skill_status_popup->is_visible()) {
		_refresh_skill_status_popup();
	}
}

void AIAgentDock::_settings_changed() {
	composer->reload_models();
	_refresh_request_progress_material();
}

void AIAgentDock::_next_marquee_settings_changed() {
	_refresh_request_progress_material();
}

void AIAgentDock::_mcp_settings_changed() {
	Ref<AIAgentV1UIBridge> adapter = _get_adapter();
	if (adapter.is_valid()) {
		(void)adapter->refresh_mcp_status();
	}
	_refresh_mcp_status_button();
	if (mcp_status_popup && mcp_status_popup->is_visible()) {
		_refresh_mcp_status_popup();
	}
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

	Dictionary source;
	if (pending_tool_approval.get("source", Variant()).get_type() == Variant::DICTIONARY) {
		source = pending_tool_approval["source"];
	}
	const String tool_name = String(pending_tool_approval.get("tool_name", source.get("tool_name", source.get("tool", String()))));
	const String reason = String(pending_tool_approval.get("reason", ""));
	const Variant arguments = pending_tool_approval.get("arguments", pending_tool_approval.get("input", source.get("input", Variant())));
	if (_is_requirement_form_tool_name(tool_name)) {
		if (requirement_form_dialog && arguments.get_type() == Variant::DICTIONARY) {
			requirement_form_dialog->set_form(arguments);
			requirement_form_dialog->popup_centered_ratio(0.45);
		}
		return;
	}

	String text = vformat(TTR("Approve AI tool call `%s`?"), tool_name.is_empty() ? String("unknown") : tool_name);
	if (!reason.is_empty()) {
		text += "\n\n" + reason;
	}
	if (arguments.get_type() == Variant::DICTIONARY) {
		text += "\n\n" + TTR("Arguments:") + "\n" + JSON::stringify(arguments, "\t");
	}

	tool_approval_dialog->set_text(text);
	tool_approval_dialog->popup_centered();
}

void AIAgentDock::_new_session_pressed() {
	_ensure_session();

	Dictionary create;
	create["directory"] = "res://";
	create["agent_id"] = "main";
	create["title"] = TTR("New Chat");
	(void)_get_adapter()->create_session(create);
	_sync_composer_agent_profile();
	if (message_list) {
		message_list->clear_messages();
	}
	_refresh_token_usage();
	_refresh_session_list();
}

void AIAgentDock::_delete_session_pressed() {
	_ensure_session();

	const String session_id = _get_selected_session_id();
	if (session_id.is_empty()) {
		return;
	}

	if (_is_run_busy()) {
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

	const String session_id = pending_delete_session_id.strip_edges();
	pending_delete_session_id.clear();
	if (session_id.is_empty()) {
		return;
	}

	const Dictionary result = _get_adapter()->delete_session(session_id);
	if (bool(result.get("success", false))) {
		_reload_messages_from_session();
	}
	_refresh_session_list();
}

void AIAgentDock::_confirm_tool_approval() {
	const String request_id = String(pending_tool_approval.get("request_id", String()));
	if (!request_id.is_empty()) {
		(void)_get_adapter()->reply_permission(request_id, true);
	}
	pending_tool_approval.clear();
}

void AIAgentDock::_reject_tool_approval() {
	if (pending_tool_approval.is_empty()) {
		return;
	}

	const String request_id = String(pending_tool_approval.get("request_id", String()));
	if (!request_id.is_empty()) {
		(void)_get_adapter()->reply_permission(request_id, false, TTR("Rejected from UI."));
	}
	pending_tool_approval.clear();
}

void AIAgentDock::_requirement_form_submitted(const Dictionary &p_answers) {
	const String request_id = String(pending_tool_approval.get("request_id", String()));
	if (!request_id.is_empty()) {
		Dictionary options;
		options["answers"] = p_answers.duplicate(true);
		(void)_get_adapter()->reply_permission(request_id, true, String(), options);
	}
	pending_tool_approval.clear();
}

void AIAgentDock::_session_selected(int p_index) {
	_ensure_session();

	if (!session_selector || p_index < 0) {
		return;
	}
	if (_is_run_busy()) {
		_select_current_session();
		return;
	}

	String session_id = session_selector->get_item_metadata(p_index);
	if (session_id.is_empty() || session_id == _get_adapter()->get_active_session_id()) {
		return;
	}

	if (_get_adapter()->set_active_session(session_id)) {
		_reload_messages_from_session();
		_refresh_session_list();
	}
}

void AIAgentDock::_ensure_session() {
	Ref<AIAgentV1UIBridge> adapter = _get_adapter();
	if (adapter.is_valid()) {
		const Callable sessions_changed = callable_mp(this, &AIAgentDock::_sessions_changed);
		if (!adapter->is_connected(SNAME("sessions_changed"), sessions_changed)) {
			adapter->connect(SNAME("sessions_changed"), sessions_changed);
		}
		const Callable active_session_changed = callable_mp(this, &AIAgentDock::_active_session_changed);
		if (!adapter->is_connected(SNAME("active_session_changed"), active_session_changed)) {
			adapter->connect(SNAME("active_session_changed"), active_session_changed);
		}
		const Callable messages_changed = callable_mp(this, &AIAgentDock::_messages_changed);
		if (!adapter->is_connected(SNAME("messages_changed"), messages_changed)) {
			adapter->connect(SNAME("messages_changed"), messages_changed);
		}
		const Callable run_state_changed = callable_mp(this, &AIAgentDock::_run_state_changed);
		if (!adapter->is_connected(SNAME("run_state_changed"), run_state_changed)) {
			adapter->connect(SNAME("run_state_changed"), run_state_changed);
		}
		const Callable permission_requested = callable_mp(this, &AIAgentDock::_permission_requested);
		if (!adapter->is_connected(SNAME("permission_requested"), permission_requested)) {
			adapter->connect(SNAME("permission_requested"), permission_requested);
		}
		const Callable permission_resolved = callable_mp(this, &AIAgentDock::_permission_resolved);
		if (!adapter->is_connected(SNAME("permission_resolved"), permission_resolved)) {
			adapter->connect(SNAME("permission_resolved"), permission_resolved);
		}
		const Callable mcp_status_changed = callable_mp(this, &AIAgentDock::_mcp_status_changed);
		if (!adapter->is_connected(SNAME("mcp_status_changed"), mcp_status_changed)) {
			adapter->connect(SNAME("mcp_status_changed"), mcp_status_changed, CONNECT_DEFERRED);
		}
		const Callable skill_status_changed = callable_mp(this, &AIAgentDock::_skill_status_changed);
		if (!adapter->is_connected(SNAME("skill_status_changed"), skill_status_changed)) {
			adapter->connect(SNAME("skill_status_changed"), skill_status_changed, CONNECT_DEFERRED);
		}
		if (adapter->get_active_session_id().is_empty()) {
			(void)adapter->restore_active_session();
		}
		if (adapter->get_active_session_id().is_empty() && adapter->list_sessions().is_empty()) {
			Dictionary create;
			create["directory"] = "res://";
			create["agent_id"] = "main";
			create["title"] = TTR("New Chat");
			(void)adapter->create_session(create);
		}
		const Dictionary snapshot = adapter->get_settings_snapshot();
		_mcp_status_changed(_array_from_variant(snapshot.get("mcp_statuses", Array())), _dictionary_from_variant(snapshot.get("mcp_summary", Dictionary())));
		_skill_status_changed(_array_from_variant(snapshot.get("skills", Array())), Dictionary());
	}
}

void AIAgentDock::_sync_composer_agent_profile() {
	if (!composer) {
		return;
	}

	_agent_profile_selected(composer->get_selected_agent_profile_id());
}

String AIAgentDock::_get_selected_session_id() const {
	if (!session_selector || session_selector->get_selected() < 0) {
		return String();
	}

	return session_selector->get_item_metadata(session_selector->get_selected());
}

void AIAgentDock::_queue_refresh_session_list() {
	if (session_list_refresh_queued) {
		return;
	}

	session_list_refresh_queued = true;
	callable_mp(this, &AIAgentDock::_flush_session_list_refresh).call_deferred();
}

void AIAgentDock::_flush_session_list_refresh() {
	if (!session_list_refresh_queued) {
		return;
	}

	session_list_refresh_queued = false;
	_refresh_session_list();
}

void AIAgentDock::_refresh_session_list() {
	session_list_refresh_queued = false;
	if (!session_selector) {
		return;
	}

	session_selector->clear();
	Array sessions = _get_adapter()->list_sessions();
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
		const bool no_selection = !session_selector || session_selector->get_item_count() == 0 || session_selector->get_selected() < 0;
		delete_session_button->set_disabled(_is_run_busy() || no_selection);
	}
}

void AIAgentDock::_select_current_session() {
	if (!session_selector) {
		return;
	}

	String current_id = _get_adapter()->get_active_session_id();
	for (int i = 0; i < session_selector->get_item_count(); i++) {
		if (String(session_selector->get_item_metadata(i)) == current_id) {
			session_selector->select(i);
			return;
		}
	}
}

void AIAgentDock::_reload_messages_from_session() {
	ERR_FAIL_NULL(message_list);
	message_list->set_messages(_get_adapter()->get_messages());
	message_list->scroll_to_bottom();
	_refresh_token_usage();
}

void AIAgentDock::_refresh_mcp_status_button() {
	if (!mcp_status_button) {
		return;
	}

	const Dictionary snapshot = _settings_snapshot_from_bridge(_get_adapter());
	const Array statuses = _array_from_variant(snapshot.get("mcp_statuses", Array()));
	const Dictionary summary = _dictionary_from_variant(snapshot.get("mcp_summary", Dictionary()));
	const int total = (int)summary.get("total", 0);
	const int ok = (int)summary.get("ready", summary.get("ok", 0));
	const int failed = (int)summary.get("failed", 0);
	const int disabled = (int)summary.get("disabled", 0);
	const int checking = MAX(0, total - ok - failed - disabled);
	const int tool_count = _mcp_total_tool_count(statuses, summary);

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

	const Dictionary snapshot = _settings_snapshot_from_bridge(_get_adapter());
	Array statuses = _array_from_variant(snapshot.get("mcp_statuses", Array()));
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
		const String display_name = String(status.get("display_name", status.get("server_name", TTR("Unnamed MCP"))));
		const String transport = String(status.get("transport", status.get("mcp_transport", String())));
		const String state = String(status.get("status", status.get("state", String())));
		const int tool_count = (int)status.get("tool_count", 0);
		const String error = String(status.get("error", status.get("last_error", String())));
		const String endpoint = String(status.get("endpoint", String()));

		String tooltip = vformat(TTR("%s\nStatus: %s"), display_name, _mcp_status_label(state));
		if (!transport.is_empty()) {
			tooltip += "\n" + vformat(TTR("Transport: %s"), transport);
		}
		if (state == "ok" || state == "ready") {
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

	const Dictionary snapshot = _settings_snapshot_from_bridge(_get_adapter());
	const Array skills = _array_from_variant(snapshot.get("skills", Array()));
	skill_status_button->set_button_icon(get_editor_theme_icon(SNAME("AISkill")));
	if (skills.is_empty()) {
		skill_status_button->set_tooltip_text(TTR("No AgentSkills configured."));
		return;
	}

	int enabled_count = 0;
	for (int i = 0; i < skills.size(); i++) {
		if (skills[i].get_type() == Variant::DICTIONARY && bool(Dictionary(skills[i]).get("enabled", true))) {
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

	const Dictionary snapshot = _settings_snapshot_from_bridge(_get_adapter());
	const Array skills = _array_from_variant(snapshot.get("skills", Array()));
	if (skills.is_empty()) {
		const int index = skill_status_list->add_item(TTR("No AgentSkills configured."), _make_status_dot_icon(_status_dot_color("disabled")), false);
		skill_status_list->set_item_disabled(index, true);
		return;
	}

	for (int i = 0; i < skills.size(); i++) {
		if (skills[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary skill = skills[i];
		const bool skill_enabled = bool(skill.get("enabled", true));
		const String state = skill_enabled ? String("enabled") : String("disabled");
		const String display_name = _skill_display_name(skill);
		const String kind = _skill_kind(skill);
		const String description = _skill_description(skill);
		String tooltip = vformat(TTR("%s\nStatus: %s\nKind: %s"), display_name, skill_enabled ? TTR("Enabled") : TTR("Disabled"), kind);
		if (!description.is_empty()) {
			tooltip += "\n" + description;
		}

		const int index = skill_status_list->add_item(display_name, _make_status_dot_icon(_status_dot_color(state)), false);
		skill_status_list->set_item_tooltip(index, tooltip);
	}
}

void AIAgentDock::_refresh_token_usage() {
	if (!token_usage_label) {
		return;
	}

	token_usage_label->set_text(vformat(TTR("Tokens  In %s  Out %s  Total %s"), _format_token_count(0), _format_token_count(0), _format_token_count(0)));
}

void AIAgentDock::_ensure_request_progress_material() {
	if (!request_progress || request_progress->get_material().is_valid()) {
		return;
	}
	request_progress->set_material(_make_request_progress_material());
}

void AIAgentDock::_refresh_request_progress_material() {
	if (!request_progress || !request_progress->is_visible()) {
		return;
	}
	request_progress->set_material(_make_request_progress_material());
}

void AIAgentDock::_clear_request_progress_material() {
	if (!request_progress || request_progress->get_material().is_null()) {
		return;
	}
	request_progress->set_material(Ref<Material>());
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
