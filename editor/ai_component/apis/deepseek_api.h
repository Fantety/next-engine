/*
 * @FilePath: \editor\ai_component\apis\deepseek_api.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 19:18:53
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-14 14:32:33
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
    Timer *timeout_timer; // 添加定时器成员变量
    Ref<JSON> json;
    
private:
    static void _thread_func(void *p_userdata);
    Dictionary merge_delta(Dictionary new_delta);
    void reset_delta();
private:
    void _on_request_start();
    void _on_request_complete();
    void _on_timeout();
public:

    DeepSeekAPI(const String modelName = "deepseek-chat")
        : AIStreamingBase(modelName){
            json.instantiate();
            timeout_timer = memnew(Timer);
            timeout_timer->connect("timeout", callable_mp(this, &DeepSeekAPI::_on_timeout));
            timeout_timer->set_one_shot(true);
            this->connect("deepseek_stop_timer", callable_mp(this, &DeepSeekAPI::_on_request_complete));
            add_child(timeout_timer);
        }
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