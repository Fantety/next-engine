/**************************************************************************/
/*  editor_user_login_dialog.cpp                                          */
/**************************************************************************/

#include "editor_user_login_dialog.h"

#include "core/object/callable_mp.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/separator.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/texture_rect.h"
#include "scene/main/timer.h"
#include "servers/text/text_server.h"

namespace {

enum CountryCode {
	COUNTRY_CHINA = 86,
	COUNTRY_US = 1,
	COUNTRY_JAPAN = 81,
	COUNTRY_KOREA = 82,
};

static const int PHONE_CODE_COOLDOWN_SECONDS = 30;

String _normalize_country_code(const String &p_country_code) {
	String country_code = p_country_code.strip_edges();
	if (country_code.is_empty()) {
		return "+86";
	}
	return country_code.begins_with("+") ? country_code : "+" + country_code;
}

String _format_phone(const String &p_country_code, const String &p_phone) {
	String phone = p_phone.strip_edges();
	if (phone.is_empty()) {
		return String();
	}
	if (phone.begins_with("+")) {
		return phone;
	}
	while (phone.begins_with("0")) {
		phone = phone.substr(1);
	}
	return _normalize_country_code(p_country_code) + phone;
}

void _configure_country_selector(OptionButton *p_selector) {
	p_selector->set_fit_to_longest_item(false);
	p_selector->set_custom_minimum_size(Size2(92, 0) * EDSCALE);
	p_selector->add_item("+86 CN", COUNTRY_CHINA);
	p_selector->add_item("+1 US", COUNTRY_US);
	p_selector->add_item("+81 JP", COUNTRY_JAPAN);
	p_selector->add_item("+82 KR", COUNTRY_KOREA);
	p_selector->select(0);
}

String _selected_country_code(const OptionButton *p_selector) {
	if (!p_selector) {
		return "+86";
	}
	const int id = p_selector->get_selected_id();
	if (id > 0) {
		return "+" + itos(id);
	}
	return "+86";
}

VBoxContainer *_make_tab_content(Control *p_page) {
	MarginContainer *margin = memnew(MarginContainer);
	margin->add_theme_constant_override("margin_left", 12 * EDSCALE);
	margin->add_theme_constant_override("margin_right", 12 * EDSCALE);
	margin->add_theme_constant_override("margin_top", 14 * EDSCALE);
	margin->add_theme_constant_override("margin_bottom", 10 * EDSCALE);
	p_page->add_child(margin);

	VBoxContainer *content = memnew(VBoxContainer);
	content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_theme_constant_override("separation", 10 * EDSCALE);
	margin->add_child(content);
	return content;
}

Label *_add_field_label(VBoxContainer *p_content, const String &p_text) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	p_content->add_child(label);
	return label;
}

} // namespace

void EditorUserLoginDialog::_bind_methods() {
}

void EditorUserLoginDialog::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build_ui();
		_update_theme();
	} else if (p_what == NOTIFICATION_THEME_CHANGED) {
		_update_theme();
	}
}

void EditorUserLoginDialog::_build_ui() {
	if (tabs) {
		return;
	}

	get_ok_button()->hide();

	MarginContainer *margin = memnew(MarginContainer);
	margin->add_theme_constant_override("margin_left", 18 * EDSCALE);
	margin->add_theme_constant_override("margin_right", 18 * EDSCALE);
	margin->add_theme_constant_override("margin_top", 16 * EDSCALE);
	margin->add_theme_constant_override("margin_bottom", 12 * EDSCALE);
	add_child(margin);

	VBoxContainer *main = memnew(VBoxContainer);
	main->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	main->add_theme_constant_override("separation", 12 * EDSCALE);
	margin->add_child(main);

	HBoxContainer *header = memnew(HBoxContainer);
	header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header->add_theme_constant_override("separation", 12 * EDSCALE);
	main->add_child(header);

	header_icon = memnew(TextureRect);
	header_icon->set_custom_minimum_size(Size2(36, 36) * EDSCALE);
	header_icon->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	header_icon->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	header_icon->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	header->add_child(header_icon);

	VBoxContainer *header_text = memnew(VBoxContainer);
	header_text->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header_text->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	header_text->add_theme_constant_override("separation", 2 * EDSCALE);
	header->add_child(header_text);

	title_label = memnew(Label);
	title_label->set_text(TTR("Account"));
	title_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title_label->set_clip_text(true);
	title_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	header_text->add_child(title_label);

	subtitle_label = memnew(Label);
	subtitle_label->set_text(TTR("Use your account to sync editor services and credits."));
	subtitle_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	subtitle_label->set_clip_text(true);
	subtitle_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	header_text->add_child(subtitle_label);

	HSeparator *header_separator = memnew(HSeparator);
	main->add_child(header_separator);

	tabs = memnew(TabContainer);
	tabs->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tabs->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	tabs->set_theme_type_variation("TabContainerInner");
	main->add_child(tabs);

	MarginContainer *phone_page = memnew(MarginContainer);
	phone_page->set_name(TTR("Phone Code"));
	tabs->add_child(phone_page);

	VBoxContainer *phone_content = _make_tab_content(phone_page);
	_add_field_label(phone_content, TTR("Phone"));
	HBoxContainer *phone_row = memnew(HBoxContainer);
	phone_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	phone_row->add_theme_constant_override("separation", 8 * EDSCALE);
	phone_content->add_child(phone_row);

	phone_code_country = memnew(OptionButton);
	_configure_country_selector(phone_code_country);
	phone_row->add_child(phone_code_country);

	phone_code_phone = memnew(LineEdit);
	phone_code_phone->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	phone_code_phone->set_placeholder("13800000000");
	phone_code_phone->set_accessibility_name(TTRC("Phone"));
	phone_code_phone->connect(SceneStringName(text_changed), callable_mp(this, &EditorUserLoginDialog::_fields_changed));
	phone_row->add_child(phone_code_phone);

	HBoxContainer *code_row = memnew(HBoxContainer);
	code_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	code_row->add_theme_constant_override("separation", 8 * EDSCALE);
	phone_content->add_child(code_row);

	VBoxContainer *code_fields = memnew(VBoxContainer);
	code_fields->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	code_row->add_child(code_fields);

	_add_field_label(code_fields, TTR("Verification Code"));
	phone_code_code = memnew(LineEdit);
	phone_code_code->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	phone_code_code->set_placeholder(TTR("Code"));
	phone_code_code->set_accessibility_name(TTRC("Verification Code"));
	phone_code_code->connect(SceneStringName(text_changed), callable_mp(this, &EditorUserLoginDialog::_fields_changed));
	code_fields->add_child(phone_code_code);

	send_code_button = memnew(Button);
	send_code_button->set_text(TTR("Send Code"));
	send_code_button->set_custom_minimum_size(Size2(116, 34) * EDSCALE);
	send_code_button->set_v_size_flags(Control::SIZE_SHRINK_END);
	send_code_button->connect(SceneStringName(pressed), callable_mp(this, &EditorUserLoginDialog::_send_code_pressed));
	code_row->add_child(send_code_button);

	phone_code_login_button = memnew(Button);
	phone_code_login_button->set_text(TTR("Continue"));
	phone_code_login_button->set_custom_minimum_size(Size2(0, 36) * EDSCALE);
	phone_code_login_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	phone_code_login_button->connect(SceneStringName(pressed), callable_mp(this, &EditorUserLoginDialog::_phone_code_login_pressed));
	phone_content->add_child(phone_code_login_button);

	MarginContainer *password_page = memnew(MarginContainer);
	password_page->set_name(TTR("Password"));
	tabs->add_child(password_page);

	VBoxContainer *password_content = _make_tab_content(password_page);
	_add_field_label(password_content, TTR("Phone"));
	HBoxContainer *password_phone_row = memnew(HBoxContainer);
	password_phone_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	password_phone_row->add_theme_constant_override("separation", 8 * EDSCALE);
	password_content->add_child(password_phone_row);

	password_country = memnew(OptionButton);
	_configure_country_selector(password_country);
	password_phone_row->add_child(password_country);

	password_phone = memnew(LineEdit);
	password_phone->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	password_phone->set_placeholder("13800000000");
	password_phone->set_accessibility_name(TTRC("Phone"));
	password_phone->connect(SceneStringName(text_changed), callable_mp(this, &EditorUserLoginDialog::_fields_changed));
	password_phone_row->add_child(password_phone);

	_add_field_label(password_content, TTR("Password"));
	password_password = memnew(LineEdit);
	password_password->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	password_password->set_secret(true);
	password_password->set_accessibility_name(TTRC("Password"));
	password_password->connect(SceneStringName(text_changed), callable_mp(this, &EditorUserLoginDialog::_fields_changed));
	password_content->add_child(password_password);

	password_login_button = memnew(Button);
	password_login_button->set_text(TTR("Continue"));
	password_login_button->set_custom_minimum_size(Size2(0, 36) * EDSCALE);
	password_login_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	password_login_button->connect(SceneStringName(pressed), callable_mp(this, &EditorUserLoginDialog::_password_login_pressed));
	password_content->add_child(password_login_button);

	HSeparator *separator = memnew(HSeparator);
	main->add_child(separator);

	status_label = memnew(Label);
	status_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	status_label->set_clip_text(true);
	status_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	main->add_child(status_label);

	provider_label = memnew(Label);
	provider_label->set_text(TTR("服务由 AIRain(airain.net) 提供"));
	provider_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	provider_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	main->add_child(provider_label);

	cooldown_timer = memnew(Timer);
	cooldown_timer->set_wait_time(1.0);
	cooldown_timer->set_one_shot(false);
	cooldown_timer->connect("timeout", callable_mp(this, &EditorUserLoginDialog::_cooldown_timeout));
	add_child(cooldown_timer);

	_update_actions();
}

void EditorUserLoginDialog::_update_theme() {
	if (header_icon && header_icon->is_inside_tree()) {
		header_icon->set_texture(header_icon->get_theme_icon(SNAME("User"), EditorStringName(EditorIcons)));
	}
	if (title_label && title_label->is_inside_tree()) {
		title_label->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("bold"), EditorStringName(EditorFonts)));
		title_label->add_theme_font_size_override(SceneStringName(font_size), get_theme_font_size(SNAME("title_size"), EditorStringName(EditorFonts)));
	}
	if (subtitle_label && subtitle_label->is_inside_tree()) {
		subtitle_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	}
	if (provider_label && provider_label->is_inside_tree()) {
		provider_label->add_theme_font_size_override(SceneStringName(font_size), get_theme_font_size(SNAME("source_size"), EditorStringName(EditorFonts)));
		provider_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	}
}

void EditorUserLoginDialog::_update_actions() {
	if (!tabs) {
		return;
	}

	const bool request_active = manager.is_valid() && (manager->get_state() == EditorUserManager::STATE_LOGGING_IN || manager->get_state() == EditorUserManager::STATE_REFRESHING);
	if (phone_code_login_button) {
		phone_code_login_button->set_disabled(request_active || !_can_submit_phone_code());
	}
	if (password_login_button) {
		password_login_button->set_disabled(request_active || !_can_submit_password());
	}
	if (send_code_button) {
		const bool phone_empty = _get_phone_code_phone().is_empty();
		send_code_button->set_disabled(request_active || phone_empty || send_code_cooldown > 0);
		send_code_button->set_text(send_code_cooldown > 0 ? vformat(TTR("Send Code (%ds)"), send_code_cooldown) : TTR("Send Code"));
	}
}

void EditorUserLoginDialog::_set_status(const String &p_text, bool p_error) {
	if (!status_label) {
		return;
	}
	status_label->set_text(p_text);
	if (status_label->is_inside_tree()) {
		status_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(p_error ? SNAME("error_color") : SNAME("success_color"), EditorStringName(Editor)));
	}
}

void EditorUserLoginDialog::_fields_changed(const String &p_text) {
	(void)p_text;
	_update_actions();
}

void EditorUserLoginDialog::_send_code_pressed() {
	if (manager.is_null()) {
		_set_status(TTR("Account service is not available."), true);
		return;
	}

	AuthResult result = manager->send_phone_code(_get_phone_code_phone());
	if (!result.success) {
		_set_status(result.error, true);
		return;
	}

	send_code_cooldown = PHONE_CODE_COOLDOWN_SECONDS;
	if (cooldown_timer && cooldown_timer->is_inside_tree()) {
		cooldown_timer->start();
	}
	_set_status(TTR("Verification code sent."), false);
	_update_actions();
}

void EditorUserLoginDialog::_phone_code_login_pressed() {
	if (manager.is_null()) {
		_set_status(TTR("Account service is not available."), true);
		return;
	}

	AuthResult result = manager->login_with_phone_code(_get_phone_code_phone(), phone_code_code->get_text());
	if (!result.success) {
		_set_status(result.error, true);
		_update_actions();
		return;
	}

	phone_code_code->clear();
	_set_status(TTR("Account ready."), false);
	hide();
}

void EditorUserLoginDialog::_password_login_pressed() {
	if (manager.is_null()) {
		_set_status(TTR("Account service is not available."), true);
		return;
	}

	AuthResult result = manager->login_with_password(_get_password_phone(), password_password->get_text());
	if (!result.success) {
		_set_status(result.error, true);
		_update_actions();
		return;
	}

	password_password->clear();
	_set_status(TTR("Account ready."), false);
	hide();
}

void EditorUserLoginDialog::_cooldown_timeout() {
	send_code_cooldown = MAX(0, send_code_cooldown - 1);
	if (send_code_cooldown == 0 && cooldown_timer) {
		cooldown_timer->stop();
	}
	_update_actions();
}

bool EditorUserLoginDialog::_can_submit_phone_code() const {
	return phone_code_phone && phone_code_code && !_get_phone_code_phone().is_empty() && !phone_code_code->get_text().strip_edges().is_empty();
}

bool EditorUserLoginDialog::_can_submit_password() const {
	return password_phone && password_password && !_get_password_phone().is_empty() && !password_password->get_text().is_empty();
}

String EditorUserLoginDialog::_get_phone_code_phone() const {
	if (!phone_code_phone) {
		return String();
	}
	return _format_phone(_selected_country_code(phone_code_country), phone_code_phone->get_text());
}

String EditorUserLoginDialog::_get_password_phone() const {
	if (!password_phone) {
		return String();
	}
	return _format_phone(_selected_country_code(password_country), password_phone->get_text());
}

void EditorUserLoginDialog::set_manager(const Ref<EditorUserManager> &p_manager) {
	manager = p_manager;
	_update_actions();
}

Ref<EditorUserManager> EditorUserLoginDialog::get_manager() const {
	return manager;
}

void EditorUserLoginDialog::popup_login() {
	_build_ui();
	_set_status(String(), false);
	_update_actions();
	popup_centered(Size2(460, 360) * EDSCALE);
}

void EditorUserLoginDialog::build_for_test() {
	_build_ui();
}

void EditorUserLoginDialog::set_phone_code_fields_for_test(const String &p_phone, const String &p_code) {
	_build_ui();
	phone_code_phone->set_text(p_phone);
	phone_code_code->set_text(p_code);
	_update_actions();
}

void EditorUserLoginDialog::send_phone_code_for_test() {
	_build_ui();
	_send_code_pressed();
}

bool EditorUserLoginDialog::can_submit_phone_code_for_test() const {
	return _can_submit_phone_code();
}

String EditorUserLoginDialog::get_phone_code_phone_for_test() const {
	return _get_phone_code_phone();
}

String EditorUserLoginDialog::get_send_code_button_text_for_test() const {
	return send_code_button ? send_code_button->get_text() : String();
}

bool EditorUserLoginDialog::is_send_code_button_disabled_for_test() const {
	return send_code_button ? send_code_button->is_disabled() : true;
}

int EditorUserLoginDialog::get_send_code_cooldown_for_test() const {
	return send_code_cooldown;
}

String EditorUserLoginDialog::format_phone_for_test(const String &p_country_code, const String &p_phone) {
	return _format_phone(p_country_code, p_phone);
}

EditorUserLoginDialog::EditorUserLoginDialog() {
	set_title(TTR("Account"));
	set_min_size(Size2(460, 360) * EDSCALE);
	set_hide_on_ok(false);
	set_cancel_button_text(TTR("Cancel"));
}
