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
#include "scene/gui/texture_rect.h"
#include "scene/resources/style_box_flat.h"
#include "servers/text/text_server.h"

namespace {

const char *DETAIL_ACTION_REFRESH = "refresh";
const char *DETAIL_ACTION_LOGOUT = "logout";

String _detail_value(const String &p_value) {
	const String value = p_value.strip_edges();
	return value.is_empty() ? String("--") : value;
}

String _format_user_id_text(const String &p_user_id) {
	const String user_id = p_user_id.strip_edges();
	return vformat(TTR("ID %s"), user_id.is_empty() ? String("--") : user_id);
}

Ref<StyleBoxFlat> _make_avatar_style(const Color &p_color) {
	Ref<StyleBoxFlat> style = memnew(StyleBoxFlat);
	style->set_bg_color(p_color);
	style->set_corner_radius_all(14 * EDSCALE);
	style->set_content_margin_all(0);
	return style;
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
		_update_account_visual();
		_update_credits_icon();
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

	VSeparator *account_separator = memnew(VSeparator);
	account_separator->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	account_separator->set_custom_minimum_size(Size2(0, 22) * EDSCALE);
	add_child(account_separator);

	account_button = memnew(Button);
	account_button->set_flat(true);
	account_button->set_theme_type_variation("FlatMenuButton");
	account_button->set_custom_minimum_size(Size2(28, 28) * EDSCALE);
	account_button->set_expand_icon(true);
	account_button->set_icon_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	account_button->set_text_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	account_button->set_clip_text(true);
	account_button->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	account_button->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	account_button->set_accessibility_name(TTRC("User Account"));
	account_button->connect(SceneStringName(pressed), callable_mp(this, &EditorUserAvatar::_account_pressed));
	add_child(account_button);

	name_label = memnew(Label);
	name_label->set_clip_text(true);
	name_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	name_label->set_custom_minimum_size(Size2(72, 0) * EDSCALE);
	name_label->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	name_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	add_child(name_label);

	credits_icon = memnew(TextureRect);
	credits_icon->set_custom_minimum_size(Size2(16, 16) * EDSCALE);
	credits_icon->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	credits_icon->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	credits_icon->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	credits_icon->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	credits_icon->set_tooltip_text(TTR("Credits"));
	add_child(credits_icon);

	credits_label = memnew(Label);
	credits_label->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	credits_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	add_child(credits_label);
	_update_credits_icon();

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
	details_credits_value = _add_detail_value(details_grid, TTR("Credit"));

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
	const String nickname = get_display_name(user, session);
	const bool logged_in = _is_logged_in();

	if (name_label) {
		name_label->set_visible(logged_in);
		name_label->set_text(logged_in ? _detail_value(nickname) : String());
	}
	if (credits_icon) {
		credits_icon->set_visible(logged_in);
	}
	if (credits_label) {
		credits_label->set_visible(logged_in);
		credits_label->set_text(logged_in ? format_credits_value(user.credits) : String());
	}

	account_button->set_tooltip_text(logged_in && !nickname.is_empty() ? vformat("%s - %s", nickname, format_credits(user.credits)) : TTR("Account"));
	_update_account_visual();

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
		(void)manager->request_refresh_profile();
	} else if (p_action == DETAIL_ACTION_LOGOUT) {
		if (details_dialog) {
			details_dialog->hide();
		}
		(void)manager->request_logout();
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
	if (details_credits_value) {
		details_credits_value->set_text(_detail_value(user.credits));
	}
}

void EditorUserAvatar::_show_details() {
	_refresh_details_ui();
	if (details_dialog) {
		details_dialog->popup_centered(Size2(360, 220) * EDSCALE);
	}
}

void EditorUserAvatar::_update_account_visual() {
	if (!account_button) {
		return;
	}

	const AuthSessionData session = manager.is_valid() ? manager->get_session() : AuthSessionData();
	const AuthUserInfo user = manager.is_valid() ? manager->get_user_info() : AuthUserInfo();
	const String nickname = get_display_name(user, session);

	if (_is_logged_in()) {
		account_button->set_button_icon(Ref<Texture2D>());
		account_button->set_text(get_avatar_initial(nickname));
		account_button->set_flat(false);
		account_button->set_expand_icon(false);
		account_button->set_custom_minimum_size(Size2(28, 28) * EDSCALE);

		if (account_button->is_inside_tree()) {
			const Color accent = account_button->get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
			account_button->add_theme_style_override(SNAME("normal"), _make_avatar_style(accent));
			account_button->add_theme_style_override(SNAME("hover"), _make_avatar_style(accent.lerp(Color(1, 1, 1, accent.a), 0.12)));
			account_button->add_theme_style_override(SceneStringName(pressed), _make_avatar_style(accent.darkened(0.16)));
			account_button->add_theme_style_override("hover_pressed", _make_avatar_style(accent.darkened(0.10)));
			account_button->add_theme_color_override(SceneStringName(font_color), Color(1, 1, 1));
			account_button->add_theme_color_override(SNAME("font_hover_color"), Color(1, 1, 1));
			account_button->add_theme_color_override(SNAME("font_pressed_color"), Color(1, 1, 1));
			account_button->add_theme_color_override(SNAME("font_focus_color"), Color(1, 1, 1));
		}
		return;
	}

	account_button->set_text(String());
	account_button->set_flat(true);
	account_button->set_expand_icon(true);
	account_button->set_custom_minimum_size(Size2(28, 28) * EDSCALE);
	account_button->remove_theme_style_override(SNAME("normal"));
	account_button->remove_theme_style_override(SNAME("hover"));
	account_button->remove_theme_style_override(SceneStringName(pressed));
	account_button->remove_theme_style_override("hover_pressed");
	account_button->remove_theme_color_override(SceneStringName(font_color));
	account_button->remove_theme_color_override(SNAME("font_hover_color"));
	account_button->remove_theme_color_override(SNAME("font_pressed_color"));
	account_button->remove_theme_color_override(SNAME("font_focus_color"));
	if (account_button->is_inside_tree()) {
		account_button->set_button_icon(account_button->get_theme_icon(SNAME("User"), EditorStringName(EditorIcons)));
	}
}

void EditorUserAvatar::_update_credits_icon() {
	if (credits_icon && credits_icon->is_inside_tree()) {
		credits_icon->set_texture(credits_icon->get_theme_icon(SNAME("Credits"), EditorStringName(EditorIcons)));
	}
}

bool EditorUserAvatar::_is_logged_in() const {
	if (manager.is_null()) {
		return false;
	}

	const EditorUserManager::State state = manager->get_state();
	if (state == EditorUserManager::STATE_LOGGED_IN || state == EditorUserManager::STATE_PROFILE_UNAVAILABLE) {
		return true;
	}
	const AuthSessionData session = manager->get_session();
	return state == EditorUserManager::STATE_REFRESHING && (!session.user_id.is_empty() || !session.token.is_empty());
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
	(void)p_session;
	if (!p_user.nickname.strip_edges().is_empty()) {
		return p_user.nickname.strip_edges();
	}
	return String();
}

String EditorUserAvatar::get_user_id(const AuthUserInfo &p_user, const AuthSessionData &p_session) {
	if (!p_user.user_id.strip_edges().is_empty()) {
		return p_user.user_id.strip_edges();
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

String EditorUserAvatar::format_user_id(const String &p_user_id) {
	return _format_user_id_text(p_user_id);
}

String EditorUserAvatar::format_credits(const String &p_credits) {
	const String credits = p_credits.strip_edges();
	return vformat(TTR("Credits %s"), credits.is_empty() ? String("--") : credits);
}

String EditorUserAvatar::format_credits_value(const String &p_credits) {
	const String credits = p_credits.strip_edges();
	return credits.is_empty() ? String("--") : credits;
}

String EditorUserAvatar::get_display_name_for_test(const AuthUserInfo &p_user, const AuthSessionData &p_session) {
	return get_display_name(p_user, p_session);
}

String EditorUserAvatar::get_user_id_for_test(const AuthUserInfo &p_user, const AuthSessionData &p_session) {
	return get_user_id(p_user, p_session);
}

String EditorUserAvatar::get_avatar_initial_for_test(const String &p_display_name) {
	return get_avatar_initial(p_display_name);
}

String EditorUserAvatar::format_user_id_for_test(const String &p_user_id) {
	return format_user_id(p_user_id);
}

String EditorUserAvatar::format_credits_for_test(const String &p_credits) {
	return format_credits(p_credits);
}

String EditorUserAvatar::format_credits_value_for_test(const String &p_credits) {
	return format_credits_value(p_credits);
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
