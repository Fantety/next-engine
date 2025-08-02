/*
 * @FilePath: \editor\ai_component\mcp\mcp_protocol.cpp
 * @Author: Fantety
 * @Descripttion: MCP Protocol implementation for Godot Engine
 * @Date: 2025-07-14
 * @LastEditors: Fantety
 * @LastEditTime: 2025-08-02 21:11:06
 */
#include "mcp_protocol.h"
#include "core/io/json.h"
#include "core/os/os.h"

void MCPProtocol::_bind_methods() {
    ClassDB::bind_method(D_METHOD("create_init_message"), &MCPProtocol::create_init_message);
    ClassDB::bind_method(D_METHOD("create_initialize_message"), &MCPProtocol::create_initialize_message);
    ClassDB::bind_method(D_METHOD("create_ping_message"), &MCPProtocol::create_ping_message);
    ClassDB::bind_method(D_METHOD("create_pong_message"), &MCPProtocol::create_pong_message);
    ClassDB::bind_method(D_METHOD("create_complete_message", "request_id"), &MCPProtocol::create_complete_message);
    ClassDB::bind_method(D_METHOD("create_error_message", "error"), &MCPProtocol::create_error_message);
    ClassDB::bind_method(D_METHOD("create_notification_message", "method", "params"), &MCPProtocol::create_notification_message);
    ClassDB::bind_method(D_METHOD("create_tool_list_message", "tools"), &MCPProtocol::create_tool_list_message);
    ClassDB::bind_method(D_METHOD("create_tool_call_message", "name", "arguments", "request_id"), &MCPProtocol::create_tool_call_message);
    ClassDB::bind_method(D_METHOD("create_tool_result_message", "name", "result", "request_id"), &MCPProtocol::create_tool_result_message);
    ClassDB::bind_method(D_METHOD("is_valid_message", "message"), &MCPProtocol::is_valid_message);
    ClassDB::bind_method(D_METHOD("get_message_method", "message"), &MCPProtocol::get_message_method);
    ClassDB::bind_method(D_METHOD("get_message_params", "message"), &MCPProtocol::get_message_params);
    ClassDB::bind_method(D_METHOD("get_message_id", "message"), &MCPProtocol::get_message_id);
    ClassDB::bind_method(D_METHOD("set_init_message", "init_message"), &MCPProtocol::set_init_message);
    ClassDB::bind_method(D_METHOD("get_init_message"), &MCPProtocol::get_init_message);
}

MCPProtocol::MCPProtocol() {
    // 初始化默认的init消息
    _build_init_message();
}

Dictionary MCPProtocol::create_init_message() {
    return init_message;
}

Dictionary MCPProtocol::create_initialize_message() {
    return init_message;
}

void MCPProtocol::_build_init_message() {
    // 构建初始化消息
    init_message["jsonrpc"] = "2.0";
    Dictionary result;
    result["protocolVersion"] = "2025-06-18";
    Dictionary capabilities;
    capabilities["tools"] = Dictionary();
    capabilities["resources"] = Dictionary();
    result["capabilities"] = capabilities;
    Dictionary serverInfo;
    serverInfo["name"] = "godot-mcp-server";
    serverInfo["version"] = "1.0.0";
    result["serverInfo"] = serverInfo;
    init_message["result"] = result;
}

Dictionary MCPProtocol::create_ping_message() {
    Dictionary message;
    message["jsonrpc"] = "2.0";
    message["method"] = "ping";
    message["id"] = String::num_int64(OS::get_singleton()->get_unix_time());
    return message;
}

Dictionary MCPProtocol::create_pong_message() {
    Dictionary message;
    message["jsonrpc"] = "2.0";
    message["method"] = "pong";
    message["id"] = String::num_int64(OS::get_singleton()->get_unix_time());
    return message;
}

Dictionary MCPProtocol::create_complete_message(const String& request_id) {
    Dictionary message;
    message["jsonrpc"] = "2.0";
    message["method"] = "complete";
    message["id"] = request_id;
    return message;
}

Dictionary MCPProtocol::create_error_message(const String& error) {
    Dictionary message;
    message["jsonrpc"] = "2.0";
    message["error"] = error;
    message["id"] = String::num_int64(OS::get_singleton()->get_unix_time());
    return message;
}

Dictionary MCPProtocol::create_notification_message(const String& method, const Dictionary& params) {
    Dictionary message;
    message["jsonrpc"] = "2.0";
    message["method"] = method;
    message["params"] = params;
    return message;
}

Dictionary MCPProtocol::create_tool_list_message(const Array& tools) {
    Dictionary message;
    message["jsonrpc"] = "2.0";
    Dictionary result;
    result["tools"] = tools;
    message["result"] = result;
    // ID将在服务器端设置为请求的ID
    return message;
}

Dictionary MCPProtocol::create_tool_call_message(const String& name, const Dictionary& arguments, const String& request_id) {
    Dictionary message;
    message["jsonrpc"] = "2.0";
    message["method"] = "tools/call";
    Dictionary params;
    params["name"] = name;
    params["arguments"] = arguments;
    message["params"] = params;
    message["id"] = request_id;
    return message;
}

Dictionary MCPProtocol::create_tool_result_message(const String& name, const Dictionary& result, const String& request_id) {
    Dictionary message;
    message["jsonrpc"] = "2.0";
    message["id"] = request_id;
    message["result"] = result;
    return message;
}

bool MCPProtocol::is_valid_message(const Dictionary& message) {
    // 检查必需的字段
    if (!message.has("jsonrpc")) {
        return false;
    }
    
    String jsonrpc = message["jsonrpc"];
    if (jsonrpc != "2.0") {
        return false;
    }
    
    // 消息必须有method或error字段
    if (!message.has("method") && !message.has("error")) {
        return false;
    }
    
    return true;
}

String MCPProtocol::get_message_method(const Dictionary& message) {
    if (message.has("method")) {
        return message["method"];
    }
    return "";
}

Dictionary MCPProtocol::get_message_params(const Dictionary& message) {
    if (message.has("params")) {
        Variant params = message["params"];
        if (params.get_type() == Variant::DICTIONARY) {
            return params;
        }
    }
    return Dictionary();
}

String MCPProtocol::get_message_id(const Dictionary& message) {
    if (message.has("id")) {
        Variant id = message["id"];
        if (id.get_type() == Variant::STRING) {
            return id;
        } else if (id.get_type() == Variant::INT) {
            return String::num_int64(id);
        }
    }
    return "";
}

bool MCPProtocol::is_request_message(const Dictionary& message) {
    // 请求消息必须有id字段且不是通知消息
    return message.has("id") && !is_notification_message(message);
}

bool MCPProtocol::is_notification_message(const Dictionary& message) {
    // 通知消息有method但没有id字段
    return message.has("method") && !message.has("id");
}

void MCPProtocol::set_init_message(const Dictionary& p_init_message) {
    init_message = p_init_message;
}

Dictionary MCPProtocol::get_init_message() const {
    return init_message;
}