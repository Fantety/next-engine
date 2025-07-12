#include "ai_ide_interface.h"

void AIIDEInterface::add_tool(String tool){
    for(int i = 0; i < tools_list.size(); i++){
        if(tools_list[i] == tool) return;
    }
    tools_list.push_back(tool);
}

Vector<String> AIIDEInterface::get_tools(){
    return tools_list;
}
