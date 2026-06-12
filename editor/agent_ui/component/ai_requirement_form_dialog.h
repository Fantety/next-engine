/**************************************************************************/
/*  ai_requirement_form_dialog.h                                          */
/**************************************************************************/

#pragma once

#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "scene/gui/dialogs.h"

class CheckBox;
class Control;
class LineEdit;
class OptionButton;
class SpinBox;
class TextEdit;
class VBoxContainer;

class AIRequirementFormDialog : public ConfirmationDialog {
	GDCLASS(AIRequirementFormDialog, ConfirmationDialog);

	struct QuestionControl {
		String id;
		String type;
		Control *control = nullptr;
		Vector<CheckBox *> checks;
	};

	Dictionary form;
	VBoxContainer *content = nullptr;
	Vector<QuestionControl> question_controls;

	void _clear_form_controls();
	void _confirmed();
	Control *_create_question_control(const Dictionary &p_question, QuestionControl &r_question_control);

protected:
	static void _bind_methods();

public:
	AIRequirementFormDialog();

	void set_form(const Dictionary &p_form);
	Dictionary get_form() const;
	Dictionary get_answers() const;
};
