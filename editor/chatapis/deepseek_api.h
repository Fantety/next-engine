/*
 * @FilePath: \editor\chatapis\deepseek_api.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 19:18:53
 * @LastEditors: Fantety
 * @LastEditTime: 2025-06-20 18:34:12
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
#include "utf8.h"
#include <queue>
#include <memory>
class DeepSeekAPI;
struct ThreadParams {
    DeepSeekAPI *self;
    String prompt;
};
class DeepSeekAPI : public AIStreamingBase {
    GDCLASS(DeepSeekAPI, Node);
    Thread thread;
    SafeFlag exit_flag;
    bool thread_running = false;

private:
    static void _thread_func(void *p_userdata);

    String get_string_from_utf8(PackedByteArray* chunk){
        String s;
		if (chunk->size() > 0) {
			const uint8_t *r = chunk->ptr();
			s.append_utf8((const char *)r, chunk->size());
		}
		return s;
    };
public:
    DeepSeekAPI(const std::string& modelName = "deepseek-chat") 
        : AIStreamingBase(modelName){}
    ~DeepSeekAPI();

    bool sendStreamingRequest(const String& prompt) override;
    
protected:
    static void _bind_methods();
    //SIGNAL("data_received", "data");
    void _notification(int p_what);
    void cancel_request(); 
    void handleStreamResponse(const char* data, size_t len) override;
    String parseJsonData(const String& data);

};





#endif // DEEPSEEK_API_H