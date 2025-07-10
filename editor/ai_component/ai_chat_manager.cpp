#include "ai_chat_manager.h"
#include "core/io/json.h"
#include "core/os/os.h"

AIChatManager::AIChatManager(/* args */)
{
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
    return array_data;
}

String AIChatManager::get_title(const String& uid){
    Dictionary d_data = chat_datas[uid];
    String title = d_data["title"];
    return title;
}