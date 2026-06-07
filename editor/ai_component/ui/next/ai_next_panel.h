/**************************************************************************/
/*  ai_next_panel.h                                                       */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"

class AIAgentNextSession;
class AIRequirementFormDialog;
class AINextFeedbackPanel;
class AINextMilestoneList;
class AINextTaskInspector;
class AINextTaskTree;
class Button;
class ColorRect;
class FoldableContainer;
class MarkdownViewer;
class OptionButton;
class VBoxContainer;
class Label;
class TextEdit;
class TextureRect;

class AINextPanel : public VBoxContainer {
	GDCLASS(AINextPanel, VBoxContainer);

	AIAgentNextSession *next_session = nullptr;
	OptionButton *workflow_selector = nullptr;
	Button *new_workflow_button = nullptr;
	Button *delete_workflow_button = nullptr;
	Button *continue_workflow_button = nullptr;
	Button *terminate_workflow_button = nullptr;
	TextEdit *brief_input = nullptr;
	Label *state_label = nullptr;
	TextureRect *operation_icon = nullptr;
	Label *operation_label = nullptr;
	ColorRect *operation_progress = nullptr;
	FoldableContainer *task_inspector_section = nullptr;
	FoldableContainer *review_findings_section = nullptr;
	FoldableContainer *activity_section = nullptr;
	Label *task_inspector_summary = nullptr;
	Label *review_findings_summary = nullptr;
	Label *activity_summary = nullptr;
	MarkdownViewer *review_findings_viewer = nullptr;
	VBoxContainer *activity_list = nullptr;
	Button *submit_button = nullptr;
	Button *plan_button = nullptr;
	Button *run_button = nullptr;
	Button *review_button = nullptr;
	AINextMilestoneList *milestone_list = nullptr;
	AINextTaskTree *task_tree = nullptr;
	AINextTaskInspector *task_inspector = nullptr;
	AINextFeedbackPanel *feedback_panel = nullptr;
	AIRequirementFormDialog *requirement_form_dialog = nullptr;
	int spinner_frame = 0;
	double spinner_elapsed = 0.0;
	String displayed_workflow_id;

	void _submit_brief_pressed();
	void _workflow_selected(int p_index);
	void _new_workflow_pressed();
	void _delete_workflow_pressed();
	void _continue_workflow_pressed();
	void _terminate_workflow_pressed();
	void _generate_plan_pressed();
	void _run_milestone_pressed();
	void _review_milestone_pressed();
	void _requirement_form_requested(const Dictionary &p_form);
	void _requirement_form_submitted(const Dictionary &p_answers);
	void _refresh();
	void _refresh_workflow_session();
	void _refresh_progress();
	void _refresh_workflows();
	void _refresh_activity();
	void _refresh_review_findings();
	void _refresh_task_inspector_summary();
	void _refresh_activity_summary();
	void _refresh_theme_icons();
	void _ensure_operation_progress_material();
	void _clear_operation_progress_material();
	void _update_operation_label();
	Label *_add_section_label(const String &p_text);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AINextPanel();
	void set_next_session(AIAgentNextSession *p_session);
	void refresh();
};
