/*
 * @FilePath: \editor\ai_component\ai_ide_interface.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-07-07 18:28:31
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-13 12:56:14
 */
#ifndef AI_IDE_INTERFACE
#define AI_IDE_INTERFACE

#include "editor/docks/filesystem_dock.h"
#include "tools/dir_serializer.h"
#include "tools/file_serializer.h"


#include "scene/main/node.h"
#include <queue>
typedef struct ToolComponent
{
    String tool_name;
    Array tool_arguments;
    String id;
    /* data */
} ToolComponent;

class AIIDEInterface : public Node {
    GDCLASS(AIIDEInterface, Node);

    enum ToolsType
    {
        GET_PROJECT_STRUCTURE = 0,
        GET_FILE_CONTENT,
    };
    
    inline static Dictionary ToolsName;
    std::queue<ToolComponent*> tools_list;
    DirSerializer dir_serializer;
    FileSerializer file_serializer;


private:
    bool is_arguments_equal(Array arguments);

public:
    AIIDEInterface();
    void add_tool(String tool_name, Array tool_arguments, String id);
    int get_tool_count();
    Dictionary get_tool_result();
    ToolComponent* get_first();
    bool is_empty();

};


#endif