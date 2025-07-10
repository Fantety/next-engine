/*
 * @FilePath: \editor\ai_component\ai_streaming_base.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 20:48:06
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-10 10:33:59
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

class AIStreamingBase : public Node {
    GDCLASS(AIStreamingBase, Node);
public:
    enum AIModel
    {
        DEEPSEEK_CHAT = 0,
        DEEPSEEK_REASONER,
        OPENAI,
    };
    inline static Dictionary AIStringName;

public:
    AIStreamingBase(const String modelName = "deepseek-chat") 
        : model(modelName){
            AIStringName["deepseek-chat"] = DEEPSEEK_CHAT;
            AIStringName["deepseek-reasoner"] = DEEPSEEK_REASONER;
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
    int timeout = 60;
    bool is_valid_utf8(const uint8_t* tdata, int len);
};

#endif // AI_STREAMING_BASE_H
