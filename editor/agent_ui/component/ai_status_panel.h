/**************************************************************************/
/*  ai_status_panel.h                                                     */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_bridge.h"
#include "scene/gui/button.h"

class ItemList;
class PopupPanel;
class TabContainer;

class AIStatusPanel : public Button {
	GDCLASS(AIStatusPanel, Button);

	Ref<AIAgentV1UIBridge> bridge;
	PopupPanel *status_popup = nullptr;
	TabContainer *status_tabs = nullptr;
	ItemList *mcp_status_list = nullptr;
	ItemList *skill_status_list = nullptr;

	void _pressed();
	void _refresh_button();
	void _refresh_popup();
	void _refresh_mcp_status_list();
	void _refresh_skill_status_list();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIStatusPanel();

	void set_bridge(const Ref<AIAgentV1UIBridge> &p_bridge);
	Ref<AIAgentV1UIBridge> get_bridge() const;
	void refresh();
};
