/*
 * @FilePath: \editor\ai_component\mcp\mcp_protocol.h
 * @Author: Fantety
 * @Descripttion: MCP Protocol handler for Godot Engine
 * @Date: 2025-07-14
 * @LastEditors: Fantety
 * @LastEditTime: 2025-08-02 21:16:46
 */
#ifndef MCP_PROTOCOL_H
#define MCP_PROTOCOL_H

#include "core/object/object.h"
#include "core/object/class_db.h"
#include "core/io/stream_peer_tcp.h"
#include "core/variant/dictionary.h"
#include "core/variant/array.h"
#include "core/string/ustring.h"

// MCP工具基类前置声明
class MCPTool;

class MCPProtocol : public Object {
    GDCLASS(MCPProtocol, Object);

private:
    // 存储初始化消息
    Dictionary init_message;
    
    // 构建初始化消息
    void _build_init_message();

protected:
    static void _bind_methods();

public:
    MCPProtocol();
    ~MCPProtocol(){};

    // 协议消息处理
    Dictionary create_init_message();
    Dictionary create_initialize_message();
    Dictionary create_ping_message();
    Dictionary create_pong_message();
    Dictionary create_complete_message(const String& request_id);
    Dictionary create_completion_message(const String& completion);
    Dictionary create_error_message(const String& error);
    Dictionary create_notification_message(const String& method, const Dictionary& params);
    
    // 工具相关消息
    Dictionary create_tool_list_message(const Array& tools);
    Dictionary create_tool_call_message(const String& tool_name, const Dictionary& arguments, const String& request_id);
    Dictionary create_tool_result_message(const String& tool_name, const Dictionary& result, const String& request_id);
    
    // 消息验证
    bool is_valid_message(const Dictionary& message);
    bool is_request_message(const Dictionary& message);
    bool is_notification_message(const Dictionary& message);
    
    // 消息解析
    String get_message_method(const Dictionary& message);
    Dictionary get_message_params(const Dictionary& message);
    String get_message_id(const Dictionary& message);
    
    // 初始化消息处理
    void set_init_message(const Dictionary& p_init_message);
    Dictionary get_init_message() const;
};

#endif // MCP_PROTOCOL_H