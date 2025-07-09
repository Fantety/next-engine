/*
 * @FilePath: \editor\chatapis\ai_streaming_base.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 20:48:06
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-04 18:34:16
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
        DEEPSEEK_REASON,
        OPENAI,
    };
    inline static Dictionary AIStringName;

public:
    AIStreamingBase(const std::string& modelName = "deepseek-chat") 
        : model(modelName){
            AIStringName["deepseek-chat"] = DEEPSEEK_CHAT;
            AIStringName["deepseek-reason"] = DEEPSEEK_REASON;
        }
    void set_apikey(const std::string& key) { apiKey = key; }
    void set_model(const std::string& modelName) { model = modelName; }
    void set_timeout(int seconds) { timeout = seconds; }
    virtual bool send_streaming_request(const Array& prompt) = 0;
    virtual PackedByteArray construct_body(const Array& prompt) = 0;
    virtual String get_respone_content(const String& jdata) = 0;
    
protected:
    std::string model;
    std::string apiKey;
    int timeout = 60;
    bool is_valid_utf8(const uint8_t* tdata, int len) {
        int i = 0;
        while (i < len) {
            if (tdata[i] <= 0x7F) { i++; continue; } // ASCII
            // 多字节UTF-8序列检查
            if ((tdata[i] & 0xE0) == 0xC0) { if (i+1 >= len || (tdata[i+1] & 0xC0) != 0x80) return false; i+=2; }
            else if ((tdata[i] & 0xF0) == 0xE0) { if (i+2 >= len || (tdata[i+1] & 0xC0) != 0x80 || (tdata[i+2] & 0xC0) != 0x80) return false; i+=3; }
            else if ((tdata[i] & 0xF8) == 0xF0) { if (i+3 >= len || (tdata[i+1] & 0xC0) != 0x80 || (tdata[i+2] & 0xC0) != 0x80 || (tdata[i+3] & 0xC0) != 0x80) return false; i+=4; }
            else return false;
        }
        return true;
    }
};

#endif // AI_STREAMING_BASE_H
