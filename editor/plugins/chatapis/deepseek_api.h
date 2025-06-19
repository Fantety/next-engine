/*
 * @FilePath: \editor\plugins\chatapis\deepseek_api.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 19:18:53
 * @LastEditors: Fantety
 * @LastEditTime: 2025-06-19 13:37:30
 */
// deepseek_api.h
#ifndef DEEPSEEK_API_H
#define DEEPSEEK_API_H

#include "ai_streaming_base.h"
#include "core/variant/variant.h"
#include "core/variant/dictionary.h"
#include "core/os/mutex.h"
#include "scene/main/timer.h"
#include <queue>
#include <memory>

class DeepSeekAPI : public AIStreamingBase {
public:
    DeepSeekAPI(const std::string& modelName = "deepseek-chat") 
        : AIStreamingBase(modelName){}
    ~DeepSeekAPI();

    bool sendStreamingRequest(const std::string& prompt) override;
    
protected:
    static void _bind_methods();

    void handleStreamResponse(const char* data, size_t len) override;


};





#endif // DEEPSEEK_API_H