/*
 * @FilePath: \editor\ai_component\apis\deepseek_api.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 19:18:53
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-13 17:53:03
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
class DeepSeekAPI;
struct ThreadParams {
    DeepSeekAPI *self;
    Array prompt;
};
class DeepSeekAPI : public AIStreamingBase {
    GDCLASS(DeepSeekAPI, Node);
    Thread thread;
    SafeFlag exit_flag;
    bool thread_running = false;

    Dictionary current_delta;

    
private:
    static void _thread_func(void *p_userdata);
    Dictionary merge_delta(Dictionary new_delta);
    void reset_delta();
public:

    DeepSeekAPI(const String modelName = "deepseek-chat")
        : AIStreamingBase(modelName){}
    bool send_streaming_request(const Array& prompt) override;
    PackedByteArray construct_body(const Array& prompt) override;
    String get_respone_content(const String& jdata) override;
    void cancel_request(); 
    int current_chat_flag = AIStreamingBase::NORMAL_CHAT;
protected:
    static void _bind_methods();
    void _notification(int p_what);
    

};

#endif