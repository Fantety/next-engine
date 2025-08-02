/*
 * @FilePath: \editor\ai_component\mcp\mcp_tool.h
 * @Author: Fantety
 * @Descripttion: MCP Tool base class for Godot Engine
 * @Date: 2025-07-14
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-14
 */
#ifndef MCP_TOOL_H
#define MCP_TOOL_H

#include "core/object/object.h"
#include "core/object/class_db.h"
#include "core/variant/dictionary.h"
#include "core/variant/array.h"
#include "core/string/ustring.h"
#include "core/variant/type_info.h"

class MCPTool : public Object {
    GDCLASS(MCPTool, Object);

public:
    enum InputMode {
        INPUT_MODE_NONE,
        INPUT_MODE_REQUIRED,
        INPUT_MODE_OPTIONAL
    };

protected:
    String name;
    String title;
    String description;
    Dictionary input_schema;
    InputMode input_mode;
    
    static void _bind_methods();

public:
    MCPTool();
    virtual ~MCPTool();

    // 工具基本信息
    void set_name(const String& p_name);
    String get_name() const;
    
    void set_title(const String& p_title);
    String get_title() const;
    
    void set_description(const String& p_description);
    String get_description() const;
    
    void set_input_schema(const Dictionary& p_schema);
    virtual Dictionary get_input_schema() const;
    
    void set_input_mode(InputMode p_input_mode);
    InputMode get_input_mode() const;
    
    // 工具执行方法（需要子类实现）
    virtual Dictionary execute(const Dictionary& arguments) = 0;
    
    // 获取工具描述信息（用于MCP协议）
    Dictionary get_tool_description() const;
};

VARIANT_ENUM_CAST(MCPTool::InputMode)

#endif // MCP_TOOL_H