#include "ai_dock.h"
#include "editor/editor_settings.h"
#include "editor/editor_node.h"
#include "ai_settings_dialog.h"

void AIDock::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY: {
            //历史页面
            history_view = memnew(VBoxContainer);
            add_child(history_view);
            set_tab_title(0, "History");
            search_box = memnew(LineEdit);
            search_box->set_placeholder("Search history...");
            history_view->add_child(search_box);

            history_list = memnew(ItemList);
            history_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
            history_list->connect("item_selected", callable_mp(this, &AIDock::_on_history_selected));
            history_view->add_child(history_list);
            new_chat_btn = memnew(Button);
            new_chat_btn->set_text("New Chat");
            new_chat_btn->connect("pressed", callable_mp(this, &AIDock::_start_new_chat));
            history_view->add_child(new_chat_btn);

            // 聊天显示区域
            chat_view = memnew(VBoxContainer);
            chat_scroll = memnew(ScrollContainer);
            chat_list = memnew(VBoxContainer);
            add_child(chat_view);
            set_tab_title(1, "Chat");
            chat_scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            chat_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
            chat_scroll->set_stretch_ratio(4);
            chat_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            chat_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
            chat_view->add_child(chat_scroll);
            chat_scroll->add_child(chat_list);

            // 模型选择器
            HBoxContainer *selector_container = memnew(HBoxContainer);
            Label *model_label = memnew(Label);
            model_label->set_text("Model:");
            selector_container->add_child(model_label);
            model_selector = memnew(OptionButton);
            model_selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            _update_model_list(); // 初始化模型列表
            selector_container->add_child(model_selector);
            selector_container->set_stretch_ratio(4);

            // 输入区域
            input_panel = memnew(PanelContainer);
            input_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            input_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
            input_panel->set_stretch_ratio(1);

            input_view = memnew(VBoxContainer);
            input_view->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            input_view->set_v_size_flags(Control::SIZE_EXPAND_FILL);

            input_bottom_bar = memnew(HBoxContainer);
            input_bottom_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            input_bottom_bar->set_v_size_flags(Control::SIZE_EXPAND_FILL);
            input_bottom_bar->set_stretch_ratio(1);

            send_button = memnew(Button);
            send_button->set_text("Send");
            send_button->connect("pressed", callable_mp(this, &AIDock::_send_message));
            send_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            send_button->set_stretch_ratio(1);

            input_box = memnew(TextEdit);
            input_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            input_box->set_v_size_flags(Control::SIZE_EXPAND_FILL);
            input_box->set_placeholder("Type your message...");
            input_box->set_stretch_ratio(4);

            input_bottom_bar->add_child(selector_container);
            input_bottom_bar->add_child(send_button);
            chat_view->add_child(input_panel);
            input_panel->add_child(input_view);
            input_view->add_child(input_box);
            input_view->add_child(input_bottom_bar);
            
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
    send_button->set_disabled(true);
    current_chat_index++;
    _add_message(message,current_chat_index, true);
    input_box->clear();
    String selected_model = model_selector->get_item_text(model_selector->get_selected());
    String apikey = EditorSettings::get_singleton()->get("deepseek/api_key");
    deepseek_api->setApiKey(apikey.utf8().get_data());
    current_chat_index++;
    deepseek_api->sendStreamingRequest(message);
}
void AIDock::_add_message(const String &message, int block_index, bool is_user) {
    if(block_index==chat_blocks.size()){
        AIChatBlock* block = memnew(AIChatBlock);
        block->set_fit_content(true);
        block->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        chat_blocks.append(block);
        chat_list->add_child(block);
    }
    else if (block_index>chat_blocks.size()||block_index == -1)
    {
        ERR_PRINT("chat_block_index out of range");
        return;
    }
    chat_blocks[block_index]->add_text(message);
}


void AIDock::on_stream_response(String text){
    _add_message(text, current_chat_index, false);
}

void AIDock::on_data_updated(){
    //print_line("updated");
}
void AIDock::on_request_completed(){
    deepseek_api->cancel_request();
    send_button->set_disabled(false);
    if(current_chat_index<chat_blocks.size()&&current_chat_index!=-1)
        chat_blocks[current_chat_index]->to_bbcode();
}


void AIDock::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_send_message"), &AIDock::_send_message);
    ClassDB::bind_method(D_METHOD("_update_model_list"), &AIDock::_update_model_list);
    ClassDB::bind_method(D_METHOD("_add_message", "message", "is_user"), &AIDock::_add_message);  // 确保这个方法被绑定
    ClassDB::bind_method(D_METHOD("_handle_ai_response", "content", "is_final"), &AIDock::_handle_ai_response);
}


void AIDock::set_ai_settings_dialog(AISettingsDialog *i_dialog){
    this->dialog = i_dialog;
    dialog->connect("confirmed", callable_mp(this, &AIDock::_update_model_list));
    dialog->connect("ai_settings_changed", callable_mp(this, &AIDock::_update_model_list));
}

void AIDock::_handle_ai_response(const String& content, bool isFinal) {
    if (isFinal) {
        // 结束响应
        //_add_message("", false);
    } else {
        // 更新最后一条消息
        if (content.length() > 0) {
            //_add_message_without_flash(content, false);
        }
    }
}

String AIDock::generate_uuid() const {
    return OS::get_singleton()->get_unique_id() + "_" + itos(OS::get_singleton()->get_unix_time()) + "_" + itos(rand());
}

void AIDock::_start_new_chat() {
    ChatSession new_session;
    new_session.uuid = generate_uuid();
    new_session.timestamp = OS::get_singleton()->get_unix_time();
    sessions.push_back(new_session);
    _update_history_list();
    set_current_tab(1);
}


void AIDock::_on_history_selected(int index) {
    if (index >= 0 && index < sessions.size()) {
        current_session = sessions[index].uuid;
        //chat_display->set_text(String("\n").join(sessions[index].messages));
        set_current_tab(1);
    }
}
void AIDock::_update_history_list(){

}

void AIDock::_handle_request_completed() {
    // 请求完成，可以添加清理代码
    print_line("AI request completed");
}

void AIDock::_handle_error(const String& message) {
    // 显示错误消息
    //_add_message("[ERROR] " + message, false);
}

AIDock::AIDock() {
    deepseek_api = new DeepSeekAPI("deepseek-chat");
    add_child(deepseek_api);
    current_ai_response = "";  // 初始化响应内容
    deepseek_api->connect("deepseek_data_received", callable_mp(this, &AIDock::on_stream_response), CONNECT_DEFERRED);
    deepseek_api->connect("deepseek_data_updated", callable_mp(this, &AIDock::on_data_updated), CONNECT_DEFERRED);
    deepseek_api->connect("deepseek_request_completed", callable_mp(this, &AIDock::on_request_completed), CONNECT_DEFERRED);
}


