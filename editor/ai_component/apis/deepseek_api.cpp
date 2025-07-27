// deepseek_api.cpp
// 假设更新后的 includePath 能正确找到头文件，这里暂时保持不变
#include "deepseek_api.h"
#include "core/variant/variant.h"
#include "core/os/os.h"
#include "core/config/engine.h"
#include "core/os/memory.h" // 添加内存管理头文件
#include "core/os/mutex.h" // 添加互斥锁头文件
#include "core/os/thread.h"

void DeepSeekAPI::_notification(int p_what) {
}
bool DeepSeekAPI::send_streaming_request(const Array& prompt){
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


PackedByteArray DeepSeekAPI::construct_body(const Array& prompt){
    Dictionary body;
    body["model"] = model;
    body["stream"] = true;
    body["tools"] = tools_data;
    Array messages;
    Dictionary sys_msg;
    sys_msg["role"] = "system";
    sys_msg["content"] = String::utf8("你是嵌入在Godot当中的AI助手，可以帮助用户开发游戏");
    messages = prompt;
    messages.push_front(sys_msg);
    body["messages"] = messages;
    String request_body = JSON::stringify(body);
    PackedByteArray body_data = request_body.to_utf8_buffer();
    return body_data;
}

String DeepSeekAPI::get_respone_content(const String& jdata){
    print_line(jdata);
    call_deferred("emit_signal", SNAME("deepseek_stop_timer"));
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
        current_delta = merge_delta(delta);
        print_line(current_delta);
        String finish_reason = choice["finish_reason"];
        print_line(finish_reason);
        call_deferred("emit_signal", SNAME("deepseek_streaming_received"), current_delta, finish_reason);
    }
    return String{};
}

void DeepSeekAPI::_thread_func(void *p_userdata) {
    ThreadParams *params = static_cast<ThreadParams*>(p_userdata);
    Ref<HTTPClient> t_http_client = HTTPClient::create();
    t_http_client->set_blocking_mode(true);
    Error err = t_http_client->connect_to_host("https://api.deepseek.com", 443, Ref<TLSOptions>());
    if (err != OK) {
        params->self->current_chat_flag = AIStreamingBase::ERR_NETWORK;
        params->self->call_deferred("emit_signal", SNAME("deepseek_request_completed"), params->self->current_chat_flag);
        ERR_PRINT("Failed to connect to host: " + String::num_int64(err));
        params->self->call_deferred("emit_signal", SNAME("deepseek_stop_timer"));
        return;
    }
    while (t_http_client->get_status() == HTTPClient::STATUS_CONNECTING || 
            t_http_client->get_status() == HTTPClient::STATUS_RESOLVING) {
        t_http_client->poll();
        OS::get_singleton()->delay_usec(100);
    }
    if (t_http_client->get_status() != HTTPClient::STATUS_CONNECTED) {
        params->self->current_chat_flag = AIStreamingBase::ERR_NETWORK;
        params->self->call_deferred("emit_signal", SNAME("deepseek_request_completed"), params->self->current_chat_flag);
        ERR_PRINT("Connection failed with status: " + String::num_int64(t_http_client->get_status()));
        params->self->call_deferred("emit_signal", SNAME("deepseek_stop_timer"));
        return;
    }
    Vector<String> headers;
    headers.push_back("Authorization: Bearer " + String{params->self->apiKey});
    headers.push_back("Content-Type: application/json");
    PackedByteArray body_data = params->self->construct_body(params->prompt);
    err = t_http_client->request(HTTPClient::METHOD_POST, "/chat/completions", headers, body_data.ptr(), body_data.size());
    if (err != OK) {
        ERR_PRINT("Request failed: " + String::num_int64(err));
        params->self->current_chat_flag = AIStreamingBase::ERR_NETWORK;
        params->self->call_deferred("emit_signal", SNAME("deepseek_request_completed"), params->self->current_chat_flag);
        params->self->call_deferred("emit_signal", SNAME("deepseek_stop_timer"));
        return;
    }
    params->self->call_deferred("emit_signal", SNAME("deepseek_data_start"));
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
                    params->self->call_deferred("emit_signal", SNAME("deepseek_request_completed"), params->self->current_chat_flag);
                    params->self->call_deferred("emit_signal", SNAME("deepseek_stop_timer"));
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
                    params->self->call_deferred("emit_signal", SNAME("deepseek_stop_timer"));
                    break;
                }
            }
        }
        else if (status == HTTPClient::STATUS_DISCONNECTED) {
            params->self->current_chat_flag = AIStreamingBase::ERR_CHAT;
            params->self->call_deferred("emit_signal", SNAME("deepseek_request_completed"), params->self->current_chat_flag);
            params->self->call_deferred("emit_signal", SNAME("deepseek_stop_timer"));
            break;
        }
        else if (status >= HTTPClient::STATUS_CONNECTION_ERROR) {
            params->self->current_chat_flag = AIStreamingBase::ERR_CHAT;
            ERR_PRINT("Error status: " + String::num_int64(status));
            params->self->call_deferred("emit_signal", SNAME("deepseek_request_completed"), params->self->current_chat_flag);
            params->self->call_deferred("emit_signal", SNAME("deepseek_stop_timer"));
            break;
        }
        OS::get_singleton()->delay_usec(1000);
    }
    params->self->thread_running = false;
}

void DeepSeekAPI::cancel_request() {
    current_delta.clear();
    if (thread.is_started()) {
        thread.wait_to_finish();
    }
    thread_running = false;
    _on_request_complete(); // 停止定时器
}

void DeepSeekAPI::reset_delta(){
    Array temp_array;
    current_delta["reasoning_content"] = "";
    current_delta["content"] = "";
    current_delta["role"] = "";
    current_delta["logprobs"] = Variant::NIL;
    current_delta["finish_reason"] = Variant::NIL;
    current_delta["tool_calls"] = Array{};


}

Dictionary DeepSeekAPI::merge_delta(Dictionary new_delta) {
    // 1. 处理 content 字段
    if (new_delta.has("content")) {
        String new_content = new_delta["content"];
        if (current_delta.has("content")) {
            String old_content = current_delta["content"];
            current_delta["content"] = old_content + new_content;
        } else {
            current_delta["content"] = new_content;
        }
    }
    // 2. 处理 tool_calls 数组
    if (new_delta.has("tool_calls")) {
        Array new_tool_calls = new_delta["tool_calls"];
        Array merged_tool_calls;
        // 获取当前累积的 tool_calls 或创建新数组
        if (current_delta.has("tool_calls")) {
            merged_tool_calls = current_delta["tool_calls"];
        } else {
            merged_tool_calls = Array();
        }
        // 遍历新的 tool_calls 进行合并
        for (int i = 0; i < new_tool_calls.size(); i++) {
            Dictionary new_call = new_tool_calls[i];
            // 确保包含 index 字段
            if (!new_call.has("index")) {
                ERR_PRINT("Tool call missing 'index' field, skipping");
                continue;
            }
            int index = new_call["index"];
            // 检查索引是否有效
            if (index < 0) {
                ERR_PRINT("Invalid tool call index: " + itos(index));
                continue;
            }
            // 确保数组足够大
            if (index >= merged_tool_calls.size()) {
                merged_tool_calls.resize(index + 1);
            }
            // 获取或创建当前索引处的工具调用
            Dictionary merged_call;
            if (merged_tool_calls.size() > index && merged_tool_calls[index].get_type() == Variant::DICTIONARY) {
                merged_call = merged_tool_calls[index];
            }
            // 合并 function 字段
            if (new_call.has("function")) {
                Dictionary new_func = new_call["function"];
                Dictionary merged_func;
                if(merged_call.has("function")) merged_func = merged_call["function"];
                else merged_func = Dictionary();
                // 合并 arguments (分块拼接)
                if (new_func.has("arguments")) {
                    String new_args = new_func["arguments"];
                    if (merged_func.has("arguments")) {
                        String merged_args = merged_func["arguments"];
                        merged_func["arguments"] = merged_args + new_args;
                    } else {
                        merged_func["arguments"] = new_args;
                    }
                }
                // 合并 name
                if (new_func.has("name")) {
                    merged_func["name"] = new_func["name"];
                }
                merged_call["function"] = merged_func;
            }
            // 合并其他字段 (id, type 等)
            Array keys = new_call.keys();
            for (int j = 0; j < keys.size(); j++) {
                String key = keys[j];
                // 跳过已处理的 function 和 index
                if (key == "function" || key == "index") continue;
                // 特殊处理：如果字段已存在且是字符串，则拼接
                if (merged_call.has(key) && merged_call[key].get_type() == Variant::STRING &&
                    new_call[key].get_type() == Variant::STRING) {
                    merged_call[key] = String(merged_call[key]) + String(new_call[key]);
                } else {
                    // 否则直接覆盖
                    merged_call[key] = new_call[key];
                }
            }
            // 更新工具调用
            merged_tool_calls[index] = merged_call;
        }
        current_delta["tool_calls"] = merged_tool_calls;
    }
    // 3. 合并其他字段 (role, finish_reason 等)
    Array new_keys = new_delta.keys();
    for (int i = 0; i < new_keys.size(); i++) {
        String key = new_keys[i];
        // 跳过已处理的字段
        if (key == "content" || key == "tool_calls") continue;
        // 特殊字段处理
        if (key == "finish_reason") {
            // finish_reason 总是覆盖
            current_delta[key] = new_delta[key];
        } else {
            // 其他字段：如果已存在且是字符串则拼接，否则覆盖
            if (current_delta.has(key) && current_delta[key].get_type() == Variant::STRING &&
                new_delta[key].get_type() == Variant::STRING) {
                current_delta[key] = String(current_delta[key]) + String(new_delta[key]);
            } else {
                current_delta[key] = new_delta[key];
            }
        }
    }
    return current_delta.duplicate();
}

void DeepSeekAPI::_on_request_start() {
    timeout_timer->connect("timeout", callable_mp(this, &DeepSeekAPI::_on_timeout));
    timeout_timer->set_wait_time(timeout);
    timeout_timer->start();
}
void DeepSeekAPI::_on_request_complete() {
    if(!timeout_timer->is_connected("timeout", callable_mp(this, &DeepSeekAPI::_on_timeout)))
        return;
    timeout_timer->disconnect("timeout", callable_mp(this, &DeepSeekAPI::_on_timeout));
}
void DeepSeekAPI::_on_timeout() {
    current_chat_flag = AIStreamingBase::ERR_TIMEOUT;
    call_deferred("emit_signal", SNAME("deepseek_request_completed"), current_chat_flag);
}

void DeepSeekAPI::_bind_methods() {
    ADD_SIGNAL(MethodInfo("deepseek_data_received", PropertyInfo(Variant::STRING, "text")));
    ADD_SIGNAL(MethodInfo("deepseek_streaming_received", PropertyInfo(Variant::DICTIONARY, "dict"), PropertyInfo(Variant::STRING, "finish_reason")));
    ADD_SIGNAL(MethodInfo("deepseek_reason_received", PropertyInfo(Variant::STRING, "text")));
    ADD_SIGNAL(MethodInfo("deepseek_tool_received", PropertyInfo(Variant::ARRAY, "tools")));
    ADD_SIGNAL(MethodInfo("deepseek_request_completed", PropertyInfo(Variant::INT, "chat_type")));
    ADD_SIGNAL(MethodInfo("deepseek_data_start"));
    ADD_SIGNAL(MethodInfo("deepseek_stop_timer"));
}



