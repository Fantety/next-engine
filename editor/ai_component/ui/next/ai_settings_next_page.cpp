/**************************************************************************/
/*  ai_settings_next_page.cpp                                             */
/**************************************************************************/

#include "ai_settings_next_page.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/ai_component/next/ai_next_agent_settings.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"

namespace {

Label *_make_next_settings_label(const String &p_text, int p_width = 0) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	if (p_width > 0) {
		label->set_custom_minimum_size(Size2(p_width, 0) * EDSCALE);
	} else {
		label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	}
	return label;
}

String _make_profile_label(const AIModelProfile &p_profile) {
	String label = p_profile.display_name;
	if (label.is_empty()) {
		label = p_profile.provider_name + " / " + p_profile.model;
	} else if (!label.contains(p_profile.model)) {
		label += " / " + p_profile.model;
	}
	return label;
}

} // namespace

void AISettingsNextPage::_bind_methods() {
	ADD_SIGNAL(MethodInfo("settings_changed"));
}

void AISettingsNextPage::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build_ui();
	}
}

AISettingsNextPage::AISettingsNextPage() {
}

void AISettingsNextPage::_build_ui() {
	if (agent_table) {
		return;
	}

	add_theme_constant_override("margin_left", 8 * EDSCALE);
	add_theme_constant_override("margin_right", 8 * EDSCALE);
	add_theme_constant_override("margin_top", 8 * EDSCALE);
	add_theme_constant_override("margin_bottom", 8 * EDSCALE);

	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(scroll);

	VBoxContainer *content = memnew(VBoxContainer);
	content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_theme_constant_override("separation", 12 * EDSCALE);
	scroll->add_child(content);

	Label *title = memnew(Label);
	title->set_text(TTR("NEXT"));
	title->add_theme_font_size_override(SceneStringName(font_size), int(22 * EDSCALE));
	content->add_child(title);

	Label *section_title = memnew(Label);
	section_title->set_text(TTR("Agent Models"));
	section_title->add_theme_font_size_override(SceneStringName(font_size), int(14 * EDSCALE));
	content->add_child(section_title);

	Label *description = memnew(Label);
	description->set_text(TTR("Choose which configured LLM model each NEXT agent should use. Models are managed on the LLM page."));
	description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	content->add_child(description);

	PanelContainer *table_panel = memnew(PanelContainer);
	table_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(table_panel);

	agent_table = memnew(VBoxContainer);
	agent_table->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	agent_table->add_theme_constant_override("separation", 0);
	table_panel->add_child(agent_table);

	HBoxContainer *header = memnew(HBoxContainer);
	header->set_custom_minimum_size(Size2(0, 32) * EDSCALE);
	header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header->add_theme_constant_override("separation", 8 * EDSCALE);
	agent_table->add_child(header);

	Label *agent_header = _make_next_settings_label(TTR("Agent"), 180);
	agent_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(agent_header);

	Label *model_header = _make_next_settings_label(TTR("Model"));
	model_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(model_header);

	agent_table->add_child(memnew(HSeparator));

	Vector<String> agent_ids = AINextAgentSettings::get_agent_ids();
	for (int i = 0; i < agent_ids.size(); i++) {
		_add_agent_model_row(agent_ids[i]);
	}
	refresh_models();
}

void AISettingsNextPage::_add_agent_model_row(const String &p_agent_id) {
	ERR_FAIL_NULL(agent_table);

	HBoxContainer *row = memnew(HBoxContainer);
	row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row->set_custom_minimum_size(Size2(0, 36) * EDSCALE);
	row->add_theme_constant_override("separation", 8 * EDSCALE);
	agent_table->add_child(row);

	Label *agent_label = _make_next_settings_label(AINextAgentSettings::get_agent_display_name(p_agent_id), 180);
	row->add_child(agent_label);

	OptionButton *selector = memnew(OptionButton);
	selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	selector->set_fit_to_longest_item(false);
	selector->connect(SceneStringName(item_selected), callable_mp(this, &AISettingsNextPage::_agent_model_selected).bind(p_agent_id), CONNECT_DEFERRED);
	row->add_child(selector);

	AgentModelRow row_data;
	row_data.agent_id = p_agent_id;
	row_data.model_selector = selector;
	agent_model_rows.push_back(row_data);

	agent_table->add_child(memnew(HSeparator));
}

void AISettingsNextPage::_populate_model_selector(AgentModelRow &r_row) {
	ERR_FAIL_NULL(r_row.model_selector);
	r_row.model_selector->clear();

	Vector<AIModelDescriptor> models = AIModelSettings::get_enabled_models();
	if (models.is_empty()) {
		r_row.model_selector->add_item(TTR("No model configured in LLM"));
		r_row.model_selector->set_disabled(true);
		return;
	}

	const String effective_profile_id = AINextAgentSettings::get_effective_model_profile_id(r_row.agent_id);
	int selected_index = 0;
	for (int i = 0; i < models.size(); i++) {
		const int item_index = r_row.model_selector->get_item_count();
		r_row.model_selector->add_item(_make_profile_label(models[i]));
		r_row.model_selector->set_item_metadata(item_index, models[i].id);
		if (models[i].id == effective_profile_id) {
			selected_index = item_index;
		}
	}
	r_row.model_selector->select(selected_index);
	r_row.model_selector->set_disabled(false);
}

void AISettingsNextPage::_agent_model_selected(int p_index, const String &p_agent_id) {
	for (int i = 0; i < agent_model_rows.size(); i++) {
		AgentModelRow &row = agent_model_rows.write[i];
		if (row.agent_id != p_agent_id || !row.model_selector || p_index < 0 || p_index >= row.model_selector->get_item_count()) {
			continue;
		}
		const String model_profile_id = String(row.model_selector->get_item_metadata(p_index));
		if (AINextAgentSettings::set_model_profile_id(p_agent_id, model_profile_id)) {
			emit_signal(SNAME("settings_changed"));
		}
		return;
	}
}

void AISettingsNextPage::build_for_test() {
	_build_ui();
}

void AISettingsNextPage::refresh_models() {
	if (!agent_table) {
		return;
	}
	for (int i = 0; i < agent_model_rows.size(); i++) {
		_populate_model_selector(agent_model_rows.write[i]);
	}
}

int AISettingsNextPage::get_agent_model_row_count_for_test() const {
	return agent_model_rows.size();
}

void AISettingsNextPage::set_agent_model_for_test(const String &p_agent_id, const String &p_model_profile_id) {
	if (AINextAgentSettings::set_model_profile_id(p_agent_id, p_model_profile_id)) {
		refresh_models();
		emit_signal(SNAME("settings_changed"));
	}
}
