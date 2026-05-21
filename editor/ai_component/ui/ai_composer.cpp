/**************************************************************************/
/*  ai_composer.cpp                                                        */
/**************************************************************************/

#include "ai_composer.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/label.h"
#include "servers/text/text_server.h"

void AIComposer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("reload_models"), &AIComposer::reload_models);
	ADD_SIGNAL(MethodInfo("send_requested", PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::STRING, "model_id"), PropertyInfo(Variant::STRING, "agent_profile_id")));
	ADD_SIGNAL(MethodInfo("cancel_requested"));
}

AIComposer::AIComposer() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_SHRINK_END);

	input = memnew(TextEdit);
	input->set_custom_minimum_size(Size2(0, 80) * EDSCALE);
	input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	input->set_placeholder(TTR("Ask about this project..."));
	add_child(input);

	HBoxContainer *bar = memnew(HBoxContainer);
	bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(bar);

	Label *mode_label = memnew(Label);
	mode_label->set_text(TTR("Mode:"));
	bar->add_child(mode_label);

	mode_selector = memnew(OptionButton);
	mode_selector->set_custom_minimum_size(Size2(80, 0) * EDSCALE);
	mode_selector->add_item(TTR("Ask"));
	mode_selector->set_item_metadata(0, "plan");
	mode_selector->add_item(TTR("Write"));
	mode_selector->set_item_metadata(1, "write");
	mode_selector->select(0);
	bar->add_child(mode_selector);

	Label *model_label = memnew(Label);
	model_label->set_text(TTR("Model:"));
	bar->add_child(model_label);

	model_selector = memnew(OptionButton);
	model_selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	model_selector->set_custom_minimum_size(Size2(96, 0) * EDSCALE);
	model_selector->set_fit_to_longest_item(false);
	model_selector->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
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
	if (!has_model || model_selector->get_item_count() == 0) {
		return String();
	}
	return String(model_selector->get_item_metadata(model_selector->get_selected()));
}

String AIComposer::get_selected_agent_profile_id() const {
	if (!mode_selector || mode_selector->get_item_count() == 0 || mode_selector->get_selected() < 0) {
		return "plan";
	}
	return String(mode_selector->get_item_metadata(mode_selector->get_selected()));
}

void AIComposer::clear_input() {
	input->clear();
}

void AIComposer::set_running(bool p_running) {
	send_button->set_disabled(p_running || !has_model);
	cancel_button->set_disabled(!p_running);
	if (mode_selector) {
		mode_selector->set_disabled(p_running);
	}
	if (model_selector) {
		model_selector->set_disabled(p_running || !has_model);
	}
}

void AIComposer::reload_models() {
	model_selector->clear();
	has_model = false;

	Vector<AIModelDescriptor> enabled_models = AIModelSettings::get_enabled_models();
	for (int i = 0; i < enabled_models.size(); i++) {
		const AIModelDescriptor &model = enabled_models[i];
		int item_index = model_selector->get_item_count();
		String label = model.display_name;
		if (label.is_empty()) {
			label = model.provider_name + " / " + model.model;
		} else if (!label.contains(model.model)) {
			label += " / " + model.model;
		}
		model_selector->add_item(label);
		model_selector->set_item_metadata(item_index, model.id);
	}

	if (model_selector->get_item_count() == 0) {
		model_selector->add_item(TTR("No model configured"));
		model_selector->set_disabled(true);
		send_button->set_disabled(true);
		return;
	}

	has_model = true;
	model_selector->set_disabled(false);
	send_button->set_disabled(false);
}

void AIComposer::_send_pressed() {
	if (!has_model) {
		return;
	}
	emit_signal(SNAME("send_requested"), get_input_text(), get_selected_model(), get_selected_agent_profile_id());
}

void AIComposer::_cancel_pressed() {
	emit_signal(SNAME("cancel_requested"));
}
