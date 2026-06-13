/**************************************************************************/
/*  ai_status_panel.cpp                                                   */
/**************************************************************************/

#include "ai_status_panel.h"

#include "core/io/image.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/item_list.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/popup.h"
#include "scene/gui/tab_container.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/style_box.h"
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

String _mcp_button_tooltip(const Array &p_statuses, const Dictionary &p_summary) {
	const int total = (int)p_summary.get("total", 0);
	const int ok = (int)p_summary.get("ready", p_summary.get("ok", 0));
	const int failed = (int)p_summary.get("failed", 0);
	const int disabled = (int)p_summary.get("disabled", 0);
	const int checking = MAX(0, total - ok - failed - disabled);
	const int tool_count = _mcp_total_tool_count(p_statuses, p_summary);

	if (total <= 0) {
		return TTR("MCP: no servers configured.");
	}
	if (failed > 0) {
		return vformat(TTR("MCP: %d failed, %d ready, %d disabled. Tools: %d."), failed, ok, disabled, tool_count);
	}
	if (checking > 0) {
		return vformat(TTR("MCP: checking %d server(s)."), checking);
	}
	return vformat(TTR("MCP: %d ready, %d disabled. Tools: %d."), ok, disabled, tool_count);
}

String _skill_button_tooltip(const Array &p_skills) {
	if (p_skills.is_empty()) {
		return TTR("AgentSkills: none configured.");
	}

	int enabled_count = 0;
	for (int i = 0; i < p_skills.size(); i++) {
		if (p_skills[i].get_type() == Variant::DICTIONARY && bool(Dictionary(p_skills[i]).get("enabled", true))) {
			enabled_count++;
		}
	}
	return vformat(TTR("AgentSkills: %d configured, %d enabled."), p_skills.size(), enabled_count);
}

} // namespace

void AIStatusPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_bridge", "bridge"), &AIStatusPanel::set_bridge);
	ClassDB::bind_method(D_METHOD("get_bridge"), &AIStatusPanel::get_bridge);
	ClassDB::bind_method(D_METHOD("refresh"), &AIStatusPanel::refresh);
}

void AIStatusPanel::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
		_refresh_button();
	}
}

AIStatusPanel::AIStatusPanel() {
	set_name("AIStatusButton");
	set_tooltip_text(TTR("MCP and AgentSkill status."));
	connect(SceneStringName(pressed), callable_mp(this, &AIStatusPanel::_pressed));

	status_popup = memnew(PopupPanel);
	status_popup->set_name("AIStatusPopup");
	status_popup->set_min_size(Size2(420, 300) * EDSCALE);
	status_popup->add_theme_style_override(SceneStringName(panel), memnew(StyleBoxEmpty));
	add_child(status_popup);

	MarginContainer *status_margin = memnew(MarginContainer);
	status_margin->add_theme_constant_override("margin_right", 8 * EDSCALE);
	status_margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
	status_margin->add_theme_constant_override("margin_left", 8 * EDSCALE);
	status_margin->add_theme_constant_override("margin_bottom", 8 * EDSCALE);
	status_popup->add_child(status_margin);

	status_tabs = memnew(TabContainer);
	status_tabs->set_name("AIStatusTabs");
	status_tabs->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	status_tabs->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	status_margin->add_child(status_tabs);

	VBoxContainer *mcp_status_page = memnew(VBoxContainer);
	mcp_status_page->set_name(TTR("MCP"));
	mcp_status_page->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	mcp_status_page->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	status_tabs->add_child(mcp_status_page);
	status_tabs->set_tab_title(0, TTR("MCP"));

	mcp_status_list = memnew(ItemList);
	mcp_status_list->set_name("AIMCPStatusList");
	_setup_status_item_list(mcp_status_list);
	mcp_status_page->add_child(mcp_status_list);

	VBoxContainer *skill_status_page = memnew(VBoxContainer);
	skill_status_page->set_name(TTR("Skill"));
	skill_status_page->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	skill_status_page->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	status_tabs->add_child(skill_status_page);
	status_tabs->set_tab_title(1, TTR("Skill"));

	skill_status_list = memnew(ItemList);
	skill_status_list->set_name("AISkillStatusList");
	_setup_status_item_list(skill_status_list);
	skill_status_page->add_child(skill_status_list);
}

void AIStatusPanel::set_bridge(const Ref<AIAgentV1UIBridge> &p_bridge) {
	bridge = p_bridge;
	refresh();
}

Ref<AIAgentV1UIBridge> AIStatusPanel::get_bridge() const {
	return bridge;
}

void AIStatusPanel::refresh() {
	_refresh_button();
	if (status_popup && status_popup->is_visible()) {
		_refresh_popup();
	}
}

void AIStatusPanel::_pressed() {
	_refresh_popup();
	if (!status_popup) {
		return;
	}

	const Size2 popup_size = Size2(420, 300) * EDSCALE;
	Rect2 rect = get_screen_rect();
	rect.position.y += rect.size.height;
	rect.size = popup_size;
	status_popup->popup(Rect2i(rect));
}

void AIStatusPanel::_refresh_button() {
	if (is_inside_tree()) {
		set_button_icon(get_editor_theme_icon(SNAME("Server")));
	}

	const Dictionary snapshot = _settings_snapshot_from_bridge(bridge);
	const Array mcp_statuses = _array_from_variant(snapshot.get("mcp_statuses", Array()));
	const Dictionary mcp_summary = _dictionary_from_variant(snapshot.get("mcp_summary", Dictionary()));
	const Array skills = _array_from_variant(snapshot.get("skills", Array()));
	set_tooltip_text(_mcp_button_tooltip(mcp_statuses, mcp_summary) + "\n" + _skill_button_tooltip(skills));
}

void AIStatusPanel::_refresh_popup() {
	_refresh_mcp_status_list();
	_refresh_skill_status_list();
}

void AIStatusPanel::_refresh_mcp_status_list() {
	if (!mcp_status_list) {
		return;
	}

	mcp_status_list->clear();

	const Dictionary snapshot = _settings_snapshot_from_bridge(bridge);
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

		Dictionary status_info = statuses[i];
		const String display_name = String(status_info.get("display_name", status_info.get("server_name", TTR("Unnamed MCP"))));
		const String transport = String(status_info.get("transport", status_info.get("mcp_transport", String())));
		const String state = String(status_info.get("status", status_info.get("state", String())));
		const int tool_count = (int)status_info.get("tool_count", 0);
		const String error = String(status_info.get("error", status_info.get("last_error", String())));
		const String endpoint = String(status_info.get("endpoint", String()));

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

void AIStatusPanel::_refresh_skill_status_list() {
	if (!skill_status_list) {
		return;
	}

	skill_status_list->clear();

	const Dictionary snapshot = _settings_snapshot_from_bridge(bridge);
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
