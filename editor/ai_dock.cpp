#include "ai_dock.h"
#include "editor/editor_settings.h"
#include "editor/editor_node.h"
#include "ai_settings_dialog.h"

void AIDock::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY: {
            // 聊天显示区域
            chat_display = memnew(TextEdit);
            chat_display->set_custom_minimum_size(Size2(0, 200));
            chat_display->set_v_size_flags(Control::SIZE_EXPAND_FILL);
            chat_display->set_editable(false);
            chat_display->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
            add_child(chat_display);

            // 模型选择器
            HBoxContainer *selector_container = memnew(HBoxContainer);
            add_child(selector_container);
            
            Label *model_label = memnew(Label);
            model_label->set_text("Model:");
            selector_container->add_child(model_label);
            
            model_selector = memnew(OptionButton);
            model_selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            _update_model_list(); // 初始化模型列表
            selector_container->add_child(model_selector);

            // 输入区域
            HBoxContainer *input_container = memnew(HBoxContainer);
            input_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            
            input_box = memnew(LineEdit);
            input_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            input_box->set_placeholder("Type your message...");
            input_box->connect("text_submitted", callable_mp(this, &AIDock::_send_message));
            input_container->add_child(input_box);

            send_button = memnew(Button);
            send_button->set_text("Send");
            send_button->connect("pressed", callable_mp(this, &AIDock::_send_message));
            input_container->add_child(send_button);

            settings_button = memnew(Button);
            settings_button->set_text("Settings");
            settings_button->connect("pressed", callable_mp(this, &AIDock::_show_settings));
            input_container->add_child(settings_button);

            add_child(input_container);
        } break;
    }
}

void AIDock::_update_model_list() {
    model_selector->clear();
    const String deepseek_models[] = {"deepseek-chat", "deepseek-reason", "deepseek-coder"};
    for (int i = 0; i < 3; i++) {
        String setting_path = "deepseek/models/" + deepseek_models[i];
        if (EditorSettings::get_singleton()->has_setting(setting_path)) {
            bool model_enabled = EditorSettings::get_singleton()->get(setting_path);
            if (model_enabled) {
                model_selector->add_item("DeepSeek: " + deepseek_models[i]);
            }
        }
    }
    const String openai_models[] = {"gpt-4-turbo", "gpt-4", "gpt-3.5-turbo"};
    for (int i = 0; i < 3; i++) {
        String setting_path = "openai/models/" + openai_models[i];
        if (EditorSettings::get_singleton()->has_setting(setting_path)) {
            bool model_enabled = EditorSettings::get_singleton()->get(setting_path);
            if (model_enabled) {
                model_selector->add_item("OpenAI: " + openai_models[i]);
            }
        }
    }

    const String gemini_models[] = {"gemini-pro", "gemini-ultra", "gemini-nano"};
    for (int i = 0; i < 3; i++) {
        String setting_path = "gemini/models/" + gemini_models[i];
        if (EditorSettings::get_singleton()->has_setting(setting_path)) {
            bool model_enabled = EditorSettings::get_singleton()->get(setting_path);
            if (model_enabled) {
                model_selector->add_item("Gemini: " + gemini_models[i]);
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

void AIDock::_send_message() {
    String text = input_box->get_text().strip_edges();
    if (text.is_empty()) {
        return;
    }

    String selected_model = model_selector->get_item_text(model_selector->get_selected());
    _add_message("[You @ " + selected_model + "]: " + text, true);
    input_box->clear();
    
    // 这里可以添加根据选择模型调用不同API的逻辑
    _add_message("[AI @ " + selected_model + "]: This is a sample response", false);
}

void AIDock::_show_settings() {
    dialog->popup_centered();
}

void AIDock::_add_message(const String &message, bool is_user) {
    if (!chat_display) {
        return;
    }
    String current_text = chat_display->get_text();
    chat_display->set_text(current_text + message + "\n\n");
    chat_display->set_caret_line(chat_display->get_line_count() - 1);
}

void AIDock::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_send_message"), &AIDock::_send_message);
    ClassDB::bind_method(D_METHOD("_show_settings"), &AIDock::_show_settings);
    ClassDB::bind_method(D_METHOD("_update_model_list"), &AIDock::_update_model_list);
    ClassDB::bind_method(D_METHOD("_add_message", "message", "is_user"), &AIDock::_add_message);  // 确保这个方法被绑定
}


void AIDock::set_ai_settings_dialog(AISettingsDialog *i_dialog){
    this->dialog = i_dialog;
}

AIDock::AIDock() {
    dialog->connect("confirmed", callable_mp(this, &AIDock::_update_model_list));
    dialog->connect("settings_changed", callable_mp(this, &AIDock::_update_model_list));
}
AIDock::~AIDock() {}