/*
 * @FilePath: \editor\ai_component\ai_ide_interface.cpp
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-07-10 18:34:02
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-14 11:09:48
 */
#include "ai_ide_interface.h"


AIIDEInterface::AIIDEInterface(){
    ToolsName["get_project_structure"] = GET_PROJECT_STRUCTURE;
}

void AIIDEInterface::add_tool(String tool_name, Array tool_arguments, String id){
    if(tool_name.is_empty()) return;
    if(!tools_list.empty()){
        if(tools_list.back()->tool_name == tool_name) return;
    }
    ToolComponent* temp_tool = new ToolComponent;
    temp_tool->tool_name = tool_name;
    temp_tool->tool_arguments = tool_arguments;
    temp_tool->id = id;
    tools_list.push(temp_tool);
}

int AIIDEInterface::get_tool_count(){
    return tools_list.size();
}

Dictionary AIIDEInterface::get_tool_result(){
    Dictionary temp;
    switch ((AIIDEInterface::ToolsType)ToolsName[tools_list.front()->tool_name])
    {
        case AIIDEInterface::GET_PROJECT_STRUCTURE:{
            String data_t = dir_serializer.directory_to_json("res://");
            temp["role"] = "tool";
            temp["tool_call_id"] = tools_list.front()->id;
            temp["content"] = data_t;
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