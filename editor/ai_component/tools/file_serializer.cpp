#include "file_serializer.h"


String FileSerializer::read_file_to_string(const String &p_file_path) {
    Error err;
    file_access = FileAccess::open(p_file_path, FileAccess::READ, &err);
    if (err != OK || file_access.is_null()) {
        String error_msg;
        switch (err) {
            case ERR_FILE_NOT_FOUND:
                error_msg = "ERR_FILE_NOT_FOUND";
                break;
            case ERR_FILE_NO_PERMISSION:
                error_msg = "ERR_FILE_NO_PERMISSION";
                break;
            case ERR_FILE_CANT_OPEN:
                error_msg = "ERR_FILE_CANT_OPEN";
                break;
            default:
                error_msg = "Unknow Error (" + itos(err) + ")";
        }
        ERR_PRINT(vformat("Failed to read file [%s]: %s", p_file_path, error_msg));
        return error_msg;
    }
    // 确保文件正确关闭（利用RAII特性，Ref会自动管理生命周期）
    String file_content;
    try {
        file_content = file_access->get_as_text(false);
    } catch (...) {
        ERR_PRINT(vformat("Exception occurred while reading file contents: %s", p_file_path));
        return "";
    }
    // 显式关闭文件（可选，Ref析构时会自动关闭）
    file_access->close();
    return file_content;
}
