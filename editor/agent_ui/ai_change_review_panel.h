/**************************************************************************/
/*  ai_change_review_panel.h                                               */
/**************************************************************************/

#pragma once

#include "scene/gui/panel_container.h"

#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_bridge.h"

class AcceptDialog;
class AITextDiffViewer;
class Button;
class ConfirmationDialog;
class HBoxContainer;
class Label;
class VBoxContainer;

class AIChangeReviewPanel : public PanelContainer {
	GDCLASS(AIChangeReviewPanel, PanelContainer);

	Ref<AIAgentV1UIBridge> bridge;
	VBoxContainer *content = nullptr;
	Label *title_label = nullptr;
	Label *empty_label = nullptr;
	AcceptDialog *diff_dialog = nullptr;
	AcceptDialog *error_dialog = nullptr;
	AITextDiffViewer *diff_viewer = nullptr;
	ConfirmationDialog *revert_dialog = nullptr;
	String pending_revert_change_set_id;

	void _refresh();
	void _view_change_set(const String &p_change_set_id);
	void _keep_change_set(const String &p_change_set_id);
	void _revert_change_set(const String &p_change_set_id);
	void _confirm_revert_change_set();
	void _show_error(const String &p_error);
	void _change_sets_changed();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIChangeReviewPanel();
	void refresh();
};
