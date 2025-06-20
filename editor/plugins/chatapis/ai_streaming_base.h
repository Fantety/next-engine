/*
 * @FilePath: \editor\plugins\chatapis\ai_streaming_base.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 20:48:06
 * @LastEditors: Fantety
 * @LastEditTime: 2025-06-20 14:52:53
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
    virtual ~AIStreamingBase() = default;
    // 设置API密钥
    void setApiKey(const std::string& key) { apiKey = key; }
    // 设置模型
    void setModel(const std::string& modelName) { model = modelName; }
    // 设置超时时间(秒)
    void setTimeout(int seconds) { timeout = seconds; }
    // 发送流式请求(纯虚函数，子类必须实现)
    virtual bool sendStreamingRequest(const std::string& prompt) = 0;
    virtual void handleStreamResponse(const char* data, size_t len) = 0;
    
protected:
    std::string model;
    std::string apiKey;
    int timeout = 60;
};

#endif // AI_STREAMING_BASE_H
