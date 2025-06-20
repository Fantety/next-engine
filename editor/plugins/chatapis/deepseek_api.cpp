// deepseek_api.cpp
// 假设更新后的 includePath 能正确找到头文件，这里暂时保持不变
#include "deepseek_api.h"
#include "core/io/json.h"
#include "core/variant/variant.h"
#include "core/os/os.h"
#include "core/config/engine.h"
#include "core/os/memory.h" // 添加内存管理头文件
#include "core/os/mutex.h" // 添加互斥锁头文件
#include "core/os/thread.h"

void DeepSeekAPI::_notification(int p_notification) {
}

DeepSeekAPI::~DeepSeekAPI() {
    cancel_request();
}

bool DeepSeekAPI::sendStreamingRequest(const std::string& prompt) {
    if (thread_running) {
        cancel_request();
    }
    print_line("[Call]: DeepSeekAPI::sendStreamingRequest");
    ThreadParams *params = memnew(ThreadParams);
    params->self = this;
    params->prompt = prompt;
    http_client = HTTPClient::create();
    thread_running = true;
    thread.start(_thread_func, params);
    return true;
}

String DeepSeekAPI::parseJsonData(const String& data){
    Ref<JSON> json;
    json.instantiate();
    Error err = json->parse(data);
    if (err != OK) {
        ERR_PRINT("Parse Json Failed");
        return String{};
    }
    Variant json_data = json->get_data();
    if (json_data.get_type() == Variant::DICTIONARY) {
        Dictionary dict_data = json_data;
        if (!dict_data.has("choices")) {
            return String{};
        }
        Array choices = dict_data["choices"];
        if (choices.size() <= 0) {
            return String{};
        }
        if (choices[0].get_type() != Variant::DICTIONARY) {
            return String{};
        }
        Dictionary choice = choices[0];
        if (!choice.has("delta")) {
            return String{};
        }
        Dictionary delta = choice["delta"];
        if (!delta.has("content")) {
            return String{};
        }
        String content = delta["content"];
        call_deferred("emit_signal", SNAME("stream_response"), content);
        return content;
    }
    return String{};
}

void DeepSeekAPI::_thread_func(void *p_userdata) {
    ThreadParams *params = static_cast<ThreadParams*>(p_userdata);
    print_line("[Call]: DeepSeekAPI::sendStreamingRequest");
    Ref<HTTPClient> t_http_client = HTTPClient::create();
    t_http_client->set_blocking_mode(true);
    Error err = t_http_client->connect_to_host("https://api.deepseek.com", 443, Ref<TLSOptions>());
    if (err != OK) {
        ERR_PRINT("Failed to connect to host: " + String::num_int64(err));
        return;
    }
    // 3. 等待连接完成
    while (t_http_client->get_status() == HTTPClient::STATUS_CONNECTING || 
            t_http_client->get_status() == HTTPClient::STATUS_RESOLVING) {
        t_http_client->poll();
        OS::get_singleton()->delay_usec(100); // 降低轮询频率，减轻CPU负担
        //status = t_http_client->get_status();
    }
    if (t_http_client->get_status() != HTTPClient::STATUS_CONNECTED) {
        ERR_PRINT("Connection failed with status: " + String::num_int64(t_http_client->get_status()));
        return;
    }

    // 4. 构造请求
    Vector<String> headers;
    headers.push_back("Authorization: Bearer " + String{params->self->apiKey.c_str()});
    headers.push_back("Content-Type: application/json");
    headers.push_back("Accept: text/event-stream");

    Dictionary body;
    body["model"] = "deepseek-chat";
    body["stream"] = true; // 假设需要流式响应
    
    Array messages;
    Dictionary sys_msg;
    sys_msg["role"] = "system";
    sys_msg["content"] = "你是一个Godot引擎专家，游戏开发大师";
    Dictionary msg;
    msg["role"] = "user";
    msg["content"] = String{params->prompt.c_str()};
    messages.push_back(sys_msg);
    messages.push_back(msg);
    body["messages"] = messages;
    String request_body = JSON::stringify(body);
    // 5. 发送请求（使用 PackedByteArray 转换）
    PackedByteArray body_data = request_body.to_utf8_buffer();
    err = t_http_client->request(HTTPClient::METHOD_POST, "/v1/chat/completions", headers, body_data.ptr(), body_data.size());
    if (err != OK) {
        ERR_PRINT("Request failed: " + String::num_int64(err));
        return;
    }

    while (true) {
        t_http_client->poll();
        HTTPClient::Status status = t_http_client->get_status();
        if (status == HTTPClient::STATUS_BODY) {
            PackedByteArray chunk = t_http_client->read_response_body_chunk();
            if (chunk.size() > 0) {
                // 正确转换 uint8_t* 到 String
                String chunk_str = String::utf8(reinterpret_cast<const char *>(chunk.ptr()), chunk.size());
                if(chunk_str.contains("[DONE]")){
                    break;
                }
                if (status == HTTPClient::STATUS_DISCONNECTED || status == HTTPClient::STATUS_CONNECTION_ERROR) {
                    break;
                }
                PackedStringArray lines = chunk_str.split("\n", false);
                for (int i = 0; i < lines.size(); i++) {
                    String line = lines[i].strip_edges();
                    if (line.is_empty() || !line.begins_with("data: ")) {
                        continue;
                    }
                    String result_data = params->self->parseJsonData(line.trim_prefix("data: "));
                    print_line("Received chunk: " + result_data);
                    params->self->call_deferred("emit_signal", SNAME("data_received"), result_data);
                    params->self->emit_signal("deepseek_data_updated");
                    OS::get_singleton()->delay_usec(10); // 减少CPU占用
                }
            }
        }
        else if (status == HTTPClient::STATUS_DISCONNECTED) {
            break;
        }
        else if (status >= HTTPClient::STATUS_CONNECTION_ERROR) {
            ERR_PRINT("Error status: " + String::num_int64(status));
            break;
        }
        OS::get_singleton()->delay_usec(1000);
    }
    params->self->thread_running = false;
}

void DeepSeekAPI::cancel_request() {
    exit_flag.set();
    if (thread.is_started()) {
        thread.wait_to_finish();
    }
    thread_running = false;
}


void DeepSeekAPI::handleStreamResponse(const char* data, size_t len) {
    std::string chunk(data, len);
    print_line("Received chunk: " + String{chunk.c_str()});
}

void DeepSeekAPI::_bind_methods() {
    ADD_SIGNAL(MethodInfo("deepseek_data_received", PropertyInfo(Variant::STRING, "text")));
    ADD_SIGNAL(MethodInfo("request_completed"));
    ADD_SIGNAL(MethodInfo("deepseek_data_updated"));
}