/**************************************************************************/
/*  editor_user_avatar.cpp                                                */
/**************************************************************************/

#include "editor_user_avatar.h"

#include "core/object/callable_mp.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/separator.h"
#include "servers/text/text_server.h"

namespace {

const char *DETAIL_ACTION_REFRESH = "refresh";
const char *DETAIL_ACTION_LOGOUT = "logout";

String _detail_value(const String &p_value) {
	const String value = p_value.strip_edges();
	return value.is_empty() ? String("--") : value;
}

Label *_add_detail_value(GridContainer *p_grid, const String &p_label) {
	Label *name = memnew(Label);
	name->set_text(p_label);
	name->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	p_grid->add_child(name);

	Label *value = memnew(Label);
	value->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	value->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	value->set_clip_text(true);
	value->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	value->set_custom_minimum_size(Size2(180, 0) * EDSCALE);
	p_grid->add_child(value);
	return value;
}

} // namespace

void EditorUserAvatar::_bind_methods() {
}

void EditorUserAvatar::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build_ui();
		if (manager.is_valid()) {
			callable_mp(manager.ptr(), &EditorUserManager::initialize).call_deferred();
		}
	} else if (p_what == NOTIFICATION_THEME_CHANGED) {
		_update_account_icon();
	}
}

void EditorUserAvatar::_build_ui() {
	if (account_button) {
		return;
	}

	set_mouse_filter(Control::MOUSE_FILTER_STOP);
	add_theme_constant_override("separation", 6 * EDSCALE);

	if (manager.is_null()) {
		manager.instantiate();
	}

	account_button = memnew(Button);
	account_button->set_flat(true);
	account_button->set_theme_type_variation("FlatMenuButton");
	account_button->set_custom_minimum_size(Size2(44, 32) * EDSCALE);
	account_button->set_expand_icon(true);
	account_button->set_icon_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	account_button->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	account_button->set_accessibility_name(TTRC("User Account"));
	account_button->connect(SceneStringName(pressed), callable_mp(this, &EditorUserAvatar::_account_pressed));
	add_child(account_button);
	_update_account_icon();

	account_label = memnew(Label);
	account_label->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	account_label->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	account_label->set_clip_text(true);
	account_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	account_label->set_custom_minimum_size(Size2(128, 0) * EDSCALE);
	account_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	add_child(account_label);

	score_label = memnew(Label);
	score_label->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	score_label->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	score_label->set_clip_text(true);
	score_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	score_label->set_custom_minimum_size(Size2(96, 0) * EDSCALE);
	score_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	add_child(score_label);

	login_dialog = memnew(EditorUserLoginDialog);
	login_dialog->set_manager(manager);
	add_child(login_dialog);

	details_dialog = memnew(AcceptDialog);
	details_dialog->set_title(TTR("Account"));
	details_dialog->set_ok_button_text(TTR("Close"));
	details_dialog->connect("custom_action", callable_mp(this, &EditorUserAvatar::_details_action));
	add_child(details_dialog);

	MarginContainer *details_margin = memnew(MarginContainer);
	details_margin->add_theme_constant_override("margin_left", 12 * EDSCALE);
	details_margin->add_theme_constant_override("margin_right", 12 * EDSCALE);
	details_margin->add_theme_constant_override("margin_top", 12 * EDSCALE);
	details_margin->add_theme_constant_override("margin_bottom", 8 * EDSCALE);
	details_dialog->add_child(details_margin);

	VBoxContainer *details_content = memnew(VBoxContainer);
	details_content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	details_content->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	details_content->add_theme_constant_override("separation", 8 * EDSCALE);
	details_margin->add_child(details_content);

	GridContainer *details_grid = memnew(GridContainer);
	details_grid->set_columns(2);
	details_grid->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	details_grid->add_theme_constant_override("h_separation", 12 * EDSCALE);
	details_grid->add_theme_constant_override("v_separation", 8 * EDSCALE);
	details_content->add_child(details_grid);

	details_name_value = _add_detail_value(details_grid, TTR("Name"));
	details_phone_value = _add_detail_value(details_grid, TTR("Phone"));
	details_user_id_value = _add_detail_value(details_grid, TTR("User ID"));
	details_score_value = _add_detail_value(details_grid, TTR("Score"));

	HSeparator *details_separator = memnew(HSeparator);
	details_content->add_child(details_separator);

	details_dialog->add_button(TTR("Refresh"), false, DETAIL_ACTION_REFRESH);
	details_dialog->add_button(TTR("Sign Out"), false, DETAIL_ACTION_LOGOUT);

	manager->connect("state_changed", callable_mp(this, &EditorUserAvatar::_state_changed));
	manager->connect("profile_changed", callable_mp(this, &EditorUserAvatar::_profile_changed));
	manager->connect("request_failed", callable_mp(this, &EditorUserAvatar::_request_failed));

	_refresh_ui();
}

void EditorUserAvatar::_refresh_ui() {
	if (!account_button || manager.is_null()) {
		return;
	}

	const AuthSessionData session = manager->get_session();
	const AuthUserInfo user = manager->get_user_info();
	const String display_name = get_display_name(user, session);
	const bool logged_in = _is_logged_in();

	account_button->set_text(String());
	account_button->set_custom_minimum_size(Size2(44, 32) * EDSCALE);
	account_button->set_tooltip_text(logged_in ? display_name : TTR("Account"));
	if (account_label) {
		account_label->set_text(logged_in ? display_name : String());
		account_label->set_visible(logged_in);
	}
	if (score_label) {
		score_label->set_text(logged_in ? format_score(user.score) : String());
		score_label->set_tooltip_text(TTR("Account score"));
		score_label->set_visible(logged_in);
	}

	_refresh_details_ui();
}

void EditorUserAvatar::_account_pressed() {
	if (_is_logged_in()) {
		_show_details();
		return;
	}

	if (login_dialog) {
		login_dialog->popup_login();
	}
}

void EditorUserAvatar::_details_action(const String &p_action) {
	if (manager.is_null()) {
		return;
	}

	if (p_action == DETAIL_ACTION_REFRESH) {
		(void)manager->refresh_profile();
	} else if (p_action == DETAIL_ACTION_LOGOUT) {
		if (details_dialog) {
			details_dialog->hide();
		}
		(void)manager->logout();
	}
}

void EditorUserAvatar::_refresh_details_ui() {
	if (!details_dialog) {
		return;
	}

	const AuthSessionData session = manager.is_valid() ? manager->get_session() : AuthSessionData();
	const AuthUserInfo user = manager.is_valid() ? manager->get_user_info() : AuthUserInfo();

	if (details_name_value) {
		details_name_value->set_text(_detail_value(get_display_name(user, session)));
	}
	if (details_phone_value) {
		details_phone_value->set_text(_detail_value(!user.phone.strip_edges().is_empty() ? user.phone : session.phone));
	}
	if (details_user_id_value) {
		details_user_id_value->set_text(_detail_value(!user.user_id.strip_edges().is_empty() ? user.user_id : session.user_id));
	}
	if (details_score_value) {
		details_score_value->set_text(_detail_value(user.score));
	}
}

void EditorUserAvatar::_show_details() {
	_refresh_details_ui();
	if (details_dialog) {
		details_dialog->popup_centered(Size2(360, 220) * EDSCALE);
	}
}

void EditorUserAvatar::_update_account_icon() {
	if (account_button && account_button->is_inside_tree()) {
		account_button->set_button_icon(account_button->get_theme_icon(SNAME("User"), EditorStringName(EditorIcons)));
	}
}

bool EditorUserAvatar::_is_logged_in() const {
	if (manager.is_null()) {
		return false;
	}

	return manager->get_state() == EditorUserManager::STATE_LOGGED_IN || manager->get_state() == EditorUserManager::STATE_PROFILE_UNAVAILABLE;
}

void EditorUserAvatar::_state_changed(int p_state) {
	(void)p_state;
	_refresh_ui();
}

void EditorUserAvatar::_profile_changed() {
	_refresh_ui();
}

void EditorUserAvatar::_request_failed(const String &p_message) {
	if (account_button && !p_message.is_empty()) {
		account_button->set_tooltip_text(p_message);
	}
}

String EditorUserAvatar::get_display_name(const AuthUserInfo &p_user, const AuthSessionData &p_session) {
	if (!p_user.nickname.strip_edges().is_empty()) {
		return p_user.nickname.strip_edges();
	}
	if (!p_user.phone.strip_edges().is_empty()) {
		return p_user.phone.strip_edges();
	}
	if (!p_session.phone.strip_edges().is_empty()) {
		return p_session.phone.strip_edges();
	}
	return p_session.user_id.strip_edges();
}

String EditorUserAvatar::get_avatar_initial(const String &p_display_name) {
	const String display_name = p_display_name.strip_edges();
	if (display_name.is_empty()) {
		return "?";
	}
	return String::chr(display_name.unicode_at(0)).to_upper();
}

String EditorUserAvatar::format_score(const String &p_score) {
	const String score = p_score.strip_edges();
	return vformat(TTR("Score %s"), score.is_empty() ? String("--") : score);
}

String EditorUserAvatar::get_display_name_for_test(const AuthUserInfo &p_user, const AuthSessionData &p_session) {
	return get_display_name(p_user, p_session);
}

String EditorUserAvatar::get_avatar_initial_for_test(const String &p_display_name) {
	return get_avatar_initial(p_display_name);
}

String EditorUserAvatar::format_score_for_test(const String &p_score) {
	return format_score(p_score);
}

void EditorUserAvatar::set_manager_for_test(const Ref<EditorUserManager> &p_manager) {
	manager = p_manager;
	if (login_dialog) {
		login_dialog->set_manager(manager);
	}
	_refresh_ui();
}

void EditorUserAvatar::build_for_test() {
	_build_ui();
}

EditorUserAvatar::EditorUserAvatar() {
	set_h_size_flags(Control::SIZE_SHRINK_END);
	set_v_size_flags(Control::SIZE_SHRINK_CENTER);
}
