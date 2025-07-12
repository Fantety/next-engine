/*
 * @FilePath: \editor\ai_component\ai_ide_interface.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-07-07 18:28:31
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-12 17:03:50
 */
#ifndef AI_IDE_INTERFACE
#define AI_IDE_INTERFACE

#include "editor/docks/filesystem_dock.h"
#include "scene/main/node.h"


class AIIDEInterface : public Node {
    GDCLASS(AIIDEInterface, Node);

    Vector<String> tools_list;

public:
    AIIDEInterface(){}
    void add_tool(String tool);
    Vector<String> get_tools();

};


#endif