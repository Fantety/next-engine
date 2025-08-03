// openai_request_handler.cpp
#include "openai_request_handler.h"
#include "core/variant/variant.h"
#include "core/os/os.h"
#include "core/config/engine.h"
#include "core/os/memory.h" // 添加内存管理头文件
#include "core/os/mutex.h" // 添加互斥锁头文件
#include "core/os/thread.h"

void OpenAIRequestHandler::_notification(int p_what) {
}

bool OpenAIRequestHandler::send_streaming_request(const Array& prompt){
    if (thread_running) {
        cancel_request();
    }
    current_delta.clear();
    ThreadParams *params = memnew(ThreadParams);
    params->self = this;
    params->prompt = prompt.duplicate();
    thread_running = true;
    _on_request_start(); 
    thread.start(_thread_func, params);
    return true;
}


PackedByteArray OpenAIRequestHandler::construct_body(const Array& prompt){
    Dictionary body;
    body["model"] = model;
    body["stream"] = true;
    body["tools"] = tools_data;
    Array messages;
    Dictionary sys_msg;
    sys_msg["role"] = "system";
    sys_msg["content"] = AITools::SYSTEM_PROMPT_STR;
    messages = prompt;
    messages.push_front(sys_msg);
    body["messages"] = messages;
    String request_body = JSON::stringify(body);
    PackedByteArray body_data = request_body.to_utf8_buffer();
    return body_data;
}

String OpenAIRequestHandler::get_respone_content(const String& jdata){
    //print_line(jdata);
    call_deferred("emit_signal", SNAME("openai_stop_timer"));
    Error err = json->parse(jdata);
    if (err != OK) return String{"Something Wrong"};
    Variant json_data = json->get_data();
    if (json_data.get_type() == Variant::DICTIONARY) {
        Dictionary dict_data = json_data;
        if (!dict_data.has("choices")) return String{"Something Wrong"};
        Array choices = dict_data["choices"];
        if (choices.size() <= 0) return String{"Something Wrong"};
        if (choices[0].get_type() != Variant::DICTIONARY) return String{};
        Dictionary choice = choices[0];
        if (!choice.has("delta")) return String{"Something Wrong"};
        Dictionary delta = choice["delta"];
        String content = delta["content"];
        String finish_reason = choice["finish_reason"];
        call_deferred("emit_signal", SNAME("openai_streaming_received"), content, finish_reason);
    }
    return String{""};
}

void OpenAIRequestHandler::_thread_func(void *p_userdata) {
    ThreadParams *params = static_cast<ThreadParams*>(p_userdata);
    Ref<HTTPClient> t_http_client = HTTPClient::create();
    t_http_client->set_blocking_mode(true);
    Error err = t_http_client->connect_to_host("https://api.deepseek.com", 443, Ref<TLSOptions>());
    if (err != OK) {
        params->self->current_chat_flag = AIStreamingBase::ERR_NETWORK;
        params->self->call_deferred("emit_signal", SNAME("openai_request_completed"), params->self->current_chat_flag);
        ERR_PRINT("Failed to connect to host: " + String::num_int64(err));
        params->self->call_deferred("emit_signal", SNAME("openai_stop_timer"));
        return;
    }
    while (t_http_client->get_status() == HTTPClient::STATUS_CONNECTING || 
            t_http_client->get_status() == HTTPClient::STATUS_RESOLVING) {
        t_http_client->poll();
        OS::get_singleton()->delay_usec(100);
    }
    if (t_http_client->get_status() != HTTPClient::STATUS_CONNECTED) {
        params->self->current_chat_flag = AIStreamingBase::ERR_NETWORK;
        params->self->call_deferred("emit_signal", SNAME("openai_request_completed"), params->self->current_chat_flag);
        ERR_PRINT("Connection failed with status: " + String::num_int64(t_http_client->get_status()));
        params->self->call_deferred("emit_signal", SNAME("openai_stop_timer"));
        return;
    }
    Vector<String> headers;
    headers.push_back("Authorization: Bearer " + String{params->self->apiKey});
    headers.push_back("Content-Type: application/json");
    PackedByteArray body_data = params->self->construct_body(params->prompt);
    err = t_http_client->request(HTTPClient::METHOD_POST, "/v1/chat/completions", headers, body_data.ptr(), body_data.size());
    if (err != OK) {
        ERR_PRINT("Request failed: " + String::num_int64(err));
        params->self->current_chat_flag = AIStreamingBase::ERR_NETWORK;
        params->self->call_deferred("emit_signal", SNAME("openai_request_completed"), params->self->current_chat_flag);
        params->self->call_deferred("emit_signal", SNAME("openai_stop_timer"));
        return;
    }
    params->self->call_deferred("emit_signal", SNAME("openai_data_start"));
    PackedByteArray buffer; // 用于累积不完整的UTF-8字节
    while (true) {
        t_http_client->poll();
        HTTPClient::Status status = t_http_client->get_status();
        if (status == HTTPClient::STATUS_BODY) {
            PackedByteArray chunk = t_http_client->read_response_body_chunk();
            if (chunk.size() > 0) {
                buffer.append_array(chunk);
                int valid_length = buffer.size();
                while (valid_length > 0) {
                    if (params->self->is_valid_utf8(buffer.ptr(), valid_length)) {
                        break;
                    }
                    valid_length--;
                }
                if (valid_length == 0) {
                    continue;
                }
                String chunk_str = String::utf8(reinterpret_cast<const char*>(buffer.ptr()), valid_length);
                buffer = buffer.slice(valid_length);
                if (!params->self->is_valid_utf8(chunk.ptr(), chunk.size())) { // 检查UTF-8有效性
                    ERR_PRINT("Invalid UTF-8");
                }
                if (status == HTTPClient::STATUS_DISCONNECTED || status == HTTPClient::STATUS_CONNECTION_ERROR) {
                    params->self->current_chat_flag = AIStreamingBase::ERR_CHAT;
                    params->self->call_deferred("emit_signal", SNAME("openai_request_completed"), params->self->current_chat_flag);
                    params->self->call_deferred("emit_signal", SNAME("openai_stop_timer"));
                    break;
                }
                PackedStringArray lines = chunk_str.split("\n\n", false);
                for (int i = 0; i < lines.size(); i++) {
                    String line = lines[i].strip_edges();
                    if (line.is_empty() || !line.begins_with("data: ")) {
                        continue;
                    }
                    String result_data = params->self->get_respone_content(line.trim_prefix("data: "));
                }
                if(chunk_str.contains("[DONE]")){
                    params->self->call_deferred("emit_signal", SNAME("openai_stop_timer"));
                    break;
                }
            }
        }
        else if (status == HTTPClient::STATUS_DISCONNECTED) {
            params->self->current_chat_flag = AIStreamingBase::ERR_CHAT;
            params->self->call_deferred("emit_signal", SNAME("openai_request_completed"), params->self->current_chat_flag);
            params->self->call_deferred("emit_signal", SNAME("openai_stop_timer"));
            break;
        }
        else if (status >= HTTPClient::STATUS_CONNECTION_ERROR) {
            params->self->current_chat_flag = AIStreamingBase::ERR_CHAT;
            ERR_PRINT("Error status: " + String::num_int64(status));
            params->self->call_deferred("emit_signal", SNAME("openai_request_completed"), params->self->current_chat_flag);
            params->self->call_deferred("emit_signal", SNAME("openai_stop_timer"));
            break;
        }
        OS::get_singleton()->delay_usec(1000);
    }
    params->self->thread_running = false;
}

void OpenAIRequestHandler::cancel_request() {
    current_delta.clear();
    if (thread.is_started()) {
        thread.wait_to_finish();
    }
    thread_running = false;
    _on_request_complete(); // 停止定时器
}

void OpenAIRequestHandler::_on_request_start() {
    timeout_timer->start(timeout);
    call_deferred("emit_signal", SNAME("openai_request_start"));
}

void OpenAIRequestHandler::_on_request_complete() {
    timeout_timer->stop();
    call_deferred("emit_signal", SNAME("openai_request_end"));
}

void OpenAIRequestHandler::_on_timeout() {
    cancel_request();
    current_chat_flag = AIStreamingBase::ERR_TIMEOUT;
    call_deferred("emit_signal", SNAME("openai_request_completed"), current_chat_flag);
}

void OpenAIRequestHandler::_bind_methods() {
    ClassDB::bind_method(D_METHOD("send_streaming_request", "prompt"), &OpenAIRequestHandler::send_streaming_request);
    ClassDB::bind_method(D_METHOD("cancel_request"), &OpenAIRequestHandler::cancel_request);
    ADD_SIGNAL(MethodInfo("openai_data_start"));
    ADD_SIGNAL(MethodInfo("openai_request_start"));
    ADD_SIGNAL(MethodInfo("openai_request_end"));
    ADD_SIGNAL(MethodInfo("openai_request_completed", PropertyInfo(Variant::INT, "flag")));
    ADD_SIGNAL(MethodInfo("openai_streaming_received", PropertyInfo(Variant::STRING, "content"), PropertyInfo(Variant::STRING, "finish_reason")));
    ADD_SIGNAL(MethodInfo("openai_stop_timer"));
}