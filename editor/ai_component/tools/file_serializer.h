/*
 * @FilePath: \editor\ai_component\tools\file_serializer.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-07-14 15:59:25
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-14 16:23:57
 */
#ifndef FILE_SERIALIZER_H
#define FILE_SERIALIZER_H
#include "core/io/json.h"
#include "core/io/file_access.h"
#include "core/string/print_string.h"


class FileSerializer{
    Ref<FileAccess> file_access;
public:
    FileSerializer(){};
    ~FileSerializer(){};

    String read_file_to_string(const String &p_file_path);
};

#endif