/*
 * @FilePath: \editor\ai_component\mcp\mcp_server.cpp
 * @Author: Fantety
 * @Descripttion: MCP Server implementation for Godot Engine
 * @Date: 2025-07-14
 * @LastEditors: Fantety
 * @LastEditTime: 2025-08-02 11:28:25
 */
#include "mcp_server.h"
#include "mcp_tool.h"
#include "mcp_protocol.h"
#include "core/os/os.h"
#include "core/error/error_macros.h"
#include "core/io/json.h"

void MCPServer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("start_server", "port"), &MCPServer::start_server, DEFVAL(3000));
    ClassDB::bind_method(D_METHOD("stop_server"), &MCPServer::stop_server);
    ClassDB::bind_method(D_METHOD("is_server_running"), &MCPServer::is_server_running);
    ClassDB::bind_method(D_METHOD("get_status"), &MCPServer::get_status);
    ClassDB::bind_method(D_METHOD("register_tool", "name", "tool"), &MCPServer::register_tool);
    ClassDB::bind_method(D_METHOD("unregister_tool", "name"), &MCPServer::unregister_tool);
    ClassDB::bind_method(D_METHOD("get_tool", "name"), &MCPServer::get_tool);
    ClassDB::bind_method(D_METHOD("get_registered_tools_list"), &MCPServer::get_registered_tools_list);
    
    ADD_SIGNAL(MethodInfo("server_started"));
    ADD_SIGNAL(MethodInfo("server_stopped"));
    ADD_SIGNAL(MethodInfo("client_connected", PropertyInfo(Variant::OBJECT, "client")));
    ADD_SIGNAL(MethodInfo("client_disconnected", PropertyInfo(Variant::OBJECT, "client")));
    ADD_SIGNAL(MethodInfo("message_received", PropertyInfo(Variant::OBJECT, "client"), PropertyInfo(Variant::DICTIONARY, "message")));
}

MCPServer::MCPServer() {
    server.instantiate();
    server_active = false;
    server_port = 3000;
    status = MCP_SERVER_STOPPED;
    singleton = this;
}

MCPServer::~MCPServer() {
    stop_server();
    singleton = nullptr;
}

Error MCPServer::start_server(uint16_t port) {
    if (status == MCP_SERVER_RUNNING || status == MCP_SERVER_STARTING) {
        return ERR_ALREADY_IN_USE;
    }
    
    status = MCP_SERVER_STARTING;
    server_port = port;
    
    Error err = server->listen(port);
    if (err != OK) {
        status = MCP_SERVER_ERROR;
        return err;
    }
    
    server_active = true;
    status = MCP_SERVER_RUNNING;
    
    // 发送服务器启动信号
    emit_signal("server_started");
    
    return OK;
}

void MCPServer::stop_server() {
    if (status != MCP_SERVER_RUNNING && status != MCP_SERVER_STARTING) {
        return;
    }
    
    status = MCP_SERVER_STOPPING;
    
    // 断开所有客户端连接
    for (auto E = clients.begin(); E != clients.end(); ++E) {
        _disconnect_client(E->key);
    }
    clients.clear();
    
    // 停止服务器
    server->stop();
    server_active = false;
    status = MCP_SERVER_STOPPED;
    
    // 发送服务器停止信号
    emit_signal("server_stopped");
}

bool MCPServer::is_server_running() const {
    return status == MCP_SERVER_RUNNING;
}

MCPServer::ServerStatus MCPServer::get_status() const {
    return status;
}

void MCPServer::register_tool(const String& name, MCPTool* tool) {
    if (tool) {
        registered_tools[name] = tool;
    }
}

void MCPServer::unregister_tool(const String& name) {
    registered_tools.erase(name);
}

MCPTool* MCPServer::get_tool(const String& name) const {
    if (registered_tools.has(name)) {
        return registered_tools.get(name);
    }
    return nullptr;
}

Array MCPServer::get_registered_tools_list() const {
    Array tool_list;
    for (auto E = registered_tools.begin(); E != registered_tools.end(); ++E) {
        tool_list.append(E->key);
    }
    return tool_list;
}

MCPServer* MCPServer::get_singleton() {
    return singleton;
}

void MCPServer::register_tool_static(const String& name, MCPTool* tool) {
    // 通过单例实例注册工具
    if (singleton != nullptr) {
        singleton->register_tool(name, tool);
    }
}

void MCPServer::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_PROCESS:
            if (server_active) {
                _accept_connections();
                
                // 处理已连接客户端的数据
                for (auto E = clients.begin(); E != clients.end(); ) {
                    Ref<StreamPeerTCP> client = E->key;
                    if (client->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
                        _handle_client_data(client);
                        ++E;
                    } else {
                        // 客户端已断开连接
                        Ref<StreamPeerTCP> client_to_remove = E->key;
                        ++E;
                        _disconnect_client(client_to_remove);
                    }
                }
            }
            break;
    }
}

void MCPServer::_process(double delta) {
    // 这个方法会在_NOTIFICATION_PROCESS中被调用
}

void MCPServer::_accept_connections() {
    while (server->is_connection_available()) {
        Ref<StreamPeerTCP> client = server->take_connection();
        if (client.is_valid()) {
            clients[client] = ""; // 存储客户端信息
            
            // 发送客户端连接信号
            emit_signal("client_connected", client);
        }
    }
}

void MCPServer::_handle_client_data(Ref<StreamPeerTCP> client) {
    // 读取客户端数据
    while (client->get_available_bytes() > 0) {
        // 读取数据长度
        int len;
        Error err = client->get_data((uint8_t*)&len, sizeof(len));
        if (err != OK) {
            _disconnect_client(client);
            return;
        }
        
        // 读取JSON数据
        PackedByteArray data;
        data.resize(len);
        err = client->get_data(data.ptrw(), len);
        if (err != OK) {
            _disconnect_client(client);
            return;
        }
        
        // 解析JSON
        String json_str = String::utf8((const char*)data.ptr(), len);
        Ref<JSON> json_parser;
        json_parser.instantiate();
        err = json_parser->parse(json_str);
        if (err != OK) {
            // 发送错误消息
            Dictionary error_msg = protocol.create_error_message("Invalid JSON");
            send_message(client, error_msg);
            continue;
        }
        
        Variant json_data = json_parser->get_data();
        if (json_data.get_type() == Variant::DICTIONARY) {
            Dictionary message = json_data;
            
            // 发送消息接收信号
            emit_signal("message_received", client, message);
            
            // 处理消息
            _handle_message(client, message);
        }
    }
}

void MCPServer::_disconnect_client(Ref<StreamPeerTCP> client) {
    if (clients.has(client)) {
        clients.erase(client);
    }
    
    // 发送客户端断开连接信号
    emit_signal("client_disconnected", client);
}

void MCPServer::_handle_message(Ref<StreamPeerTCP> client, const Dictionary& message) {
    // 检查消息有效性
    if (!protocol.is_valid_message(message)) {
        Dictionary error_msg = protocol.create_error_message("Invalid message format");
        send_message(client, error_msg);
        return;
    }
    
    // 获取消息方法
    String method = protocol.get_message_method(message);
    
    // 处理不同方法的消息
    if (method == "ping") {
        // 回复pong消息
        Dictionary pong_msg = protocol.create_pong_message();
        send_message(client, pong_msg);
    } else if (method == "initialize") {
        // 处理初始化消息
        Dictionary init_response = protocol.create_initialize_message();
        // 确保响应包含正确的ID
        init_response["id"] = protocol.get_message_id(message);
        send_message(client, init_response);
    } else if (method == "notifications/initialized") {
        // 客户端初始化完成通知，无需响应
        // 可以在这里添加一些处理逻辑，如果需要的话
    } else if (method == "tools/list") {
        // 获取工具列表
        Array tools;
        for (auto E = registered_tools.begin(); E != registered_tools.end(); ++E) {
            Dictionary tool_desc = E->value->get_tool_description();
            tools.append(tool_desc);
        }
        
        Dictionary tools_list_msg = protocol.create_tool_list_message(tools);
        // 设置正确的ID
        tools_list_msg["id"] = protocol.get_message_id(message);
        send_message(client, tools_list_msg);
    } else if (method == "tools/call") {
        // 调用工具
        Dictionary params = protocol.get_message_params(message);
        if (params.has("name") && params.has("arguments")) {
            String tool_name = params["name"];
            Dictionary arguments = params["arguments"];
            String request_id = protocol.get_message_id(message);
            
            if (registered_tools.has(tool_name)) {
                MCPTool* tool = registered_tools[tool_name];
                Dictionary tool_result = tool->execute(arguments);
                
                Dictionary tool_result_msg = protocol.create_tool_result_message(tool_name, tool_result, request_id);
                send_message(client, tool_result_msg);
            } else {
                Dictionary error_msg = protocol.create_error_message("Tool not found: " + tool_name);
                send_message(client, error_msg);
            }
        } else {
            Dictionary error_msg = protocol.create_error_message("Invalid tool call parameters");
            send_message(client, error_msg);
        }
    } else {
        Dictionary error_msg = protocol.create_error_message("Unknown method: " + method);
        send_message(client, error_msg);
    }
}

Error MCPServer::send_message(Ref<StreamPeerTCP> client, const Dictionary& message) {
    // 将消息转换为JSON
    String json_str = JSON::stringify(message);
    PackedByteArray data = json_str.to_utf8_buffer();
    
    // 发送数据长度
    int len = data.size();
    Error err = client->put_data((const uint8_t*)&len, sizeof(len));
    if (err != OK) {
        return err;
    }
    
    // 发送JSON数据
    err = client->put_data(data.ptr(), len);
    return err;
}