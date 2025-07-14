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

// 在 ai_dock.cpp 中
void AIDock::send_or_retry_message(bool is_retry) {
    int tab_index = this->get_current_tab();
    String message = tab_index==1?chat_input_panel->get_input_text():history_input_panel->get_input_text();
    String selected_model = tab_index==1?chat_input_panel->get_model():history_input_panel->get_model();
    int selected_index = tab_index==1?chat_input_panel->get_model_index():history_input_panel->get_model_index();
    if (message.strip_edges().is_empty()&&!is_retry) {
        ERR_PRINT("Message is empty, returning");
        return;
    }

    if (!is_retry && chat_blocks.size()==0 && tab_index==1) {
        current_chat_uid = generate_uuid();
        chat_manager.create_new_chat(current_chat_uid, message);
    } else if (!is_retry && tab_index==0) {
        current_chat_uid = generate_uuid();
        chat_manager.create_new_chat(current_chat_uid, message);
        delete_all_blocks();
        set_current_tab(1);
        chat_input_panel->set_model(selected_index);
        load_historys();
    }

    chat_input_panel->set_button_enabled(false);
    history_input_panel->set_button_enabled(false);
    chat_input_panel->set_loading_bar_visible(true);

    if (!is_retry) {
        _create_chat_block(AIChatBlock::ChatType::AI_CHAT_TYPE_USER, message);
        chat_manager.add_user_chat(current_chat_uid, message);
        chat_input_panel->clear_text();
        history_input_panel->clear_text();
    }

    switch ((AIChatBlock::ChatType)AIStreamingBase::AIStringName[selected_model]) {
        case AIStreamingBase::DEEPSEEK_CHAT: {
            String apikey = EditorSettings::get_singleton()->get("deepseek/api_key");
            if (apikey.is_empty()) return;
            deepseek_api->set_apikey(apikey);
            deepseek_api->set_model("deepseek-chat");
            deepseek_api->send_streaming_request(chat_manager.get_chat(current_chat_uid));
            break;
        }
        case AIStreamingBase::DEEPSEEK_REASONER: {
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

void AIDock::_send_message() {
    send_or_retry_message(false);
}

void AIDock::_retry_message() {
    send_or_retry_message(true);
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
    else if(chat_type==AIChatBlock::ChatType::AI_CHAT_SYS_TOOL){
        block->add_text(message);
    }
    else if(chat_type==AIChatBlock::ChatType::AI_CHAT_SYS_ERROR){
        block->set_text(message);
    }
    else if(chat_type==AIChatBlock::ChatType::AI_CHAT_SYS_MESSAGE){
        block->set_text(message);
    }
    else if(chat_type==AIChatBlock::ChatType::AI_CHAT_SYS_WARNING){
        block->set_text(message);
    }
    chat_sum++;
    current_chat_index++;
    block->set_block_index(current_chat_index);
}

void AIDock::_add_message(const String &message, int block_index) {
    if(block_index < chat_blocks.size()){
        chat_blocks[block_index]->set_text(message);
    }
    else if (block_index>=chat_blocks.size()||block_index == -1)
    {
        ERR_PRINT("chat_block_index out of range");
        return;
    }
}
void AIDock::_add_reason_message(const String &message, int block_index){
    if(block_index < chat_blocks.size()){
        chat_blocks[block_index]->set_reason_text(message);
    }
    else if (block_index>=chat_blocks.size()||block_index == -1)
    {
        ERR_PRINT("chat_block_index out of range");
        return;
    }
}

void AIDock::on_streaming_response(Dictionary dict, String finish_reason) {
    chat_input_panel->set_loading_bar_visible(false);
    if (dict.has("content") && dict["content"].get_type() == Variant::STRING) {
        String content = dict["content"];
        if(content.is_empty()||content == ""){}
        else{
            if (block_create_flag) {
                _create_chat_block(AIChatBlock::ChatType::AI_CHAT_TYPE_ASSISTANT, "");
                block_create_flag = false;
            }
            _add_message(content, current_chat_index);
            chat_scroll->get_v_scroll_bar()->set_value(chat_scroll->get_v_scroll_bar()->get_max());
        }
    }
    // 处理推理内容
    if (dict.has("reasoning_content") && dict["reasoning_content"].get_type() == Variant::STRING) {
        String reasoning_content = dict["reasoning_content"];
        if(reasoning_content.is_empty()||reasoning_content == ""){}
        else{
            if (block_create_flag) {
                _create_chat_block(AIChatBlock::ChatType::AI_CHAT_TYPE_ASSISTANT, "");
                block_create_flag = false;
            }
            _add_reason_message(reasoning_content, current_chat_index);
            chat_scroll->get_v_scroll_bar()->set_value(chat_scroll->get_v_scroll_bar()->get_max());
        }
    }
    // 处理工具调用
    if (dict.has("tool_calls") && dict["tool_calls"].get_type() == Variant::ARRAY) {
        Array tool_calls = dict["tool_calls"];
        String tools_text;
        for (int i = 0; i < tool_calls.size(); i++) {
            if (tool_calls[i].get_type() == Variant::DICTIONARY) {
                Dictionary tool = tool_calls[i];
                if (tool.has("function") && tool["function"].get_type() == Variant::DICTIONARY) {
                    Dictionary function = tool["function"];
                    String name = function.get("name", "");
                    String arguments = function.get("arguments", "");
                    String id = tool.get("id", "");
                    String tool_text = "[Tool: " + name + "] [Arguments: " + arguments+"]";
                    tools_text += tool_text + "\n";
                    // 添加到IDE接口
                    if (arguments.is_empty()) {
                        ide_interface->add_tool(name, Array{}, id);
                    } else {
                        Ref<JSON> json;
                        json.instantiate();
                        Error err = json->parse(arguments);
                        if (err != OK) return;
                        Variant parsed_args = json->get_data();
                        if (parsed_args.get_type() == Variant::DICTIONARY) {
                            Dictionary args_dict = parsed_args;
                            Array args_array;
                            args_array.append(args_dict);
                            ide_interface->add_tool(name, args_array, id);
                        } else {
                            Array args_array;
                            args_array.append(arguments);
                            ide_interface->add_tool(name, args_array, id);
                        }
                    }
                }
            }
        }
        // 显示工具调用信息
        if (block_create_flag) {
            _create_chat_block(AIChatBlock::ChatType::AI_CHAT_TYPE_ASSISTANT, tools_text);
            block_create_flag = false;
        }
    }
    // 处理其他可能的信息
    if (dict.has("role") && dict["role"].get_type() == Variant::STRING) {
        String role = dict["role"];
    }
    // 处理完成状态
    if (finish_reason!="null") {
        if (finish_reason == "stop") {
            on_request_completed(AIStreamingBase::NORMAL_CHAT);
        } else if (finish_reason == "tool_calls") {
            Dictionary tool_dict = dict.duplicate();
            chat_manager.add_tool_chat(current_chat_uid, tool_dict);
            on_request_completed(AIStreamingBase::TOOL_CHAT);
        }
    }
}
void AIDock::on_data_start(){
}

void AIDock::on_request_completed(int chat_flag){
    block_create_flag = true;
    if(chat_flag == AIStreamingBase::NORMAL_CHAT){
        deepseek_api->cancel_request();
        chat_input_panel->set_button_enabled(true);
        history_input_panel->set_button_enabled(true);
        if(current_chat_index<chat_blocks.size()&&current_chat_index!=-1){
            chat_manager.add_assistant_chat(current_chat_uid, chat_blocks[current_chat_index]->get_text());
        }
        chat_manager.save_chats();
        if(!ide_interface->is_empty()){
            chat_flag = AIStreamingBase::TOOL_CHAT;
        }
    }
    if(chat_flag == AIStreamingBase::TOOL_CHAT){
        if(ide_interface->is_empty()) return;
        else{
            Dictionary dict = ide_interface->get_tool_result();
            String call_id = dict["tool_call_id"];
            String text = "Complete execute: " + call_id;
            _create_chat_block(AIChatBlock::ChatType::AI_CHAT_SYS_TOOL, text);
            chat_manager.add_tool_chat(current_chat_uid, dict);
            deepseek_api->cancel_request();
            _retry_message();
        }
    }
    if(chat_flag == AIStreamingBase::ERR_CHAT){
        ERR_PRINT("API Request END ERROR");
        _create_chat_block(AIChatBlock::ChatType::AI_CHAT_SYS_ERROR, "Something unexpected happened.");
        chat_manager.add_sys_chat(current_chat_uid, "Something unexpected happened.", "sys_error");
        chat_input_panel->set_button_enabled(true);
        history_input_panel->set_button_enabled(true);
    }
    if(chat_flag == AIStreamingBase::ERR_NETWORK){
        _create_chat_block(AIChatBlock::ChatType::AI_CHAT_SYS_ERROR, "Please check the network connection!");
        chat_manager.add_sys_chat(current_chat_uid, "Please check the network connection!", "sys_error");
        chat_input_panel->set_button_enabled(true);
        history_input_panel->set_button_enabled(true);
    }
    if(chat_flag == AIStreamingBase::ERR_TIMEOUT){
        _create_chat_block(AIChatBlock::ChatType::AI_CHAT_SYS_ERROR, "Request timed out.");
        chat_manager.add_sys_chat(current_chat_uid, "Request timed out.", "sys_error");
        chat_input_panel->set_button_enabled(true);
        history_input_panel->set_button_enabled(true);
    }
}

String AIDock::generate_uuid() const {
    return OS::get_singleton()->get_unique_id() + "_" + itos(OS::get_singleton()->get_unix_time()) + "_" + itos(rand());
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
    for (int i = 0; i < chat_sum - index; i++)
    {
        chat_blocks[chat_blocks.size()-1]->queue_free();
        chat_blocks.remove_at(chat_blocks.size()-1);
    }
    chat_sum = index;
    current_chat_index = index-1;
}

void AIDock::on_history_button_pressed(String uuid){
    delete_all_blocks();
    set_current_tab(1);
    Dictionary temp_chat_datas = chat_manager.get_chat_datas();
    Dictionary temp_data = temp_chat_datas[uuid];
    current_chat_uid = uuid;
    Array temp_array = temp_data["chats"];
    for(int i=0;i<temp_array.size();i++){
        Dictionary temp_dict = temp_array[i];
        String role = temp_dict["role"];
        String content = temp_dict["content"];

        if(role == "user"){
            _create_chat_block(AIChatBlock::ChatType::AI_CHAT_TYPE_USER, content);
        }else if(role == "assistant"){
            if (temp_dict.has("tool_calls") && temp_dict["tool_calls"].get_type() == Variant::ARRAY){
                Array tool_calls = temp_dict["tool_calls"];
                String tools_text;
                for (int i = 0; i < tool_calls.size(); i++) {
                    if (tool_calls[i].get_type() == Variant::DICTIONARY) {
                        Dictionary tool = tool_calls[i];
                        if (tool.has("function") && tool["function"].get_type() == Variant::DICTIONARY) {
                            Dictionary function = tool["function"];
                            String name = function.get("name", "");
                            String arguments = function.get("arguments", "");
                            String tool_text = "[Tool: " + name + "] [Arguments: " + arguments+"]";
                            tools_text += tool_text + "\n";
                        }
                    }
                }
                _create_chat_block(AIChatBlock::ChatType::AI_CHAT_TYPE_ASSISTANT, tools_text);
            }else{
                _create_chat_block(AIChatBlock::ChatType::AI_CHAT_TYPE_ASSISTANT, content);
            }
        }else if(role == "tool"){
            String call_id = temp_dict["tool_call_id"];
            String text = "Complete execute: " + call_id;
            _create_chat_block(AIChatBlock::ChatType::AI_CHAT_SYS_TOOL, text);
        }else if(role == "sys_tool"){
            _create_chat_block(AIChatBlock::ChatType::AI_CHAT_SYS_TOOL, content);
        }else if(role == "sys_error"){
            _create_chat_block(AIChatBlock::ChatType::AI_CHAT_SYS_ERROR, content);
        }else if(role == "sys_warning"){
            _create_chat_block(AIChatBlock::ChatType::AI_CHAT_SYS_WARNING, content);
        }
    }
}

void AIDock::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_send_message"), &AIDock::_send_message);
    ClassDB::bind_method(D_METHOD("_add_message", "message", "block_index"), &AIDock::_add_message);  // 修正参数数量
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
    deepseek_api->connect("deepseek_data_start", callable_mp(this, &AIDock::on_data_start), CONNECT_DEFERRED);
    deepseek_api->connect("deepseek_request_completed", callable_mp(this, &AIDock::on_request_completed), CONNECT_DEFERRED);
    deepseek_api->connect("deepseek_streaming_received", callable_mp(this, &AIDock::on_streaming_response), CONNECT_DEFERRED);
    print_line("AIDock constructor called finished");

    AISettingsDialog::get_singleton()->connect("confirmed", callable_mp(chat_input_panel, &AIChatPanel::_update_model_list));
    AISettingsDialog::get_singleton()->connect("ai_settings_changed", callable_mp(chat_input_panel, &AIChatPanel::_update_model_list));
    AISettingsDialog::get_singleton()->connect("confirmed", callable_mp(history_input_panel, &AIChatPanel::_update_model_list));
    AISettingsDialog::get_singleton()->connect("ai_settings_changed", callable_mp(history_input_panel, &AIChatPanel::_update_model_list));
    
    accept_dialog->connect("confirmed", callable_mp(this, &AIDock::confirm_retry));
    singleton = this;
    ide_interface = memnew(AIIDEInterface);
    load_historys();
}