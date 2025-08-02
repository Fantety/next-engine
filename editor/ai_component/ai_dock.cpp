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
    current_tool_str = "";
    int tab_index = this->get_current_tab();
    String message = "<question>" + (tab_index==1?chat_input_panel->get_input_text():history_input_panel->get_input_text()) + "</question>";
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
            openai_api->set_apikey(apikey);
            openai_api->set_model("deepseek-chat");
            openai_api->send_streaming_request(chat_manager.get_chat(current_chat_uid));
            break;
        }
        case AIStreamingBase::DEEPSEEK_REASONER: {
            String apikey = EditorSettings::get_singleton()->get("deepseek/api_key");
            if (apikey.is_empty()) return;
            openai_api->set_apikey(apikey);
            openai_api->set_model("deepseek-reasoner");
            openai_api->send_streaming_request(chat_manager.get_chat(current_chat_uid));
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

void AIDock::_toggle_mcp_server(bool p_toggled) {
    if (p_toggled) {
        Error err = mcp_server->start_server(3000);
        if (err == OK) {
            mcp_toggle_button->set_text("Stop MCP Server");
        } else {
            // 显示错误信息
            print_line("Failed to start MCP server: " + itos(err));
            mcp_toggle_button->set_pressed(false);
        }
    } else {
        mcp_server->stop_server();
        mcp_toggle_button->set_text("Start MCP Server");
    }
}


void AIDock::_create_chat_block(AIChatBlock::ChatType chat_type, const String& thought, const String& tool, const String& final_answer){
    if(block_create_flag){
        AIChatBlock* block = memnew(AIChatBlock(chat_type));
        current_chat_block = block;
        block->set_fit_content(true);
        block->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        chat_blocks.append(block);
        chat_list->add_child(block);
        block->connect("retry_pressed", callable_mp(this, &AIDock::on_retry_pressed));
        if(chat_type==AIChatBlock::ChatType::AI_CHAT_TYPE_USER){
            block->set_text(thought);
        }
        else if(chat_type==AIChatBlock::ChatType::AI_CHAT_TYPE_ASSISTANT){
            block->set_text(thought);
            if(!tool.empty()){
                block->set_tool_content(tool);
            }
            if(!final_answer.empty()){
                block->set_final_answer_content(final_answer);
            }
        }
        else if(chat_type==AIChatBlock::ChatType::AI_CHAT_SYS_ERROR){
            block->set_text(thought);
        }
        else if(chat_type==AIChatBlock::ChatType::AI_CHAT_SYS_MESSAGE){
            block->set_text(thought);
        }
        else if(chat_type==AIChatBlock::ChatType::AI_CHAT_SYS_WARNING){
            block->set_text(thought);
        }
        chat_sum++;
        current_chat_index++;
        block->set_block_index(current_chat_index);
    }
    else{
        if(current_chat_block){
            current_chat_block->set_text(thought);
            current_chat_block->set_tool_content(tool);
            current_chat_block->set_final_answer_content(final_answer);
        }
    }
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

void AIDock::on_streaming_response(String content, String finish_reason) {
    if(finish_reason.is_empty()){
        chat_manager.add_assistant_chat(current_chat_uid, stream_processor->get_full_content());
        on_request_completed(AIStreamingBase::NORMAL_CHAT);
        if(!current_tool_str.is_empty()){
            engine_operator->add_tool_with_parse(current_tool_str);
            on_request_completed(AIStreamingBase::TOOL_CHAT);
        }
    }
    else{
        if (!content.is_empty()) {
            stream_processor->process_stream_data(content);
            // 获取特定标签的内容
            String thought = stream_processor->get_tag_content("thought");
            String tool = stream_processor->get_tag_content("tool");
            String final_answer = stream_processor->get_tag_content("final_answer");
            if(!thought.is_empty()){
                _create_chat_block(AIChatBlock::ChatType::AI_CHAT_TYPE_ASSISTANT, thought, tool, final_answer);
            }
            if(!tool.is_empty()){
                current_tool_str = tool;
            }
        }
    }
}


void AIDock::on_data_start(){
}

void AIDock::on_request_completed(int chat_flag){
    block_create_flag = true;
    //print_line("on_request_completed");
    if(chat_flag == AIStreamingBase::NORMAL_CHAT){
        print_line("on_request_completed: AIStreamingBase::NORMAL_CHAT");
        openai_api->cancel_request();
        chat_input_panel->set_button_enabled(true);
        history_input_panel->set_button_enabled(true);
        if(current_chat_index<chat_blocks.size()&&current_chat_index!=-1){
            chat_manager.add_assistant_chat(current_chat_uid, chat_blocks[current_chat_index]->get_text());
        }
        chat_manager.save_chats();
        if(!engine_operator->is_empty()){
            chat_flag = AIStreamingBase::TOOL_CHAT;
        }
    }
    if(chat_flag == AIStreamingBase::TOOL_CHAT){
        print_line("on_request_completed: AIStreamingBase::TOOL_CHAT");
        if(engine_operator->is_empty()) return;
        else{
            Dictionary dict = engine_operator->get_tool_result();
            chat_manager.add_tool_chat(current_chat_uid, dict);
            openai_api->cancel_request();
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
    if(temp_chat_datas.is_empty()) return;
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
    ClassDB::bind_method(D_METHOD("_toggle_mcp_server", "toggled"), &AIDock::_toggle_mcp_server);
}

AIDock::AIDock() {
    print_line("AIDock Init Start");
    
    // 创建MCP开关按钮
    mcp_toggle_button = memnew(CheckButton);
    mcp_toggle_button->set_text("Start MCP Server");
    mcp_toggle_button->connect("toggled", callable_mp(this, &AIDock::_toggle_mcp_server));
    
    // 将MCP开关按钮添加到历史视图的顶部
    history_view = memnew(VBoxContainer);
    history_view->add_child(mcp_toggle_button);
    history_view->add_child(history_input_panel);
    history_view->add_theme_constant_override("separation",40);
    
    //历史页面
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
    
    // 初始化DeepSeek API
    deepseek_api = memnew(DeepSeekAPI("deepseek-chat"));
    if (!deepseek_api) {
        print_line("Failed to create deepseek_api");
    }
    add_child(deepseek_api);
    
    deepseek_api->connect("deepseek_data_start", callable_mp(this, &AIDock::on_data_start), CONNECT_DEFERRED);
    deepseek_api->connect("deepseek_request_completed", callable_mp(this, &AIDock::on_request_completed), CONNECT_DEFERRED);
    deepseek_api->connect("deepseek_streaming_received", callable_mp(this, &AIDock::on_streaming_response), CONNECT_DEFERRED);
    
    current_ai_response = "";  // 初始化响应内容
    // 初始化OpenAI API
    openai_api = memnew(OpenAIRequestHandler("deepseek-chat"));
    if (!openai_api) {
        print_line("Failed to create openai_api");
    }
    add_child(openai_api);
    stream_processor = memnew(AIStreamProcessor());

    openai_api->connect("openai_data_start", callable_mp(this, &AIDock::on_data_start), CONNECT_DEFERRED);
    openai_api->connect("openai_request_completed", callable_mp(this, &AIDock::on_request_completed), CONNECT_DEFERRED);
    openai_api->connect("openai_streaming_received", callable_mp(this, &AIDock::on_streaming_response), CONNECT_DEFERRED);
    
    // 初始化MCP服务器
    mcp_server = memnew(MCPServer);
    if (!mcp_server) {
        print_line("Failed to create mcp_server");
    }
    add_child(mcp_server);
    
    print_line("AIDock constructor called finished");

    AISettingsDialog::get_singleton()->connect("confirmed", callable_mp(chat_input_panel, &AIChatPanel::_update_model_list));
    AISettingsDialog::get_singleton()->connect("ai_settings_changed", callable_mp(chat_input_panel, &AIChatPanel::_update_model_list));
    AISettingsDialog::get_singleton()->connect("confirmed", callable_mp(history_input_panel, &AIChatPanel::_update_model_list));
    AISettingsDialog::get_singleton()->connect("ai_settings_changed", callable_mp(history_input_panel, &AIChatPanel::_update_model_list));
    
    accept_dialog->connect("confirmed", callable_mp(this, &AIDock::confirm_retry));
    singleton = this;
    engine_operator = memnew(EngineOperator);
    load_historys();
}