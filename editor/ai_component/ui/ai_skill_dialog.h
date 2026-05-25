/**************************************************************************/
/*  ai_skill_dialog.h                                                      */
/**************************************************************************/

#pragma once

#include "editor/ai_component/skills/ai_skill_settings.h"
#include "scene/gui/dialogs.h"

class CheckBox;
class Label;
class LineEdit;
class TextEdit;

class AISkillDialog : public ConfirmationDialog {
	GDCLASS(AISkillDialog, ConfirmationDialog);

	String editing_skill_id;
	LineEdit *name_edit = nullptr;
	LineEdit *description_edit = nullptr;
	TextEdit *content_edit = nullptr;
	CheckBox *enabled_check = nullptr;
	Label *error_label = nullptr;

	void _build_ui();
	void _reset_form();
	void _confirmed();
	void _set_error(const String &p_error);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AISkillDialog();

	void popup_add_skill();
	void popup_edit_skill(const AISkillConfig &p_skill);
	bool is_editing_skill() const;
	String get_editing_skill_id() const;
	AISkillConfig get_submitted_skill() const;
};
