#ifndef DIR_SERIALIZER_H
#define DIR_SERIALIZER_H

#include "core/io/json.h"
#include "core/io/dir_access.h"
#include "core/string/print_string.h"

class DirSerializer{
private:
    Ref<DirAccess> dir_access;
    bool include_files;
    bool include_hidden;
    bool include_metadata;

public:
    DirSerializer();
    ~DirSerializer();
    // 设置选项
    void set_include_files(bool p_include);
    void set_include_hidden(bool p_include);
    void set_include_metadata(bool p_include);
    // 转换目录为 JSON
    String directory_to_json(const String &p_path, Error *r_error = nullptr);
private:
    // 递归构建目录结构
    Dictionary _build_directory_structure(const String &p_path);
};

#endif