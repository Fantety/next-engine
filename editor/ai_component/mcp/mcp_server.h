/*
 * @FilePath: \editor\ai_component\mcp\mcp_server.h
 * @Author: Fantety
 * @Descripttion: MCP Server implementation for Godot Engine
 * @Date: 2025-07-14
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-14
 */
#ifndef MCP_SERVER_H
#define MCP_SERVER_H

#include "core/object/object.h"
#include "core/object/class_db.h"
#include "core/io/tcp_server.h"
#include "core/io/net_socket.h"
#include "core/io/stream_peer_tcp.h"
#include "core/templates/hash_map.h"
#include "scene/main/node.h"
#include "mcp_protocol.h"
#include "core/variant/binder_common.h"

class MCPTool;

class MCPServer : public Node {
    GDCLASS(MCPServer, Node);

public:
    enum ServerStatus {
        MCP_SERVER_STOPPED,
        MCP_SERVER_STARTING,
        MCP_SERVER_RUNNING,
        MCP_SERVER_STOPPING,
        MCP_SERVER_ERROR
    };

private:
    Ref<TCPServer> server;
    bool server_active;
    uint16_t server_port;
    ServerStatus status;
    
    // 存储已注册的MCP工具
    HashMap<String, MCPTool*> registered_tools;
    
    // 存储客户端连接
    HashMap<Ref<StreamPeerTCP>, String> clients;
    
    // 协议处理器
    MCPProtocol protocol;
    
    // 单例实例
    static inline MCPServer* singleton = nullptr;

protected:
    static void _bind_methods();

public:
    MCPServer();
    ~MCPServer();

    // 服务器控制方法
    Error start_server(uint16_t port = 3000);
    void stop_server();
    bool is_server_running() const;
    ServerStatus get_status() const;
    
    // 工具注册方法
    void register_tool(const String& name, MCPTool* tool);
    void unregister_tool(const String& name);
    MCPTool* get_tool(const String& name) const;
    Array get_registered_tools_list() const;
    
    // 静态工具注册方法
    static void register_tool_static(const String& name, MCPTool* tool);
    
    // 获取单例实例
    static MCPServer* get_singleton();
    
    // 通知处理
    void _notification(int p_what);
    
    // 主循环处理
    void _process(double delta);
    
    // 网络处理
    void _accept_connections();
    void _handle_client_data(Ref<StreamPeerTCP> client);
    void _disconnect_client(Ref<StreamPeerTCP> client);
    
    // 消息处理
    void _handle_message(Ref<StreamPeerTCP> client, const Dictionary& message);
    
    // 发送消息
    Error send_message(Ref<StreamPeerTCP> client, const Dictionary& message);
};

VARIANT_ENUM_CAST(MCPServer::ServerStatus)

#endif // MCP_SERVER_H