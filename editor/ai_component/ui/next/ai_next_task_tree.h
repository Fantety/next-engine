/**************************************************************************/
/*  ai_next_task_tree.h                                                   */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"
#include "core/templates/vector.h"

class AIAgentNextSession;
class AIMessageList;
class Button;
class CheckBox;
class ConfirmationDialog;
class Control;
class LineEdit;
class OptionButton;
class TextEdit;
class VBoxContainer;

class AINextTaskTree : public VBoxContainer {
	GDCLASS(AINextTaskTree, VBoxContainer);

	AIAgentNextSession *next_session = nullptr;
	ConfirmationDialog *task_dialog = nullptr;
	LineEdit *task_title_edit = nullptr;
	TextEdit *task_description_edit = nullptr;
	OptionButton *task_agent_selector = nullptr;
	OptionButton *task_milestone_selector = nullptr;
	ConfirmationDialog *delete_dialog = nullptr;
	ConfirmationDialog *dependency_dialog = nullptr;
	VBoxContainer *dependency_list = nullptr;
	ConfirmationDialog *task_session_dialog = nullptr;
	AIMessageList *task_session_messages = nullptr;
	TextEdit *task_session_input = nullptr;
	Button *task_session_send_button = nullptr;
	Vector<CheckBox *> dependency_checks;
	String editing_task_id;
	String pending_delete_task_id;
	String dependency_task_id;
	String session_task_id;

	void _clear_rows();
	void _task_pressed(const String &p_task_id);
	void _run_task_pressed(const String &p_task_id);
	void _task_session_pressed(const String &p_task_id);
	void _send_task_session_message_pressed();
	void _task_session_input_changed();
	void _task_session_source_changed();
	void _add_task_pressed();
	void _edit_task_pressed(const String &p_task_id);
	void _confirm_task_dialog();
	void _delete_task_pressed(const String &p_task_id);
	void _confirm_delete_task();
	void _move_task_pressed(const String &p_task_id, int p_to_index);
	void _dependencies_pressed(const String &p_task_id);
	void _confirm_dependencies();
	void _build_dialogs();
	void _populate_milestone_selector(const String &p_selected_milestone_id);
	void _populate_dependency_dialog(const String &p_task_id);
	void _refresh_task_session_dialog();
	void _update_task_session_send_button();
	Variant _get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool _can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void _drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);
	int _get_drop_slot(const Point2 &p_point, const Control *p_from = nullptr) const;

protected:
	static void _bind_methods();
	void _notification(int p_what);
	virtual bool can_drop_data(const Point2 &p_point, const Variant &p_data) const override;
	virtual void drop_data(const Point2 &p_point, const Variant &p_data) override;

public:
	AINextTaskTree();
	void set_next_session(AIAgentNextSession *p_session);
	void refresh();
};
