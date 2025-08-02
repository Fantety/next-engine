/*
 * @FilePath: \editor\ai_component\mcp\example_tool.h
 * @Author: Fantety
 * @Descripttion: Example MCP Tool for Godot Engine
 * @Date: 2025-07-14
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-14
 */
#ifndef EXAMPLE_TOOL_H
#define EXAMPLE_TOOL_H

#include "mcp_tool.h"
#include "core/object/class_db.h"

class ExampleTool : public MCPTool {
    GDCLASS(ExampleTool, MCPTool);

protected:
    static void _bind_methods();

public:
    ExampleTool();
    virtual Dictionary execute(const Dictionary& arguments) override;
    virtual Dictionary get_input_schema() const override;
};

#endif // EXAMPLE_TOOL_H