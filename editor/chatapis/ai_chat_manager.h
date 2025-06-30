#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "core/variant/array.h"


class AIChatManager
{
private:
    Dictionary chat_datas;
    /* data */
public:
    AIChatManager(/* args */);
    void create_new_chat(const String& uid);

    void add_user_chat(const String& uid, const String& chat_text);
    void add_assistant_chat(const String& uid, const String& chat_text);

    Array get_chat(const String& uid);

    
};

