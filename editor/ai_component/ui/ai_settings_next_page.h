/**************************************************************************/
/*  ai_settings_next_page.h                                               */
/**************************************************************************/

#pragma once

#include "scene/gui/margin_container.h"

class OptionButton;
class VBoxContainer;

class AISettingsNextPage : public MarginContainer {
	GDCLASS(AISettingsNextPage, MarginContainer);

	struct AgentModelRow {
		String agent_id;
		OptionButton *model_selector = nullptr;
	};

	VBoxContainer *agent_table = nullptr;
	Vector<AgentModelRow> agent_model_rows;

	void _build_ui();
	void _add_agent_model_row(const String &p_agent_id);
	void _populate_model_selector(AgentModelRow &r_row);
	void _agent_model_selected(int p_index, const String &p_agent_id);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AISettingsNextPage();

	void build_for_test();
	void refresh_models();
	int get_agent_model_row_count_for_test() const;
	void set_agent_model_for_test(const String &p_agent_id, const String &p_model_profile_id);
};
