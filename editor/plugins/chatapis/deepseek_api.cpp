// deepseek_api.cpp
#include "deepseek_api.h"
#include <iostream>
#include "core/io/json.h"
#include "core/variant/variant.h"
#include "core/os/os.h"
#include "core/config/engine.h"
#include "core/os/memory.h" // 添加内存管理头文件
#include "core/os/mutex.h" // 添加互斥锁头文件
#include "core/os/thread.h" // 添加线程头文件


DeepSeekAPI::~DeepSeekAPI() {}
// 在 Godot 模块或核心代码中使用
#include "core/io/http_client.h"
#include "core/os/os.h"


bool DeepSeekAPI::sendStreamingRequest(const std::string& prompt) {
    print_line("[Call]: DeepSeekAPI::sendStreamingRequest");
    // 1. 创建 HTTPClient 实例
    Ref<HTTPClient> http = HTTPClient::create();
    
    // 2. 连接到主机（使用显式 HTTPS 端口 443）
    Error err = http->connect_to_host("https://api.deepseek.com", 443, Ref<TLSOptions>());
    if (err != OK) {
        ERR_PRINT("Failed to connect to host: " + String::num_int64(err));
        return false;
    }
    
    // 3. 等待连接完成
    while (http->get_status() == HTTPClient::STATUS_CONNECTING || 
            http->get_status() == HTTPClient::STATUS_RESOLVING) {
        http->poll();
        OS::get_singleton()->delay_usec(1000);
    }
    
    if (http->get_status() != HTTPClient::STATUS_CONNECTED) {
        ERR_PRINT("Connection failed with status: " + String::num_int64(http->get_status()));
        return false;
    }
    
    // 4. 构造请求
    Vector<String> headers;
    headers.push_back("Authorization: Bearer " + String{apiKey.c_str()});
    headers.push_back("Content-Type: application/json");
    headers.push_back("Accept: text/event-stream");
    
    String request_body = R"({
        "model": "deepseek-chat",
        "messages": [{"role": "user", "content": "Hello!"}],
        "stream": true
    })";
    
    // 5. 发送请求（使用 PackedByteArray 转换）
    PackedByteArray body_data = request_body.to_utf8_buffer();
    err = http->request(HTTPClient::METHOD_POST, "/v1/chat/completions", headers, body_data.ptr(), body_data.size());
    if (err != OK) {
        ERR_PRINT("Request failed: " + String::num_int64(err));
        return false;
    }
    
    // 6. 流式读取响应（修正类型转换）
    while (true) {
        http->poll();
        HTTPClient::Status status = http->get_status();
        
        if (status == HTTPClient::STATUS_BODY) {
            PackedByteArray chunk = http->read_response_body_chunk();
            if (chunk.size() > 0) {
                // 正确转换 uint8_t* 到 String
                String chunk_str = String::utf8(reinterpret_cast<const char *>(chunk.ptr()), chunk.size());
                print_line("Received chunk: " + chunk_str);
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
    
    http->close();
    return true;
}


// void DeepSeekAPI::processStreamResponse(const std::string& chunk, const ResponseCallback& callback) {
//     // 确保在主线程执行
//     if (Thread::get_caller_id() != Thread::get_main_id()) {
//         call_deferred("processStreamResponse", String(chunk.c_str()), callback);
//         return;
//     }
    
//     size_t pos = 0;
//     while (pos < chunk.size()) {
//         // 查找下一个data行
//         size_t lineStart = chunk.find("data: ", pos);
//         if (lineStart == std::string::npos) break;
//         lineStart += 6; // 跳过"data: "
        
//         // 查找行结束
//         size_t lineEnd = chunk.find("\n", lineStart);
//         if (lineEnd == std::string::npos) lineEnd = chunk.size();
        
//         // 获取一行数据
//         std::string line = chunk.substr(lineStart, lineEnd - lineStart);
        
//         // 处理结束标记
//         if (line == "[DONE]") {
//             if (callback) {
//                 callback("", true); // 发送结束标记
//             }
//             pos = lineEnd + 1;
//             continue;
//         }
        
//         // 使用Godot的JSON解析器
//         String jsonStr(line.c_str());
//         Ref<JSON> json;
//         json.instantiate();
//         Error err = json->parse(jsonStr);
//         if (err == OK) {
//             Variant jsonData = json->get_data();
//             if (jsonData.get_type() == Variant::DICTIONARY) {
//                 Dictionary data = jsonData;
//                 // 提取内容
//                 if (data.has("choices")) {
//                     Array choices = data["choices"];
//                     if (choices.size() > 0) {
//                         if (choices[0].get_type() == Variant::DICTIONARY) {
//                             Dictionary choice = choices[0];
//                             if (choice.has("delta")) {
//                                 Dictionary delta = choice["delta"];
//                                 if (delta.has("content")) {
//                                     String content = delta["content"];
//                                     if (callback) {
//                                         callback(content.utf8().get_data(), false); // 发送内容片段
//                                     }
//                                 }
//                             }
//                         }
//                     }
//                 }
//             }
//         } else {
//             std::cerr << "JSON parse error at line " << json->get_error_line() << ": " << json->get_error_message().utf8().get_data() << std::endl;
//         }
        
//         pos = lineEnd + 1;
//     }
// }

void DeepSeekAPI::handleStreamResponse(const char* data, size_t len) {
    std::string chunk(data, len);
    print_line("Received chunk: " + String{chunk.c_str()});
}

void DeepSeekAPI::_bind_methods() {
}