/*
 * @FilePath: \editor\ai_component\mcp\mcp_tool_register.h
 * @Author: Fantety
 * @Descripttion: MCP Tool Register for Godot Engine
 * @Date: 2025-07-14
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-14
 */
#ifndef MCP_TOOL_REGISTER_H
#define MCP_TOOL_REGISTER_H

#include "core/object/class_db.h"
#include "mcp_tool.h"

class MCPToolRegister {
public:
    // 注册所有MCP工具
    static void register_all_tools();
    
    // 注册示例工具
    static void register_example_tool();
};

#endif // MCP_TOOL_REGISTER_H