/*
 * @FilePath: \editor\ai_component\ai_ide_interface.cpp
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-07-10 18:34:02
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-17 13:55:31
 */
#include "ai_ide_interface.h"


AIIDEInterface::AIIDEInterface(){
    ToolsName["get_project_structure"] = GET_PROJECT_STRUCTURE;
    ToolsName["get_file_content"] = GET_FILE_CONTENT;
}

void AIIDEInterface::add_tool(String tool_name, Array tool_arguments, String id){
    if(tool_name.is_empty()) return;
    if(!tools_list.empty()){
        if(tools_list.back()->tool_name == tool_name && is_arguments_equal(tool_arguments)) return;
    }
    ToolComponent* temp_tool = new ToolComponent;
    temp_tool->tool_name = tool_name;
    temp_tool->tool_arguments = tool_arguments;
    temp_tool->id = id;
    tools_list.push(temp_tool);
}

bool AIIDEInterface::is_arguments_equal(Array arguments){
    if(arguments.size() != tools_list.back()->tool_arguments.size()) return false;
    for(int i = 0; i < arguments.size(); i++){
        if(arguments[i] != tools_list.front()->tool_arguments[i]) return false;
    }
    return true;
}

int AIIDEInterface::get_tool_count(){
    return tools_list.size();
}

Dictionary AIIDEInterface::get_tool_result(){
    Dictionary temp;
    switch ((AIIDEInterface::ToolsType)ToolsName[tools_list.front()->tool_name])
    {
        case AIIDEInterface::GET_PROJECT_STRUCTURE:{
            print_line("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");
            String data_t = dir_serializer.directory_to_json("res://");
            temp["role"] = "tool";
            temp["tool_call_id"] = tools_list.front()->id;
            print_line("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
            temp["content"] = data_t;
            break;
        }
        case AIIDEInterface::GET_FILE_CONTENT:{
            temp["role"] = "tool";
            temp["tool_call_id"] = tools_list.front()->id;
            if(tools_list.front()->tool_arguments.size() == 0){
                temp["content"] = "Path parameter not passed successfully";
                break;
            }
            Dictionary dict = tools_list.front()->tool_arguments[0];
            if(dict.has("path")){
                String data_t = file_serializer.read_file_to_string(dict["path"]);
                temp["content"] = data_t;
            }
            else{
                temp["content"] = "An unknown error occurred";
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

ToolComponent* AIIDEInterface::get_first(){
    return tools_list.front();
}

bool AIIDEInterface::is_empty(){
    return tools_list.empty();
}