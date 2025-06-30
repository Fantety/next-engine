#include "ai_dock.h"
#include "editor/editor_settings.h"
#include "editor/editor_node.h"
#include "ai_settings_dialog.h"

void AIDock::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY: {
            add_child(history_view);
            add_child(chat_view);
            set_tab_title(0, "History");
            set_tab_title(1, "Chat");
        } break;
    }
}

void AIDock::_send_message() {
    print_line("_send_message called");
    int tab_index = this->get_current_tab();
    String message = tab_index==1?chat_input_panel->get_input_text():history_input_panel->get_input_text();
    if (message.strip_edges().is_empty()) {
        print_line("Message is empty, returning");
        return;
    }
    if(chat_blocks.size()==0){
        String uid = generate_uuid();
        current_chat_uid = uid;
        print_line(String("Creating new chat with UID: ") + uid);
        chat_manager.create_new_chat(uid);
    }
    tab_index==1?chat_input_panel->set_button_enabled(false):history_input_panel->set_button_enabled(false);
    current_chat_index++;
    print_line(String("Adding user message, chat index: ") + itos(current_chat_index));
    _add_message(message,current_chat_index, true);
    chat_manager.add_user_chat(current_chat_uid,message);
    tab_index==1?chat_input_panel->clear_text():history_input_panel->clear_text();
    String selected_model = tab_index==1?chat_input_panel->get_model():history_input_panel->get_model();
    String apikey = EditorSettings::get_singleton()->get("deepseek/api_key");
    if (apikey.is_empty()) {
        print_line("DeepSeek API key is empty");
    }
    deepseek_api->set_apikey(apikey.utf8().get_data());
    current_chat_index++;
    print_line(String("Sending streaming request, chat index: ") + itos(current_chat_index));
    print_line(JSON::stringify(chat_manager.get_chat(current_chat_uid)));
    deepseek_api->send_streaming_request(chat_manager.get_chat(current_chat_uid));
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
    this->get_current_tab()==1?chat_input_panel->set_button_enabled(true):history_input_panel->set_button_enabled(true);
    if(current_chat_index<chat_blocks.size()&&current_chat_index!=-1){
        chat_manager.add_assistant_chat(current_chat_uid, chat_blocks[current_chat_index]->get_text());
        chat_blocks[current_chat_index]->to_bbcode();
    }
    //print_line(JSON::stringify(chat_manager.get_chat(current_chat_uid)));
}


void AIDock::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_send_message"), &AIDock::_send_message);
    ClassDB::bind_method(D_METHOD("_add_message", "message", "block_index", "is_user"), &AIDock::_add_message);  // 修正参数数量
    ClassDB::bind_method(D_METHOD("_handle_ai_response", "content", "is_final"), &AIDock::_handle_ai_response);
    ClassDB::bind_method(D_METHOD("_handle_request_completed"), &AIDock::_handle_request_completed);
    ClassDB::bind_method(D_METHOD("set_ai_settings_dialog", "i_dialog"), &AIDock::set_ai_settings_dialog);
}


void AIDock::set_ai_settings_dialog(AISettingsDialog *i_dialog){
    this->dialog = i_dialog;
    dialog->connect("confirmed", callable_mp(chat_input_panel, &AIChatPanel::_update_model_list));
    dialog->connect("ai_settings_changed", callable_mp(chat_input_panel, &AIChatPanel::_update_model_list));
    dialog->connect("confirmed", callable_mp(history_input_panel, &AIChatPanel::_update_model_list));
    dialog->connect("ai_settings_changed", callable_mp(history_input_panel, &AIChatPanel::_update_model_list));
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

    print_line("AIDock Init Start");
    chat_input_panel = memnew(AIChatPanel);
    history_input_panel = memnew(AIChatPanel);
    chat_input_panel->connect("send_button_pressed", callable_mp(this, &AIDock::_send_message));
    history_input_panel->connect("send_button_pressed", callable_mp(this, &AIDock::_send_message));
    print_line("AIChatPanel Init Finished");
    //历史页面
    history_view = memnew(VBoxContainer);
    history_view->add_child(history_input_panel);
    history_list = memnew(ItemList);
    history_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    history_list->connect("item_selected", callable_mp(this, &AIDock::_on_history_selected));
    history_list->set_stretch_ratio(4);
    history_view->add_child(history_list);
    print_line("history_view Init Finished");
    // 聊天显示区域
    chat_view = memnew(VBoxContainer);
    
    chat_scroll = memnew(ScrollContainer);
    chat_scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    chat_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    chat_scroll->set_stretch_ratio(4);
    chat_list = memnew(VBoxContainer);
    chat_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    chat_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    chat_scroll->add_child(chat_list);
    chat_view->add_child(chat_scroll);
    chat_view->add_child(chat_input_panel);
    print_line("AIDock Init Finish");
    print_line("AIDock constructor called");
    deepseek_api = new DeepSeekAPI("deepseek-chat");
    if (!deepseek_api) {
        print_line("Failed to create deepseek_api");
    }
    add_child(deepseek_api);
    current_ai_response = "";  // 初始化响应内容
    deepseek_api->connect("deepseek_data_received", callable_mp(this, &AIDock::on_stream_response), CONNECT_DEFERRED);
    deepseek_api->connect("deepseek_data_updated", callable_mp(this, &AIDock::on_data_updated), CONNECT_DEFERRED);
    deepseek_api->connect("deepseek_request_completed", callable_mp(this, &AIDock::on_request_completed), CONNECT_DEFERRED);
    print_line("AIDock constructor called finished");
    
}



