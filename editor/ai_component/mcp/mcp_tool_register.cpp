/*
 * @FilePath: \editor\ai_component\mcp\mcp_tool_register.cpp
 * @Author: Fantety
 * @Descripttion: MCP Tool Register implementation for Godot Engine
 * @Date: 2025-07-14
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-14
 */
#include "mcp_tool_register.h"
#include "example_tool.h"
#include "mcp_server.h"

void MCPToolRegister::register_all_tools() {
    // 注册所有MCP工具
    register_example_tool();
}

void MCPToolRegister::register_example_tool() {
    // 创建示例工具实例
    ExampleTool* example_tool = memnew(ExampleTool);
    
    // 通过MCPServer单例注册工具
    MCPServer::register_tool_static("example_tool", example_tool);
}