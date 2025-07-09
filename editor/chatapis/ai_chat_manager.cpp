#include "ai_chat_manager.h"

AIChatManager::AIChatManager(/* args */)
{
}

void AIChatManager::create_new_chat(const String uid){
    Array new_chat;
    chat_datas[uid] = new_chat;
}

void AIChatManager::add_user_chat(const String uid, const String chat_text){
    Dictionary chat;
    chat["role"] = "user";
    chat["content"] = chat_text;
    if(chat_datas[uid].is_array()){
        Array temp = chat_datas[uid];
        temp.push_back(chat);
        chat_datas[uid] = temp;
    }
    
}
void AIChatManager::add_assistant_chat(const String uid, const String chat_text){
    Dictionary chat;
    chat["role"] = "assistant";
    chat["content"] = chat_text;
    if(chat_datas[uid].is_array()){
        Array temp = chat_datas[uid];
        temp.push_back(chat);
        chat_datas[uid] = temp;
    }
}

Array AIChatManager::get_chat(const String& uid){
    return chat_datas[uid];
}