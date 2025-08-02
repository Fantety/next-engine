/*
 * @FilePath: \editor\ai_component\engine_operator.cpp
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-07-10 18:34:02
 * @LastEditors: Fantety
 * @LastEditTime: 2025-08-02 21:05:50
 */
#include "engine_operator.h"
#include "editor/ai_component/tools/string_tag_wrapper.h"


EngineOperator::EngineOperator(){
    ToolsName["get_project_structure"] = GET_PROJECT_STRUCTURE;
    ToolsName["get_file_content"] = GET_FILE_CONTENT;
}

void EngineOperator::add_tool(String tool_name, Array tool_arguments){
    if(tool_name.is_empty()) return;
    if(!tools_list.empty()){
        if(tools_list.back()->tool_name == tool_name && is_arguments_equal(tool_arguments)) return;
    }
    ToolComponent* temp_tool = new ToolComponent;
    temp_tool->tool_name = tool_name;
    temp_tool->tool_arguments = tool_arguments;
    tools_list.push(temp_tool);
}

void EngineOperator::add_tool_with_parse(const String& tool_data){
    // 解析JSON格式的工具调用字符串
    // 例如: {"tool":"write_to_file", "args":"/tmp/test.txt<:>内容"}
    // 使用JSON解析器解析工具数据
    Ref<JSON> json_parser;
    json_parser.instantiate();
    // 解析JSON字符串
    Error err = json_parser->parse(tool_data);
    if (err != OK) {
        // 解析失败，直接返回
        return;
    }
    // 获取解析后的数据
    Variant json_data = json_parser->get_data();
    // 确保数据是字典类型
    if (json_data.get_type() != Variant::DICTIONARY) {
        return;
    }
    // 转换为字典
    Dictionary tool_dict = json_data;
    // 提取工具名称
    if (!tool_dict.has("tool")) {
        return;
    }
    String tool_name = tool_dict["tool"];
    // 处理参数
    Array tool_arguments;
    // 如果有参数，处理参数
    if (tool_dict.has("args")) {
        String args_str = tool_dict["args"];
        // 如果参数包含分隔符<:>，则分割参数
        if (args_str.contains("<:>")) {
            // 分割参数字符串
            PackedStringArray args_array = args_str.split("<:>");
            // 如果是write_to_file工具，需要特殊处理参数
            if (tool_name == "write_to_file" && args_array.size() == 2) {
                // 创建参数字典
                Dictionary file_args;
                file_args["path"] = args_array[0];
                file_args["content"] = args_array[1];
                // 添加到参数数组
                tool_arguments.append(file_args);
            } else {
                // 对于其他工具，将参数作为字符串数组处理
                for (int i = 0; i < args_array.size(); i++) {
                    tool_arguments.append(args_array[i]);
                }
            }
        } else {
            // 没有分隔符，将整个参数字符串作为一个参数
            // 对于get_project_structure等不需要参数的工具
            if (args_str == "") {
                // 不需要参数
            } else {
                // 其他工具将参数作为一个字符串处理
                tool_arguments.append(args_str);
            }
        }
    }
    // 调用add_tool函数
    add_tool(tool_name, tool_arguments);
}

bool EngineOperator::is_arguments_equal(Array arguments){
    if(arguments.size() != tools_list.back()->tool_arguments.size()) return false;
    for(int i = 0; i < arguments.size(); i++){
        if(arguments[i] != tools_list.front()->tool_arguments[i]) return false;
    }
    return true;
}

int EngineOperator::get_tool_count(){
    return tools_list.size();
}

Dictionary EngineOperator::get_tool_result(){
    Dictionary temp;
    switch ((EngineOperator::ToolsType)ToolsName[tools_list.front()->tool_name])
    {
        case EngineOperator::GET_PROJECT_STRUCTURE:{
            String data_t = dir_serializer.directory_to_json("res://");
            temp["role"] = "user";
            temp["content"] = StringTagWrapper::wrap_with_tag(data_t, "observation");
            break;
        }
        case EngineOperator::GET_FILE_CONTENT:{
            temp["role"] = "user";
            if(tools_list.front()->tool_arguments.size() == 0){
                temp["content"] = StringTagWrapper::wrap_with_tag("Path parameter not passed successfully", "observation");
                break;
            }
            Dictionary dict = tools_list.front()->tool_arguments[0];
            if(dict.has("path")){
                String data_t = StringTagWrapper::wrap_with_tag(file_serializer.read_file_to_string(dict["path"]), "observation");
                temp["content"] = data_t;
            }
            else{
                temp["content"] = StringTagWrapper::wrap_with_tag("An unknown error occurred", "observation");
            }
            break;
        }
        default:{
            break;
        }
    }
    ToolComponent* ptr = tools_list.front();
    delete ptr;
    tools_list.pop();
    return temp;
}

ToolComponent* EngineOperator::get_first(){
    return tools_list.front();
}

bool EngineOperator::is_empty(){
    return tools_list.empty();
}