/**************************************************************************/
/*  ai_change_review_panel.h                                               */
/**************************************************************************/

#pragma once

#include "scene/gui/panel_container.h"

#include "editor/ai_component/review/ai_change_set_store.h"

class AcceptDialog;
class AITextDiffViewer;
class Button;
class ConfirmationDialog;
class HBoxContainer;
class Label;
class VBoxContainer;

class AIChangeReviewPanel : public PanelContainer {
	GDCLASS(AIChangeReviewPanel, PanelContainer);

	Ref<AIChangeSetStore> store;
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
	void _store_changed();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIChangeReviewPanel();
	void refresh();
};
