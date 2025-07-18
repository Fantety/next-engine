#include "ai_chat_panel.h"
#include "editor/settings/editor_settings.h"
#include "editor/editor_node.h"

void AIChatPanel::on_send_button_pressed(){
    emit_signal("send_button_pressed");
}

AIChatPanel::AIChatPanel(){
    set_h_size_flags(Control::SIZE_EXPAND_FILL);
    set_v_size_flags(Control::SIZE_EXPAND_FILL);
    set_stretch_ratio(1);
    print_line("AIChatPanel Init Start");
    selector_container = memnew(HBoxContainer);
    loading_bar = memnew(ProgressBar);
    model_label = memnew(Label);
    model_label->set_text("Model:");
    selector_container->add_child(model_label);
    model_selector = memnew(OptionButton);
    model_selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    _update_model_list(); // 初始化模型列表
    selector_container->add_child(model_selector);
    selector_container->set_stretch_ratio(4);

    loading_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    //loading_bar->set_max(1);
    //loading_bar->set_step(0.1);
    loading_bar->set_visible(false);
    //loading_bar->set_show_percentage(false);
    loading_bar->set_indeterminate(true);
    input_view = memnew(VBoxContainer);
    input_view->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    input_view->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    input_view->add_child(loading_bar);
    input_bottom_bar = memnew(HBoxContainer);
    input_bottom_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    input_bottom_bar->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    input_bottom_bar->set_stretch_ratio(1);
    send_button = memnew(Button);
    send_button->set_text("Send");
    send_button->connect("pressed", callable_mp(this, &AIChatPanel::on_send_button_pressed));
    send_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    send_button->set_stretch_ratio(1);
    input_box = memnew(TextEdit);
    input_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    input_box->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    input_box->set_placeholder("Type your message...");
    input_box->set_stretch_ratio(4);
    input_bottom_bar->add_child(selector_container);
    input_bottom_bar->add_child(send_button);
    this->add_child(input_view);
    input_view->add_child(input_box);
    input_view->add_child(input_bottom_bar);
    print_line("AIChatPanel Init Finish");
}


void AIChatPanel::_update_model_list() {
    model_selector->clear();
    const String deepseek_models[] = {"deepseek-chat", "deepseek-reasoner"};
    for (int i = 0; i < 2; i++) {
        String setting_path = "deepseek/models/" + deepseek_models[i];
        if (EditorSettings::get_singleton()->has_setting(setting_path)) {
            bool model_enabled = EditorSettings::get_singleton()->get(setting_path);
            if (model_enabled) {
                model_selector->add_item(deepseek_models[i]);
                print_line("DeepSeek: " + deepseek_models[i]);
            }
        }
    }
    const String openai_models[] = {"gpt-4-turbo", "gpt-4", "gpt-3.5-turbo"};
    for (int i = 0; i < 3; i++) {
        String setting_path = "openai/models/" + openai_models[i];
        if (EditorSettings::get_singleton()->has_setting(setting_path)) {
            bool model_enabled = EditorSettings::get_singleton()->get(setting_path);
            if (model_enabled) {
                model_selector->add_item(openai_models[i]);
            }
        }
    }

    const String gemini_models[] = {"gemini-pro", "gemini-ultra", "gemini-nano"};
    for (int i = 0; i < 3; i++) {
        String setting_path = "gemini/models/" + gemini_models[i];
        if (EditorSettings::get_singleton()->has_setting(setting_path)) {
            bool model_enabled = EditorSettings::get_singleton()->get(setting_path);
            if (model_enabled) {
                model_selector->add_item(gemini_models[i]);
            }
        }
    }
    
    // 如果没有启用的模型，添加一个禁用项
    if (model_selector->get_item_count() == 0) {
        model_selector->add_item("No models enabled");
        model_selector->set_disabled(true);
    } else {
        model_selector->set_disabled(false);
    }
}

String AIChatPanel::get_input_text(){
    return input_box->get_text();
}
String AIChatPanel::get_model(){
    return model_selector->get_item_text(model_selector->get_selected());
}

void AIChatPanel::set_model(int index){
    model_selector->select(index);
}

int AIChatPanel::get_model_index(){
    return model_selector->get_selected();
}

void AIChatPanel::clear_text(){
    input_box->clear();
}

void AIChatPanel::set_button_enabled(bool enabled){
    send_button->set_disabled(!enabled);
}
void AIChatPanel::set_loading_bar_visible(bool i_visible){
    loading_bar->set_visible(i_visible);
}

void AIChatPanel::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_update_model_list"), &AIChatPanel::_update_model_list);
    ClassDB::bind_method(D_METHOD("get_input_text"), &AIChatPanel::get_input_text);
    ClassDB::bind_method(D_METHOD("get_model"), &AIChatPanel::get_model);
    ClassDB::bind_method(D_METHOD("clear_text"), &AIChatPanel::clear_text);
    ClassDB::bind_method(D_METHOD("set_button_enabled","enabled"), &AIChatPanel::set_button_enabled);
    ClassDB::bind_method(D_METHOD("on_send_button_pressed"), &AIChatPanel::on_send_button_pressed);
    ADD_SIGNAL(MethodInfo("send_button_pressed"));
}