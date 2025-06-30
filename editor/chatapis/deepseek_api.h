/*
 * @FilePath: \editor\chatapis\deepseek_api.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 19:18:53
 * @LastEditors: Fantety
 * @LastEditTime: 2025-06-24 15:30:46
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
    String prompt;
    Array prompt_array;
};
class DeepSeekAPI : public AIStreamingBase {
    GDCLASS(DeepSeekAPI, Node);
    Thread thread;
    SafeFlag exit_flag;
    bool thread_running = false;

private:
    static void _thread_func(void *p_userdata);
public:
    DeepSeekAPI(const std::string& modelName = "deepseek-chat")
        : AIStreamingBase(modelName){}

    bool send_streaming_request(const String& prompt) override;
    bool send_streaming_request(const Array& prompt) override;
    void cancel_request(); 
    
protected:
    static void _bind_methods();
    //SIGNAL("data_received", "data");
    void _notification(int p_what);
    String parse_json_data(const String& jdata);

};





#endif // DEEPSEEK_API_H