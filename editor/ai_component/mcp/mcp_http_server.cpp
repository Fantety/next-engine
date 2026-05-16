/*
 * @FilePath: \editor\ai_component\mcp\mcp_http_server.cpp
 * @Author: Fantety
 * @Descripttion: MCP HTTP Server implementation for Godot Engine
 * @Date: 2025-08-06
 * @LastEditors: Fantety
 * @LastEditTime: 2025-08-06
 */
#include "mcp_http_server.h"
#include "core/os/os.h"
#include "core/error/error_macros.h"
#include "core/io/json.h"
#include "scene/main/mcp_router.h"

void MCPHttpServer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_tool", "name", "tool"), &MCPHttpServer::register_tool);
    ClassDB::bind_method(D_METHOD("unregister_tool", "name"), &MCPHttpServer::unregister_tool);
    ClassDB::bind_method(D_METHOD("get_tool", "name"), &MCPHttpServer::get_tool);
    ClassDB::bind_method(D_METHOD("get_registered_tools_list"), &MCPHttpServer::get_registered_tools_list);
    
    ADD_SIGNAL(MethodInfo("server_started"));
    ADD_SIGNAL(MethodInfo("server_stopped"));
    ADD_SIGNAL(MethodInfo("client_connected", PropertyInfo(Variant::OBJECT, "client")));
    ADD_SIGNAL(MethodInfo("client_disconnected", PropertyInfo(Variant::OBJECT, "client")));
    ADD_SIGNAL(MethodInfo("message_received", PropertyInfo(Variant::OBJECT, "client"), PropertyInfo(Variant::DICTIONARY, "message")));
}

MCPHttpServer::MCPHttpServer() {
    protocol.instantiate();
    singleton = this;
    // 注册MCP路由器
    Ref<McpRouter> router;
    router.instantiate();
    register_router("/mcp", router);
}

MCPHttpServer::~MCPHttpServer() {
    stop();
    singleton = nullptr;
}

void MCPHttpServer::register_tool(const String& name, MCPTool* tool) {
    if (tool) {
        registered_tools[name] = tool;
    }
}

void MCPHttpServer::unregister_tool(const String& name) {
    registered_tools.erase(name);
}

MCPTool* MCPHttpServer::get_tool(const String& name) const {
    if (registered_tools.has(name)) {
        return registered_tools.get(name);
    }
    return nullptr;
}

Array MCPHttpServer::get_registered_tools_list() const {
    Array tool_list;
    for (const KeyValue<String, MCPTool*> &E : registered_tools) {
        tool_list.append(E.key);
    }
    return tool_list;
}

MCPHttpServer* MCPHttpServer::get_singleton() {
    return singleton;
}

void MCPHttpServer::register_tool_static(const String& name, MCPTool* tool) {
    // 通过单例实例注册工具
    if (singleton != nullptr) {
        singleton->register_tool(name, tool);
    }
}

void MCPHttpServer::_handle_mcp_message(Ref<StreamPeerTCP> client, const Dictionary& message) {
    // 检查消息有效性
    if (!protocol->is_valid_message(message)) {
        Dictionary error_msg = protocol->create_error_message("Invalid message format");
        send_message(client, error_msg);
        return;
    }
    
    // 获取消息方法
    String method = protocol->get_message_method(message);
    String request_id = protocol->get_message_id(message);
    
    // 处理不同方法的消息
    if (method == "ping") {
        // 回复pong消息
        Dictionary pong_msg = protocol->create_pong_message();
        // 确保响应包含正确的ID
        pong_msg["id"] = request_id;
        send_message(client, pong_msg);
    } else if (method == "initialize") {
        // 处理初始化消息
        Dictionary init_response = protocol->create_initialize_message();
        // 确保响应包含正确的ID
        init_response["id"] = request_id;
        send_message(client, init_response);
    } else if (method == "notifications/initialized") {
        // 客户端初始化完成通知，无需响应
        // 可以在这里添加一些处理逻辑，如果需要的话
    } else if (method == "tools/list") {
        // 获取工具列表
        Array tools;
        for (const KeyValue<String, MCPTool*> &E : registered_tools) {
            Dictionary tool_desc = E.value->get_tool_description();
            tools.append(tool_desc);
        }
        
        Dictionary tools_list_msg = protocol->create_tool_list_message(tools);
        // 设置正确的ID
        tools_list_msg["id"] = request_id;
        send_message(client, tools_list_msg);
    } else if (method == "tools/call") {
        // 调用工具
        Dictionary params = protocol->get_message_params(message);
        if (params.has("name") && params.has("arguments")) {
            String tool_name = params["name"];
            Dictionary arguments = params["arguments"];
            
            if (registered_tools.has(tool_name)) {
                MCPTool* tool = registered_tools[tool_name];
                Dictionary tool_result = tool->execute(arguments);
                
                Dictionary tool_result_msg = protocol->create_tool_result_message(tool_name, tool_result, request_id);
                send_message(client, tool_result_msg);
            } else {
                Dictionary error_msg = protocol->create_error_message("Tool not found: " + tool_name);
                // 确保响应包含正确的ID
                error_msg["id"] = request_id;
                send_message(client, error_msg);
            }
        } else {
            Dictionary error_msg = protocol->create_error_message("Invalid tool call parameters");
            // 确保响应包含正确的ID
            error_msg["id"] = request_id;
            send_message(client, error_msg);
        }
    } else {
        Dictionary error_msg = protocol->create_error_message("Unknown method: " + method);
        // 确保响应包含正确的ID
        error_msg["id"] = request_id;
        send_message(client, error_msg);
    }
}

Error MCPHttpServer::send_message(Ref<StreamPeerTCP> client, const Dictionary& message) {
    // 将消息转换为JSON
    String json_str = JSON::stringify(message);
    
    // 发送JSON响应
    Ref<HttpResponse> response;
    response.instantiate();
    response->client = client.ptr();
    response->json(200, message);
    
    return OK;
}

void MCPHttpServer::send_stream_message(Ref<StreamPeerTCP> client, const Dictionary& message) {
    // 将消息转换为JSON
    String json_str = JSON::stringify(message);
    
    // 发送流消息
    StreamableHttpServer::send_stream_message(client, "message", json_str);
}