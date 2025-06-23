/*
 * @FilePath: \editor\chatapis\ai_streaming_base.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 20:48:06
 * @LastEditors: Fantety
 * @LastEditTime: 2025-06-23 15:16:51
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
    AIStreamingBase(const std::string& modelName = "deepseek-chat") 
        : model(modelName){}
    // 设置API密钥
    void setApiKey(const std::string& key) { apiKey = key; }
    // 设置模型
    void setModel(const std::string& modelName) { model = modelName; }
    // 设置超时时间(秒)
    void setTimeout(int seconds) { timeout = seconds; }
    // 发送流式请求(纯虚函数，子类必须实现)
    virtual bool sendStreamingRequest(const String& prompt) = 0;
    virtual void handleStreamResponse(const char* data, size_t len) = 0;
    
protected:
    std::string model;
    std::string apiKey;
    int timeout = 60;
    bool is_valid_utf8(const uint8_t* data, int len) {
        int i = 0;
        while (i < len) {
            if (data[i] <= 0x7F) { i++; continue; } // ASCII
            // 多字节UTF-8序列检查
            if ((data[i] & 0xE0) == 0xC0) { if (i+1 >= len || (data[i+1] & 0xC0) != 0x80) return false; i+=2; }
            else if ((data[i] & 0xF0) == 0xE0) { if (i+2 >= len || (data[i+1] & 0xC0) != 0x80 || (data[i+2] & 0xC0) != 0x80) return false; i+=3; }
            else if ((data[i] & 0xF8) == 0xF0) { if (i+3 >= len || (data[i+1] & 0xC0) != 0x80 || (data[i+2] & 0xC0) != 0x80 || (data[i+3] & 0xC0) != 0x80) return false; i+=4; }
            else return false;
        }
        return true;
    }
};

#endif // AI_STREAMING_BASE_H
