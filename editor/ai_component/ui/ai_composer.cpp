/**************************************************************************/
/*  ai_composer.cpp                                                        */
/**************************************************************************/

#include "ai_composer.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/label.h"

void AIComposer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("reload_models"), &AIComposer::reload_models);
	ADD_SIGNAL(MethodInfo("send_requested", PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::STRING, "model_id")));
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
		return AIModelSettings::get_model_id("deepseek", "deepseek-chat");
	}
	return String(model_selector->get_item_metadata(model_selector->get_selected()));
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

	Vector<AIModelDescriptor> enabled_models = AIModelSettings::get_enabled_models();
	for (int i = 0; i < enabled_models.size(); i++) {
		const AIModelDescriptor &model = enabled_models[i];
		int item_index = model_selector->get_item_count();
		model_selector->add_item(model.provider_name + " / " + model.model);
		model_selector->set_item_metadata(item_index, model.id);
	}

	if (model_selector->get_item_count() == 0) {
		int item_index = model_selector->get_item_count();
		model_selector->add_item("DeepSeek / deepseek-chat");
		model_selector->set_item_metadata(item_index, AIModelSettings::get_model_id("deepseek", "deepseek-chat"));
	}
}

void AIComposer::_send_pressed() {
	emit_signal(SNAME("send_requested"), get_input_text(), get_selected_model());
}

void AIComposer::_cancel_pressed() {
	emit_signal(SNAME("cancel_requested"));
}
