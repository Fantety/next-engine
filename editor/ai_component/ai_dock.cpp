#include "ai_dock.h"
#include "editor/settings/editor_settings.h"
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
    int tab_index = this->get_current_tab();
    String message = tab_index==1?chat_input_panel->get_input_text():history_input_panel->get_input_text();
    String selected_model = tab_index==1?chat_input_panel->get_model():history_input_panel->get_model();
    int selected_index = tab_index==1?chat_input_panel->get_model_index():history_input_panel->get_model_index();
    if (message.strip_edges().is_empty()) {
        ERR_PRINT("Message is empty, returning");
        return;
    }
    if(chat_blocks.size()==0 && tab_index==1){
        current_chat_uid = generate_uuid();
        chat_manager.create_new_chat(current_chat_uid, message);
    }
    else if(tab_index==0){
        current_chat_uid = generate_uuid();
        chat_manager.create_new_chat(current_chat_uid, message);
        delete_all_blocks();
        set_current_tab(1);
        chat_input_panel->set_model(selected_index);
        load_historys();
    }
    chat_input_panel->set_button_enabled(false);
    history_input_panel->set_button_enabled(false);
    _create_chat_block(AIChatBlock::ChatType::AI_CHAT_TYPE_USER, message);
    chat_manager.add_user_chat(current_chat_uid,message);
    chat_input_panel->clear_text();
    history_input_panel->clear_text();
    switch ((AIChatBlock::ChatType)AIStreamingBase::AIStringName[selected_model])
    {
        case AIStreamingBase::DEEPSEEK_CHAT:{
            String apikey = EditorSettings::get_singleton()->get("deepseek/api_key");
            if (apikey.is_empty()) return;
            deepseek_api->set_apikey(apikey);
            deepseek_api->set_model("deepseek-chat");
            deepseek_api->send_streaming_request(chat_manager.get_chat(current_chat_uid));
            break;
        }
        case AIStreamingBase::DEEPSEEK_REASONER:{
            String apikey = EditorSettings::get_singleton()->get("deepseek/api_key");
            if (apikey.is_empty()) return;
            deepseek_api->set_apikey(apikey);
            deepseek_api->set_model("deepseek-reasoner");
            deepseek_api->send_streaming_request(chat_manager.get_chat(current_chat_uid));
            break;
        }
        default:
            break;
    }
}

void AIDock::_retry_message(){
    int tab_index = this->get_current_tab();
    String message = tab_index==1?chat_input_panel->get_input_text():history_input_panel->get_input_text();
    String selected_model = tab_index==1?chat_input_panel->get_model():history_input_panel->get_model();
    int selected_index = tab_index==1?chat_input_panel->get_model_index():history_input_panel->get_model_index();
    chat_input_panel->set_button_enabled(false);
    history_input_panel->set_button_enabled(false); 
    switch ((AIChatBlock::ChatType)AIStreamingBase::AIStringName[selected_model])
    {
        case AIStreamingBase::DEEPSEEK_CHAT:{
            String apikey = EditorSettings::get_singleton()->get("deepseek/api_key");
            if (apikey.is_empty()) return;
            deepseek_api->set_apikey(apikey);
            deepseek_api->set_model("deepseek-chat");
            deepseek_api->send_streaming_request(chat_manager.get_chat(current_chat_uid));
            break;
        }
        case AIStreamingBase::DEEPSEEK_REASONER:{
            String apikey = EditorSettings::get_singleton()->get("deepseek/api_key");
            if (apikey.is_empty()) return;
            deepseek_api->set_apikey(apikey);
            deepseek_api->set_model("deepseek-reasoner");
            deepseek_api->send_streaming_request(chat_manager.get_chat(current_chat_uid));
            break;
        }
        default:
            break;
    }
}

void AIDock::_create_chat_block(AIChatBlock::ChatType chat_type, const String& message){
    AIChatBlock* block = memnew(AIChatBlock(chat_type));
    block->set_fit_content(true);
    block->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    chat_blocks.append(block);
    chat_list->add_child(block);
    block->connect("retry_pressed", callable_mp(this, &AIDock::on_retry_pressed));
    if(chat_type==AIChatBlock::ChatType::AI_CHAT_TYPE_USER){
        block->set_text(message);
    }
    else if(chat_type==AIChatBlock::ChatType::AI_CHAT_TYPE_ASSISTANT){
        block->set_text("");
        block->add_text(message);
    }
    chat_sum++;
    current_chat_index++;
    block->set_block_index(current_chat_index);
}

void AIDock::_add_message(const String &message, int block_index) {
    if(block_index < chat_blocks.size()){
        chat_blocks[block_index]->add_text(message);
    }
    else if (block_index>=chat_blocks.size()||block_index == -1)
    {
        ERR_PRINT("chat_block_index out of range");
        return;
    }
}
void AIDock::_add_reason_message(const String &message, int block_index){
    if(block_index < chat_blocks.size()){
        chat_blocks[block_index]->add_reason_text(message);
    }
    else if (block_index>=chat_blocks.size()||block_index == -1)
    {
        ERR_PRINT("chat_block_index out of range");
        return;
    }
}

void AIDock::on_stream_response(String text){
    _add_message(text, current_chat_index);
    chat_scroll->get_v_scroll_bar()->set_value(chat_scroll->get_v_scroll_bar()->get_max());
}

void AIDock::on_reason_response(String text){
    _add_reason_message(text, current_chat_index);
    chat_scroll->get_v_scroll_bar()->set_value(chat_scroll->get_v_scroll_bar()->get_max());
}

void AIDock::on_data_start(){
    _create_chat_block(AIChatBlock::ChatType::AI_CHAT_TYPE_ASSISTANT, "");
}

void AIDock::on_request_completed(){
    deepseek_api->cancel_request();
    print_line(JSON::stringify(chat_manager.get_chat(current_chat_uid)));
    chat_input_panel->set_button_enabled(true);
    history_input_panel->set_button_enabled(true);
    if(current_chat_index<chat_blocks.size()&&current_chat_index!=-1){
        chat_manager.add_assistant_chat(current_chat_uid, chat_blocks[current_chat_index]->get_text());
        chat_blocks[current_chat_index]->to_bbcode();
    }
    chat_manager.save_chats();
}


void AIDock::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_send_message"), &AIDock::_send_message);
    ClassDB::bind_method(D_METHOD("_add_message", "message", "block_index"), &AIDock::_add_message);  // 修正参数数量
}




String AIDock::generate_uuid() const {
    return OS::get_singleton()->get_unique_id() + "_" + itos(OS::get_singleton()->get_unix_time()) + "_" + itos(rand());
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
    history_view->add_theme_constant_override("separation",40);
    history_list = memnew(VBoxContainer);
    history_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    history_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    //history_list->connect("item_selected", callable_mp(this, &AIDock::_on_history_selected));
    history_list->set_stretch_ratio(4);
    history_view->add_child(history_list);
    print_line("history_view Init Finished");
    // 聊天显示区域
    chat_view = memnew(VBoxContainer);
    accept_dialog = memnew(AIAcceptDialog);
    add_child(accept_dialog);
    chat_scroll = memnew(ScrollContainer);
    chat_scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    chat_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    chat_scroll->set_stretch_ratio(4);
    chat_list = memnew(VBoxContainer);
    chat_list->add_theme_constant_override("separation",10);
    chat_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    chat_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    chat_scroll->add_child(chat_list);
    chat_view->add_child(chat_scroll);
    chat_view->add_child(chat_input_panel);
    print_line("AIDock Init Finish");
    print_line("AIDock constructor called");
    deepseek_api = memnew(DeepSeekAPI("deepseek-chat"));
    if (!deepseek_api) {
        print_line("Failed to create deepseek_api");
    }
    add_child(deepseek_api);
    current_ai_response = "";  // 初始化响应内容
    deepseek_api->connect("deepseek_data_received", callable_mp(this, &AIDock::on_stream_response), CONNECT_DEFERRED);
    deepseek_api->connect("deepseek_reason_received", callable_mp(this, &AIDock::on_reason_response), CONNECT_DEFERRED);
    deepseek_api->connect("deepseek_data_start", callable_mp(this, &AIDock::on_data_start), CONNECT_DEFERRED);
    deepseek_api->connect("deepseek_request_completed", callable_mp(this, &AIDock::on_request_completed), CONNECT_DEFERRED);
    print_line("AIDock constructor called finished");

    AISettingsDialog::get_singleton()->connect("confirmed", callable_mp(chat_input_panel, &AIChatPanel::_update_model_list));
    AISettingsDialog::get_singleton()->connect("ai_settings_changed", callable_mp(chat_input_panel, &AIChatPanel::_update_model_list));
    AISettingsDialog::get_singleton()->connect("confirmed", callable_mp(history_input_panel, &AIChatPanel::_update_model_list));
    AISettingsDialog::get_singleton()->connect("ai_settings_changed", callable_mp(history_input_panel, &AIChatPanel::_update_model_list));
    
    accept_dialog->connect("confirmed", callable_mp(this, &AIDock::confirm_retry));
    singleton = this;
    load_historys();
}


AIDock* AIDock::get_singleton(){
    return singleton;
}


void AIDock::delete_all_blocks(){
    if(chat_list->get_child_count()==0){
        return;
    }
    for(int i=0;i<chat_list->get_child_count();i++){
        chat_list->remove_child(chat_list->get_child(i));
    }
    if(chat_blocks.size()==0){
        return;
    }
    for(int i=0;i<chat_blocks.size();i++){
        chat_blocks[i]->queue_free();
    }
    chat_sum = 0;
    current_chat_index = -1;
    chat_blocks.clear();
}

void AIDock::on_retry_pressed(int chat_type, int block_index){
    retry_chat_type = chat_type;
    retry_block_index = block_index;
    accept_dialog->set_dialog_message("This operation will remove all previous messages, please confirm whether to continue.");
    accept_dialog->popup_centered();
}

void AIDock::confirm_retry(){
    switch (retry_chat_type)
    {
        case AIChatBlock::ChatType::AI_CHAT_TYPE_USER:{
            /* code */
            break;
        }
        case AIChatBlock::ChatType::AI_CHAT_TYPE_ASSISTANT:{
            Array prompt = chat_manager.get_chat_before_index(current_chat_uid, retry_block_index);
            delete_blocks_after_index(retry_block_index);
            _retry_message();
            break;
        }
        default:
            break;
    }
}


void AIDock::load_historys(){
    if(history_buttons.size()!= 0 ){
        for(int i=0;i<history_buttons.size();i++){
            history_buttons[i]->queue_free();
        }
        history_buttons.clear();
    }
    Dictionary temp_chat_datas = chat_manager.get_chat_datas();
    Array chat_keys = temp_chat_datas.keys();
    for(int i=0;i<chat_keys.size();i++){
        AIHistoryButton* history_button = memnew(AIHistoryButton(chat_keys[i]));
        history_button->connect("history_button_pressed", callable_mp(this, &AIDock::on_history_button_pressed));
        Dictionary temp_data = temp_chat_datas[chat_keys[i]];
        history_button->set_text(temp_data["title"]);
        history_buttons.push_back(history_button);
        history_list->add_child(history_button);
    }
}

void AIDock::delete_blocks_after_index(int index){
    for (int i = index; i < chat_sum; i++)
    {
        chat_blocks[i]->queue_free();
        chat_blocks.remove_at(i);
    }
    chat_sum = index;
    current_chat_index = index-1;
}


void AIDock::on_history_button_pressed(String uuid){
    delete_all_blocks();
    set_current_tab(1);
    Dictionary temp_chat_datas = chat_manager.get_chat_datas();
    Dictionary temp_data = temp_chat_datas[uuid];
    Array temp_array = temp_data["chats"];
    for(int i=0;i<temp_array.size();i++){
        Dictionary temp_dict = temp_array[i];
        String role = temp_dict["role"];
        String content = temp_dict["content"];
        if(role == "user"){
            _create_chat_block(AIChatBlock::ChatType::AI_CHAT_TYPE_USER, content);
        }else if(role == "assistant"){
            _create_chat_block(AIChatBlock::ChatType::AI_CHAT_TYPE_ASSISTANT, content);
        }
    }
}