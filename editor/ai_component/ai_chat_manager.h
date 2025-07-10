/*
 * @FilePath: \editor\ai_component\ai_chat_manager.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-07-07 12:16:06
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-10 13:57:29
 */
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "core/variant/array.h"
#include "core/io/file_access.h"


class AIChatManager
{
private:
    Dictionary chat_datas;
    /* data */
public:
    AIChatManager(/* args */);
    void create_new_chat(const String uid, const String title);

    void add_user_chat(const String uid, const String chat_text);
    void add_assistant_chat(const String uid, const String chat_text);
    void save_chats();
    Array get_chat(const String& uid);
    String get_title(const String& uid);
};

