/**************************************************************************/
/*  ai_composer.cpp                                                        */
/**************************************************************************/

#include "ai_composer.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/label.h"

void AIComposer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("reload_models"), &AIComposer::reload_models);
	ADD_SIGNAL(MethodInfo("send_requested", PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::STRING, "model")));
	ADD_SIGNAL(MethodInfo("cancel_requested"));
}

AIComposer::AIComposer() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_SHRINK_END);

	input = memnew(TextEdit);
	input->set_custom_minimum_size(Size2(0, 96) * EDSCALE);
	input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	input->set_placeholder(TTR("Ask about this project..."));
	add_child(input);

	HBoxContainer *bar = memnew(HBoxContainer);
	bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(bar);

	Label *model_label = memnew(Label);
	model_label->set_text(TTR("Model:"));
	bar->add_child(model_label);

	model_selector = memnew(OptionButton);
	model_selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	bar->add_child(model_selector);

	cancel_button = memnew(Button);
	cancel_button->set_text(TTR("Cancel"));
	cancel_button->connect("pressed", callable_mp(this, &AIComposer::_cancel_pressed));
	bar->add_child(cancel_button);

	send_button = memnew(Button);
	send_button->set_text(TTR("Send"));
	send_button->connect("pressed", callable_mp(this, &AIComposer::_send_pressed));
	bar->add_child(send_button);

	reload_models();
	set_running(false);
}

String AIComposer::get_input_text() const {
	return input->get_text();
}

String AIComposer::get_selected_model() const {
	if (model_selector->get_item_count() == 0) {
		return "deepseek-chat";
	}
	return model_selector->get_item_text(model_selector->get_selected());
}

void AIComposer::clear_input() {
	input->clear();
}

void AIComposer::set_running(bool p_running) {
	send_button->set_disabled(p_running);
	cancel_button->set_disabled(!p_running);
}

void AIComposer::reload_models() {
	model_selector->clear();
	EditorSettings *settings = EditorSettings::get_singleton();
	const String deepseek_models[] = { "deepseek-chat", "deepseek-reasoner" };
	for (int i = 0; i < 2; i++) {
		String setting_path = "deepseek/models/" + deepseek_models[i];
		bool enabled = true;
		if (settings && settings->has_setting(setting_path)) {
			enabled = settings->get(setting_path);
		}
		if (enabled) {
			model_selector->add_item(deepseek_models[i]);
		}
	}
	if (model_selector->get_item_count() == 0) {
		model_selector->add_item("deepseek-chat");
	}
}

void AIComposer::_send_pressed() {
	emit_signal(SNAME("send_requested"), get_input_text(), get_selected_model());
}

void AIComposer::_cancel_pressed() {
	emit_signal(SNAME("cancel_requested"));
}
