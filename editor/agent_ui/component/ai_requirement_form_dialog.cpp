/**************************************************************************/
/*  ai_requirement_form_dialog.cpp                                        */
/**************************************************************************/

#include "ai_requirement_form_dialog.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/check_box.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/text_edit.h"
#include "servers/text/text_server.h"

namespace {

bool _variant_to_bool_default(const Variant &p_value) {
	if (p_value.get_type() == Variant::BOOL) {
		return bool(p_value);
	}
	const String text = String(p_value).strip_edges().to_lower();
	return text == "true" || text == "yes" || text == "on" || text == "1";
}

} // namespace

void AIRequirementFormDialog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_confirmed"), &AIRequirementFormDialog::_confirmed);
	ClassDB::bind_method(D_METHOD("set_form", "form"), &AIRequirementFormDialog::set_form);
	ClassDB::bind_method(D_METHOD("get_form"), &AIRequirementFormDialog::get_form);
	ClassDB::bind_method(D_METHOD("get_answers"), &AIRequirementFormDialog::get_answers);

	ADD_SIGNAL(MethodInfo("form_submitted", PropertyInfo(Variant::DICTIONARY, "answers")));
}

AIRequirementFormDialog::AIRequirementFormDialog() {
	set_title(TTR("Confirm Requirements"));
	set_ok_button_text(TTR("Submit"));
	set_cancel_button_text(TTR("Cancel"));
	set_min_size(Size2(520, 420) * EDSCALE);

	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	scroll->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_AUTO);
	add_child(scroll);

	content = memnew(VBoxContainer);
	content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_theme_constant_override("separation", 8 * EDSCALE);
	scroll->add_child(content);

	connect(SceneStringName(confirmed), callable_mp(this, &AIRequirementFormDialog::_confirmed), CONNECT_DEFERRED);
}

void AIRequirementFormDialog::_clear_form_controls() {
	question_controls.clear();
	if (!content) {
		return;
	}
	while (content->get_child_count() > 0) {
		Node *child = content->get_child(0);
		content->remove_child(child);
		memdelete(child);
	}
}

Control *AIRequirementFormDialog::_create_question_control(const Dictionary &p_question, QuestionControl &r_question_control) {
	const String type = String(p_question.get("type", "text")).strip_edges().to_lower();
	const Variant default_value = p_question.get("default", Variant());
	r_question_control.type = type;

	if (type == "single_choice") {
		OptionButton *option = memnew(OptionButton);
		option->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		Array options = p_question.get("options", Array());
		for (int i = 0; i < options.size(); i++) {
			option->add_item(String(options[i]));
		}
		const String default_text = String(default_value);
		for (int i = 0; i < option->get_item_count(); i++) {
			if (option->get_item_text(i) == default_text) {
				option->select(i);
				break;
			}
		}
		r_question_control.control = option;
		return option;
	}

	if (type == "multi_choice") {
		VBoxContainer *choices = memnew(VBoxContainer);
		choices->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		Array options = p_question.get("options", Array());
		Array default_values;
		if (default_value.get_type() == Variant::ARRAY) {
			default_values = default_value;
		}
		for (int i = 0; i < options.size(); i++) {
			CheckBox *check = memnew(CheckBox);
			const String option_text = String(options[i]);
			check->set_text(option_text);
			for (int j = 0; j < default_values.size(); j++) {
				if (String(default_values[j]) == option_text) {
					check->set_pressed(true);
					break;
				}
			}
			choices->add_child(check);
			r_question_control.checks.push_back(check);
		}
		r_question_control.control = choices;
		return choices;
	}

	if (type == "boolean") {
		CheckBox *check = memnew(CheckBox);
		check->set_pressed(_variant_to_bool_default(default_value));
		r_question_control.control = check;
		return check;
	}

	if (type == "number") {
		SpinBox *spin = memnew(SpinBox);
		spin->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		spin->set_min(-1000000);
		spin->set_max(1000000);
		spin->set_step(1);
		if (default_value.get_type() == Variant::INT || default_value.get_type() == Variant::FLOAT) {
			spin->set_value(double(default_value));
		}
		r_question_control.control = spin;
		return spin;
	}

	if (type == "multiline") {
		TextEdit *edit = memnew(TextEdit);
		edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		edit->set_custom_minimum_size(Size2(0, 86) * EDSCALE);
		edit->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
		edit->set_text(String(default_value));
		r_question_control.control = edit;
		return edit;
	}

	LineEdit *edit = memnew(LineEdit);
	edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	edit->set_text(String(default_value));
	r_question_control.control = edit;
	return edit;
}

void AIRequirementFormDialog::set_form(const Dictionary &p_form) {
	form = p_form.duplicate(true);
	_clear_form_controls();
	if (!content) {
		return;
	}

	const String form_title = String(form.get("title", TTR("Confirm Requirements")));
	set_title(form_title);

	const String purpose = String(form.get("purpose", ""));
	if (!purpose.strip_edges().is_empty()) {
		Label *purpose_label = memnew(Label);
		purpose_label->set_text(purpose);
		purpose_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		purpose_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		content->add_child(purpose_label);
	}

	Array questions = form.get("questions", Array());
	for (int i = 0; i < questions.size(); i++) {
		if (Variant(questions[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary question = questions[i];
		const String id = String(question.get("id", "")).strip_edges();
		if (id.is_empty()) {
			continue;
		}

		VBoxContainer *row = memnew(VBoxContainer);
		row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_theme_constant_override("separation", 3 * EDSCALE);
		content->add_child(row);

		Label *label = memnew(Label);
		label->set_text(String(question.get("label", id)));
		label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(label);

		const String help = String(question.get("help", ""));
		if (!help.strip_edges().is_empty()) {
			Label *help_label = memnew(Label);
			help_label->set_text(help);
			help_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
			help_label->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
			help_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			row->add_child(help_label);
		}

		QuestionControl question_control;
		question_control.id = id;
		Control *field = _create_question_control(question, question_control);
		if (field) {
			row->add_child(field);
			question_controls.push_back(question_control);
		}
	}
}

Dictionary AIRequirementFormDialog::get_form() const {
	return form.duplicate(true);
}

Dictionary AIRequirementFormDialog::get_answers() const {
	Dictionary answers;
	for (int i = 0; i < question_controls.size(); i++) {
		const QuestionControl &question = question_controls[i];
		if (question.type == "single_choice") {
			OptionButton *option = Object::cast_to<OptionButton>(question.control);
			if (option && option->get_selected() >= 0) {
				answers[question.id] = option->get_item_text(option->get_selected());
			}
		} else if (question.type == "multi_choice") {
			Array selected;
			for (int j = 0; j < question.checks.size(); j++) {
				if (question.checks[j] && question.checks[j]->is_pressed()) {
					selected.push_back(question.checks[j]->get_text());
				}
			}
			answers[question.id] = selected;
		} else if (question.type == "boolean") {
			CheckBox *check = Object::cast_to<CheckBox>(question.control);
			answers[question.id] = check ? check->is_pressed() : false;
		} else if (question.type == "number") {
			SpinBox *spin = Object::cast_to<SpinBox>(question.control);
			answers[question.id] = spin ? spin->get_value() : 0.0;
		} else if (question.type == "multiline") {
			TextEdit *edit = Object::cast_to<TextEdit>(question.control);
			answers[question.id] = edit ? edit->get_text() : String();
		} else {
			LineEdit *edit = Object::cast_to<LineEdit>(question.control);
			answers[question.id] = edit ? edit->get_text() : String();
		}
	}
	return answers;
}

void AIRequirementFormDialog::_confirmed() {
	emit_signal(SNAME("form_submitted"), get_answers());
}
