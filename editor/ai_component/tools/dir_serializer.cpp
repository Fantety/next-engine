/*
 * @FilePath: \editor\ai_component\tools\dir_serializer.cpp
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-07-12 11:11:48
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-12 12:42:04
 */
#include "dir_serializer.h"

DirSerializer::DirSerializer() :
        include_files(true),
        include_hidden(false),
        include_metadata(false) {
}

DirSerializer::~DirSerializer() {
}

void DirSerializer::set_include_files(bool p_include) {
    include_files = p_include;
}

void DirSerializer::set_include_hidden(bool p_include) {
    include_hidden = p_include;
}

void DirSerializer::set_include_metadata(bool p_include) {
    include_metadata = p_include;
}

String DirSerializer::directory_to_json(const String &p_path, Error *r_error) {
    Error err = OK;
    dir_access = DirAccess::open(p_path, &err);

    if (err != OK) {
        if (r_error) {
            *r_error = err;
        }
        return "{}";
    }

    Dictionary result = _build_directory_structure(p_path);
    String json_str = JSON::stringify(result);
    return json_str;
}

Dictionary DirSerializer::_build_directory_structure(const String &p_path) {
    Dictionary dir_info;
    dir_info["name"] = p_path.get_file();
    dir_info["path"] = p_path;

    if (include_metadata) {
        dir_info["is_dir"] = true;
        dir_info["size"] = 0; // 目录大小为0，或可递归计算
        dir_info["modified_time"] = 0; // 需要实现获取修改时间
    }
    Array children;
    if (dir_access->change_dir(p_path) == OK) {
        dir_access->list_dir_begin();
        String entry = dir_access->get_next();
        while (!entry.is_empty()) {
            if (entry == "." || entry == "..") {
                entry = dir_access->get_next();
                continue;
            }
            bool is_hidden = entry.begins_with(".");
            if (!include_hidden && is_hidden) {
                entry = dir_access->get_next();
                continue;
            }
            String full_path = p_path.path_join(entry);
            bool is_dir = dir_access->current_is_dir();
            if (is_dir) {
                Dictionary sub_dir = _build_directory_structure(full_path);
                children.push_back(sub_dir);
            } else if (include_files) {
                Dictionary file_info;
                file_info["name"] = entry;
                file_info["path"] = full_path;
                if (include_metadata) {
                    file_info["is_dir"] = false;
                    Ref<FileAccess> file = FileAccess::open(full_path, FileAccess::READ);
                    if (file.is_valid()) {
                        file_info["size"] = file->get_length();
                    } else {
                        file_info["size"] = 0;
                    }
                }
                children.push_back(file_info);
            }
            entry = dir_access->get_next();
        }
        dir_access->list_dir_end();
    }
    dir_info["children"] = children;
    return dir_info;
}    