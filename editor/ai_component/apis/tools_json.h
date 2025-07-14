#ifndef TOOLS_JSON_STR_H
#define TOOLS_JSON_STR_H

#include "core/string/ustring.h"

namespace AITools {

// Auto-generated from tools.json
// DO NOT EDIT - Changes will be overwritten
const String TOOLS_JSON_STR =
    "[\n    {\n        \"type\": \"function\",\n        \"function\": {\n            "\
    "\"name\": \"get_project_structure\",\n            \"description\": \"Obtain the "\
    "complete directory structure of the project folder.\",\n            \"parameters\": "\
    "{\n                \"type\": \"object\",\n                \"properties\": {}\n  "\
    "          }\n        }\n    },\n    {\n        \"type\": \"function\",\n        "\
    "\"function\": {\n            \"name\": \"get_file_content\",\n            \"description\": "\
    "\"Get file content\",\n            \"parameters\": {\n                \"type\": "\
    "\"object\",\n                \"properties\": {\n                    \"path\": {\n "\
    "                       \"type\": \"string\",\n                        \"description\": "\
    "\"File path, starting with res://\"\n                    }\n                },\n "\
    "               \"required\": [\"path\"]\n            }\n        }\n    }\n]";

} // namespace AITools

#endif // TOOLS_JSON_STR_H
