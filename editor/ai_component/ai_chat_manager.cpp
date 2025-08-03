/*
 * @FilePath: \editor\ai_component\ai_chat_manager.cpp
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-07-10 18:34:02
 * @LastEditors: Fantety
 * @LastEditTime: 2025-08-03 16:34:16
 */
#include "ai_chat_manager.h"
#include "core/io/json.h"
#include "core/os/os.h"

AIChatManager::AIChatManager(/* args */)
{
    load_chats();
}

void AIChatManager::save_chats(){
    Ref<FileAccess> file = FileAccess::open("user://chat_datas.json", FileAccess::WRITE);
    if (file.is_null()) {
        Error err = FileAccess::get_open_error();
        return;
    }
    String data = JSON::stringify(chat_datas);
    // 写入文本内容
    file->store_string(data);
}

void AIChatManager::load_chats(){
    if(!FileAccess::exists("user://chat_datas.json")) return;
    Ref<FileAccess> file = FileAccess::open("user://chat_datas.json", FileAccess::READ);
    if (file.is_null()) {
        Error err = FileAccess::get_open_error();
        ERR_PRINT("Failed to open file");
        return; // 返回空字符串表示失败
    }
    String jsonString = file->get_as_text();
    if(jsonString.is_empty()) return;
    // 检查读取是否成功
    if (file->get_error() != OK) {
        ERR_PRINT("Error reading file");
        return;
    }
    Ref<JSON> json;
    json.instantiate();
    Error err = json->parse(jsonString);
    if (err != OK) return;
    Variant json_data = json->get_data();
    if (json_data.get_type() == Variant::DICTIONARY) {
        chat_datas.clear();
        chat_datas = json_data;
    }
}
void AIChatManager::create_new_chat(const String uid, const String title){
    Dictionary new_chat;
    new_chat["title"] = title;
    Array new_chat_array;
    new_chat["chats"] = new_chat_array;
    chat_datas[uid] = new_chat;
}

void AIChatManager::add_user_chat(const String uid, const String chat_text){
    Dictionary chat;
    chat["role"] = "user";
    chat["content"] = chat_text;
    Dictionary temp_data = chat_datas[uid];
    if( temp_data.has("chats")){
        Array temp_array =  temp_data["chats"];
        temp_array.push_back(chat);
        temp_data["chats"] = temp_array;
        chat_datas[uid] = temp_data;
    }
}

void AIChatManager::add_assistant_chat(const String uid, const String chat_text){
    Dictionary chat;
    chat["role"] = "assistant";
    chat["content"] = chat_text;
    Dictionary temp_data = chat_datas[uid];
    if( temp_data.has("chats")){
        Array temp_array =  temp_data["chats"];
        temp_array.push_back(chat);
        temp_data["chats"] = temp_array;
        chat_datas[uid] = temp_data;
    }
}

Array AIChatManager::get_chat(const String& uid){
    Dictionary d_data = chat_datas[uid];
    Array array_data = d_data["chats"];
    Array temp_data;
    for(int i=0;i<array_data.size();i++){
        Dictionary temp = array_data[i];
        if(temp["role"]=="user"||temp["role"]=="assistant"){
            temp_data.push_back(temp);
        }
    }
    return temp_data;
}

Array AIChatManager::get_chat_before_index(const String& uid, int index){
    Dictionary d_data = chat_datas[uid];
    Array array_data = d_data["chats"];
    Array temp_array;
    for(int i=0;i<index;i++){
        if(index < array_data.size()){
            temp_array.push_back(array_data[i]);
        }
        else{
            ERR_PRINT("Index out of range. Index:"+String::num(index)+" Size:"+String::num(array_data.size()));
        }
    }
    d_data["chats"] = temp_array;
    chat_datas[uid] = d_data;
    return get_chat(uid);
}


String AIChatManager::get_title(const String& uid){
    Dictionary d_data = chat_datas[uid];
    String title = d_data["title"];
    return title;
}

Dictionary AIChatManager::get_chat_datas(){
    return chat_datas;
}

void AIChatManager::add_tool_chat(const String uid, const Dictionary dict){
    Dictionary d_data = chat_datas[uid];
    Array array_data = d_data["chats"];
    array_data.push_back(dict);
    d_data["chats"] = array_data;
    chat_datas[uid] = d_data;
}

void AIChatManager::add_sys_chat(const String uid, const String chat_text, String sys_type){
    Dictionary chat;
    chat["role"] = sys_type;
    chat["content"] = chat_text;
    Dictionary temp_data = chat_datas[uid];
    if( temp_data.has("chats")){
        Array temp_array =  temp_data["chats"];
        temp_array.push_back(chat);
        temp_data["chats"] = temp_array;
        chat_datas[uid] = temp_data;
    }
}

