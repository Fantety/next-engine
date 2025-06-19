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
    String message = input_box->get_text();
    if (message.strip_edges().is_empty()) return;

    _add_message(message, true);
    input_box->clear();
    String selected_model = model_selector->get_item_text(model_selector->get_selected());
    String apikey = EditorSettings::get_singleton()->get("deepseek/api_key");
    deepseek_api->setApiKey(apikey.utf8().get_data());

    auto callback = [this](const std::string& content, bool isFinal) {
        call_deferred("_handle_ai_response", String(content.c_str()), isFinal);
    };
    _add_message_without_flash("[AI]: ", false);
    
    // // 发送请求（包含回调函数）
    // Dictionary additionalParams;
    // additionalParams["temperature"] = 0.7;
    // additionalParams["max_tokens"] = 500;
    // 发送请求，不再需要回调
    deepseek_api->sendStreamingRequest(message.utf8().get_data());
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

void AIDock::_add_message_without_flash(const String &text, bool is_user){
    if (!chat_display) {
        return;
    }
    String current_text = chat_display->get_text();
    chat_display->set_text(current_text + text);
}

void AIDock::on_stream_response(const String &data){
    if (!chat_display) {
        return;
    }
    String current_text = chat_display->get_text();
    chat_display->set_text(current_text + data);
}

void AIDock::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_send_message"), &AIDock::_send_message);
    ClassDB::bind_method(D_METHOD("_show_settings"), &AIDock::_show_settings);
    ClassDB::bind_method(D_METHOD("_update_model_list"), &AIDock::_update_model_list);
    ClassDB::bind_method(D_METHOD("_add_message", "message", "is_user"), &AIDock::_add_message);  // 确保这个方法被绑定
    ClassDB::bind_method(D_METHOD("_handle_ai_response", "content", "is_final"), &AIDock::_handle_ai_response);
}


void AIDock::set_ai_settings_dialog(AISettingsDialog *i_dialog){
    this->dialog = i_dialog;
    dialog->connect("confirmed", callable_mp(this, &AIDock::_update_model_list));
    dialog->connect("settings_changed", callable_mp(this, &AIDock::_update_model_list));
}

void AIDock::_handle_ai_response(const String& content, bool isFinal) {
    if (isFinal) {
        // 结束响应
        _add_message("", false);
    } else {
        // 更新最后一条消息
        if (content.length() > 0) {
            _add_message_without_flash(content, false);
        }
    }
}

void AIDock::_handle_request_completed() {
    // 请求完成，可以添加清理代码
    print_line("AI request completed");
}

void AIDock::_handle_error(const String& message) {
    // 显示错误消息
    _add_message("[ERROR] " + message, false);
}

AIDock::AIDock() {
    deepseek_api = new DeepSeekAPI("deepseek-chat");
    current_ai_response = "";  // 初始化响应内容
    deepseek_api->connect("stream_response", callable_mp(this, &AIDock::on_stream_response));
}
AIDock::~AIDock() {
    if (deepseek_api) {
        memdelete(deepseek_api);
        deepseek_api = nullptr;
    }
}