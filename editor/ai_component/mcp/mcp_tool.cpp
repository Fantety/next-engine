/*
 * @FilePath: \editor\ai_component\mcp\mcp_tool.cpp
 * @Author: Fantety
 * @Descripttion: MCP Tool base class implementation for Godot Engine
 * @Date: 2025-07-14
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-14
 */
#include "mcp_tool.h"

void MCPTool::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_name"), &MCPTool::get_name);
    ClassDB::bind_method(D_METHOD("set_name", "name"), &MCPTool::set_name);
    ClassDB::bind_method(D_METHOD("get_title"), &MCPTool::get_title);
    ClassDB::bind_method(D_METHOD("set_title", "title"), &MCPTool::set_title);
    ClassDB::bind_method(D_METHOD("get_description"), &MCPTool::get_description);
    ClassDB::bind_method(D_METHOD("set_description", "description"), &MCPTool::set_description);
    ClassDB::bind_method(D_METHOD("get_input_mode"), &MCPTool::get_input_mode);
    ClassDB::bind_method(D_METHOD("set_input_mode", "input_mode"), &MCPTool::set_input_mode);
    ClassDB::bind_method(D_METHOD("get_tool_description"), &MCPTool::get_tool_description);
    ClassDB::bind_method(D_METHOD("execute", "arguments"), &MCPTool::execute);
    
    BIND_ENUM_CONSTANT(INPUT_MODE_NONE);
    BIND_ENUM_CONSTANT(INPUT_MODE_REQUIRED);
    BIND_ENUM_CONSTANT(INPUT_MODE_OPTIONAL);
}

MCPTool::MCPTool() {
    input_mode = INPUT_MODE_NONE;
}

String MCPTool::get_name() const {
    return name;
}

void MCPTool::set_name(const String& p_name) {
    name = p_name;
    // 默认情况下，title与name相同
    if (title.is_empty()) {
        title = p_name;
    }
}

String MCPTool::get_title() const {
    return title;
}

void MCPTool::set_title(const String& p_title) {
    title = p_title;
}

String MCPTool::get_description() const {
    return description;
}

void MCPTool::set_description(const String& p_description) {
    description = p_description;
}

MCPTool::InputMode MCPTool::get_input_mode() const {
    return input_mode;
}

void MCPTool::set_input_mode(InputMode p_input_mode) {
    input_mode = p_input_mode;
}

Dictionary MCPTool::get_tool_description() const {
    Dictionary tool_desc;
    tool_desc["name"] = name;
    tool_desc["title"] = title;
    tool_desc["description"] = description;
    
    // 添加inputSchema字段
    // 根据用户提供的示例，即使input_mode为NONE也应该添加inputSchema
    tool_desc["inputSchema"] = get_input_schema();
    
    return tool_desc;
}

Dictionary MCPTool::get_input_schema() const {
    // 默认实现返回空字典
    // 子类应该重写此方法以提供实际的输入模式
    return Dictionary();
}