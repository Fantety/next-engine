/*
 * @FilePath: \editor\ai_component\engine_operator.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-07-07 18:28:31
 * @LastEditors: Fantety
 * @LastEditTime: 2025-08-02 18:06:38
 */
#ifndef ENGINE_OPERATOR
#define ENGINE_OPERATOR

#include "editor/docks/filesystem_dock.h"
#include "tools/dir_serializer.h"
#include "tools/file_serializer.h"


#include "scene/main/node.h"
#include "core/io/json.h"
#include "core/os/time.h"
#include <queue>
typedef struct ToolComponent
{
    String tool_name;
    Array tool_arguments;
    /* data */
} ToolComponent;

class EngineOperator : public Node {
    GDCLASS(EngineOperator, Node);

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
    EngineOperator();
    void add_tool(String tool_name, Array tool_arguments);
    void add_tool_with_parse(const String& tool_data);
    int get_tool_count();
    String get_tool_result();
    String get_tool_name();
    ToolComponent* get_first();
    bool is_empty();

};


#endif