/*
 * @FilePath: \editor\ai_component\apis\openai_request_handler.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 19:18:53
 * @LastEditors: Fantety
 * @LastEditTime: 2025-08-02 11:42:43
 */
// openai_request_handler.h
#ifndef OPENAI_REQUEST_HANDLER_H
#define OPENAI_REQUEST_HANDLER_H

#include "ai_streaming_base.h"
#include "core/object/callable_mp.h"
#include "core/variant/variant.h"
#include "core/variant/dictionary.h"
#include "core/os/mutex.h"
#include "scene/main/timer.h"
#include "core/os/thread.h"
#include <queue>
#include <memory>
class OpenAIRequestHandler;
struct ThreadParams {
    OpenAIRequestHandler *self;
    Array prompt;
};
class OpenAIRequestHandler : public AIStreamingBase {
    GDCLASS(OpenAIRequestHandler, Node);
    Thread thread;
    SafeFlag exit_flag;
    bool thread_running = false;

    Dictionary current_delta;
    Timer *timeout_timer; // 添加定时器成员变量
    Ref<JSON> json;
    
private:
    static void _thread_func(void *p_userdata);
private:
    void _on_request_start();
    void _on_request_complete();
    void _on_timeout();
public:

    OpenAIRequestHandler(const String modelName = "deepseek-chat")
        : AIStreamingBase(modelName){
            json.instantiate();
            timeout_timer = memnew(Timer);
            timeout_timer->set_one_shot(true);
            add_child(timeout_timer);
            this->connect("openai_stop_timer", callable_mp(this, &OpenAIRequestHandler::_on_request_complete));
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

#endif // OPENAI_REQUEST_HANDLER_H
