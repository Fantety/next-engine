/**************************************************************************/
/*  ai_project_tool_utils.cpp                                             */
/**************************************************************************/

#include "ai_project_tool_utils.h"

#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/message_queue.h"
#include "core/os/thread.h"
#include "core/variant/variant.h"
#include "editor/editor_node.h"
#include "editor/file_system/editor_file_system.h"

bool AIV1ProjectToolUtils::is_allowed_path(const String &p_path) {
	return p_path.begins_with("res://") && !p_path.contains("..");
}

bool AIV1ProjectToolUtils::is_allowed_text_extension(const String &p_path) {
	const String ext = p_path.get_extension().to_lower();
	return ext == "gd" || ext == "cs" || ext == "tscn" || ext == "tres" || ext == "md" || ext == "txt" || ext == "json" || ext == "cfg" || ext == "shader" || ext == "gdshader";
}

bool AIV1ProjectToolUtils::is_allowed_image_extension(const String &p_path) {
	const String ext = p_path.get_extension().to_lower();
	return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "gif";
}

String AIV1ProjectToolUtils::get_path_argument(const Dictionary &p_arguments, const String &p_default_path) {
	return String(p_arguments.get("path", p_default_path)).strip_edges();
}

int AIV1ProjectToolUtils::get_int_argument(const Dictionary &p_arguments, const String &p_key, int p_default_value, int p_min_value, int p_max_value) {
	int value = p_arguments.get(p_key, p_default_value);
	return CLAMP(value, p_min_value, p_max_value);
}

void AIV1ProjectToolUtils::refresh_editor_file_system(const String &p_path, bool p_update_file) {
	if (!Thread::is_main_thread()) {
		if (MessageQueue::get_main_singleton()) {
			MessageQueue::get_main_singleton()->push_callable(callable_mp_static(&AIV1ProjectToolUtils::refresh_editor_file_system), p_path, p_update_file);
		}
		return;
	}

	EditorFileSystem *file_system = EditorFileSystem::get_singleton();
	if (!file_system) {
		return;
	}

	if (!EditorNode::get_singleton() || EditorNode::is_cmdline_mode()) {
		return;
	}
	if (p_update_file && !p_path.is_empty()) {
		file_system->update_file(p_path);
	}
	file_system->scan_changes();
}
