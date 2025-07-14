/*
 * @FilePath: \editor\ai_component\apis\ai_streaming_base.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 20:48:06
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-14 12:36:14
 */
#ifndef AI_STREAMING_BASE_H
#define AI_STREAMING_BASE_H

#include <string>
#include <functional>
#include "core/variant/variant.h"
#include "core/variant/dictionary.h"
#include "scene/main/node.h"
#include "core/object/class_db.h"
#include "core/io/http_client.h"
#include "core/io/stream_peer_tcp.h"
#include "core/io/json.h"
#include "tools_json.h"
#include "scene/main/timer.h"

class AIStreamingBase : public Node {
    GDCLASS(AIStreamingBase, Node);
public:
    enum AIModel
    {
        DEEPSEEK_CHAT = 0,
        DEEPSEEK_REASONER,
        OPENAI,
    };
    enum CurrentChatFlag{
        NORMAL_CHAT = 0,
        TOOL_CHAT,
        ERR_CHAT,
        ERR_NETWORK,
        ERR_TIMEOUT,
    };
    inline static Dictionary AIStringName;

public:
    AIStreamingBase(const String modelName = "deepseek-chat") 
        : model(modelName){
        AIStringName["deepseek-chat"] = DEEPSEEK_CHAT;
        AIStringName["deepseek-reasoner"] = DEEPSEEK_REASONER;
        Ref<JSON> json;
        json.instantiate();
        Error err = json->parse(AITools::TOOLS_JSON_STR);
        if (err != OK) return;
        Variant json_data = json->get_data();
        if (json_data.get_type() == Variant::ARRAY) {
            this->tools_data = json_data;
            print_line("[AI Component]: Init tools data success");
        }
    }
    void set_apikey(const String key) { apiKey = key; }
    void set_model(const String modelName) { model = modelName; }
    void set_timeout(int seconds) { timeout = seconds; }
    virtual bool send_streaming_request(const Array& prompt) = 0;
    virtual PackedByteArray construct_body(const Array& prompt) = 0;
    virtual String get_respone_content(const String& jdata) = 0;
    
protected:
    String model;
    String apiKey;
    int timeout = 30;
    Array tools_data;

    bool is_valid_utf8(const uint8_t* tdata, int len);

};

#endif // AI_STREAMING_BASE_H
