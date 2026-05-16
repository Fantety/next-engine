/**************************************************************************/
/*  ai_file_context_provider.cpp                                           */
/**************************************************************************/

#include "ai_file_context_provider.h"

#include "core/io/file_access.h"
#include "core/object/class_db.h"

#include "editor/ai_component/context/ai_context_document.h"

void AIFileContextProvider::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_file_paths", "paths"), &AIFileContextProvider::set_file_paths);
	ClassDB::bind_method(D_METHOD("set_max_file_bytes", "max_file_bytes"), &AIFileContextProvider::set_max_file_bytes);
}

void AIFileContextProvider::set_file_paths(const PackedStringArray &p_paths) {
	file_paths = p_paths;
}

void AIFileContextProvider::set_max_file_bytes(int p_max_file_bytes) {
	max_file_bytes = MAX(1024, p_max_file_bytes);
}

Array AIFileContextProvider::collect_context() {
	Array result;
	for (int i = 0; i < file_paths.size(); i++) {
		String path = file_paths[i];
		AIContextDocument doc;
		doc.title = path.get_file();
		doc.source = path;

		if (!_is_allowed_path(path)) {
			doc.content = "File skipped: only res:// files can be read.";
			doc.truncated = true;
			result.push_back(doc.to_dict());
			continue;
		}
		if (!_is_allowed_extension(path)) {
			doc.content = "File skipped: extension is not in the text allowlist.";
			doc.truncated = true;
			result.push_back(doc.to_dict());
			continue;
		}
		if (!FileAccess::exists(path)) {
			doc.content = "File skipped: file does not exist.";
			doc.truncated = true;
			result.push_back(doc.to_dict());
			continue;
		}

		Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
		if (file.is_null()) {
			doc.content = "File skipped: failed to open file.";
			doc.truncated = true;
			result.push_back(doc.to_dict());
			continue;
		}

		uint64_t length = file->get_length();
		if (length > (uint64_t)max_file_bytes) {
			doc.truncated = true;
		}
		doc.content = file->get_as_text().substr(0, max_file_bytes);
		result.push_back(doc.to_dict());
	}
	return result;
}

bool AIFileContextProvider::_is_allowed_path(const String &p_path) const {
	return p_path.begins_with("res://") && !p_path.contains("..");
}

bool AIFileContextProvider::_is_allowed_extension(const String &p_path) const {
	const String ext = p_path.get_extension().to_lower();
	return ext == "gd" || ext == "cs" || ext == "tscn" || ext == "tres" || ext == "md" || ext == "txt" || ext == "json" || ext == "cfg" || ext == "shader" || ext == "gdshader";
}
