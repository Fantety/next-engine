/*
 * @FilePath: \editor\ai_component\mcp\mcp_http_server.h
 * @Author: Fantety
 * @Descripttion: MCP HTTP Server implementation for Godot Engine
 * @Date: 2025-08-06
 * @LastEditors: Fantety
 * @LastEditTime: 2025-08-06
 */
#ifndef MCP_HTTP_SERVER_H
#define MCP_HTTP_SERVER_H

#include "scene/main/streamable_http_server.h"
#include "mcp_protocol.h"
#include "mcp_tool.h"
#include "core/templates/hash_map.h"
#include "core/variant/array.h"

class MCPHttpServer : public StreamableHttpServer {
    GDCLASS(MCPHttpServer, StreamableHttpServer);

private:
    // 存储已注册的MCP工具
    HashMap<String, MCPTool*> registered_tools;
    
    // 协议处理器
    Ref<MCPProtocol> protocol;
    
    // 单例实例
    static inline MCPHttpServer* singleton = nullptr;

protected:
    static void _bind_methods();

public:
    MCPHttpServer();
    ~MCPHttpServer();

    // 工具注册方法
    void register_tool(const String& name, MCPTool* tool);
    void unregister_tool(const String& name);
    MCPTool* get_tool(const String& name) const;
    Array get_registered_tools_list() const;
    
    // 静态工具注册方法
    static void register_tool_static(const String& name, MCPTool* tool);
    
    // 获取单例实例
    static MCPHttpServer* get_singleton();
    
    // 处理MCP消息
    void _handle_mcp_message(Ref<StreamPeerTCP> client, const Dictionary& message);
    
    // 发送消息
    Error send_message(Ref<StreamPeerTCP> client, const Dictionary& message);
    
    // 发送流消息
    void send_stream_message(Ref<StreamPeerTCP> client, const Dictionary& message);
};

#endif // MCP_HTTP_SERVER_H