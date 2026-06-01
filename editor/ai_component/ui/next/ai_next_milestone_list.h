/**************************************************************************/
/*  ai_next_milestone_list.h                                              */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"

class AIAgentNextSession;
class Button;
class ConfirmationDialog;
class Control;
class LineEdit;
class OptionButton;
class TextEdit;

class AINextMilestoneList : public VBoxContainer {
	GDCLASS(AINextMilestoneList, VBoxContainer);

	AIAgentNextSession *next_session = nullptr;
	ConfirmationDialog *milestone_dialog = nullptr;
	LineEdit *milestone_title_edit = nullptr;
	TextEdit *milestone_description_edit = nullptr;
	ConfirmationDialog *delete_dialog = nullptr;
	ConfirmationDialog *merge_dialog = nullptr;
	OptionButton *merge_target_selector = nullptr;
	String editing_milestone_id;
	String pending_delete_milestone_id;
	String pending_merge_source_milestone_id;

	void _clear_rows();
	void _milestone_pressed(const String &p_milestone_id);
	void _add_milestone_pressed();
	void _edit_milestone_pressed(const String &p_milestone_id);
	void _delete_milestone_pressed(const String &p_milestone_id);
	void _confirm_delete_milestone();
	void _move_milestone_pressed(const String &p_milestone_id, int p_to_index);
	void _merge_milestone_pressed(const String &p_milestone_id);
	void _confirm_merge_milestone();
	void _confirm_milestone_dialog();
	void _build_dialogs();
	Variant _get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool _can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void _drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);
	int _get_drop_slot(const Point2 &p_point, const Control *p_from = nullptr) const;

protected:
	static void _bind_methods();
	virtual bool can_drop_data(const Point2 &p_point, const Variant &p_data) const override;
	virtual void drop_data(const Point2 &p_point, const Variant &p_data) override;
	void _notification(int p_what);

public:
	AINextMilestoneList();
	void set_next_session(AIAgentNextSession *p_session);
	void refresh();
};
