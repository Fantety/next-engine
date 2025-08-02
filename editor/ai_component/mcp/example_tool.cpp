/*
 * @FilePath: \editor\ai_component\mcp\example_tool.cpp
 * @Author: Fantety
 * @Descripttion: Example MCP Tool implementation for Godot Engine
 * @Date: 2025-07-14
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-14
 */
#include "example_tool.h"

void ExampleTool::_bind_methods() {
    // 这里可以绑定特定于示例工具的方法
}

ExampleTool::ExampleTool() {
    // 设置工具的基本信息
    set_name("example_tool");
    set_description("An example tool that demonstrates how to implement MCP tools in Godot Engine.");
    set_input_mode(INPUT_MODE_OPTIONAL); // 设置输入模式为可选
}

Dictionary ExampleTool::execute(const Dictionary& arguments) {
    // 实现工具的具体功能
    Dictionary result;
    
    // 创建content数组
    Array content;
    Dictionary text_content;
    text_content["type"] = "text";
    
    // 检查是否有传入参数
    if (arguments.size() > 0) {
        text_content["text"] = "Received arguments: " + String(JSON::stringify(arguments));
    } else {
        text_content["text"] = "Hello from ExampleTool! No arguments provided.";
    }
    
    content.append(text_content);
    result["content"] = content;
    
    // 可以在这里添加更复杂的逻辑
    
    return result;
}

Dictionary ExampleTool::get_input_schema() const {
    // 定义工具的输入模式
    Dictionary schema;
    schema["type"] = "object";
    
    Dictionary properties;
    Dictionary message_prop;
    message_prop["type"] = "string";
    message_prop["description"] = "An optional message to include in the response";
    properties["message"] = message_prop;
    
    schema["properties"] = properties;
    
    return schema;
}