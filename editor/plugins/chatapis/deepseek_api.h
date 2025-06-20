/*
 * @FilePath: \editor\plugins\chatapis\deepseek_api.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 19:18:53
 * @LastEditors: Fantety
 * @LastEditTime: 2025-06-20 14:56:21
 */
// deepseek_api.h
#ifndef DEEPSEEK_API_H
#define DEEPSEEK_API_H

#include "ai_streaming_base.h"
#include "core/variant/variant.h"
#include "core/variant/dictionary.h"
#include "core/os/mutex.h"
#include "scene/main/timer.h"
#include "core/os/thread.h"
#include <queue>
#include <memory>
class AIDock;
class DeepSeekAPI;
struct ThreadParams {
    DeepSeekAPI *self;
    std::string prompt;
};
class DeepSeekAPI : public AIStreamingBase {
    GDCLASS(DeepSeekAPI, AIStreamingBase);
    Thread thread;
    SafeFlag exit_flag;
    Ref<HTTPClient> http_client;
    bool thread_running = false;

private:
    static void _thread_func(void *p_userdata);
public:
    DeepSeekAPI(const std::string& modelName = "deepseek-chat") 
        : AIStreamingBase(modelName){}
    ~DeepSeekAPI();

    bool sendStreamingRequest(const std::string& prompt) override;
    
protected:
    static void _bind_methods();
    //SIGNAL("data_received", "data");
    void _notification(int p_notification);
    void cancel_request(); 
    void handleStreamResponse(const char* data, size_t len) override;
    String parseJsonData(const String& data);


};





#endif // DEEPSEEK_API_H