/**************************************************************************/
/*  ai_script_editing_service.h                                           */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "editor/ai_component/tools/ai_tool_execution_context.h"

class Node;

struct AIScriptEditingResult {
	bool success = false;
	String error;
	String message;
	Dictionary metadata;
};

class AIScriptEditingService : public RefCounted {
	GDCLASS(AIScriptEditingService, RefCounted);

	struct MainThreadRequest {
		enum Operation {
			OP_CREATE_SCRIPT,
			OP_WRITE_SCRIPT,
			OP_PATCH_FUNCTION,
			OP_DELETE_SCRIPT,
			OP_BIND_TO_NODE,
			OP_UNBIND_FROM_NODE,
		};

		Operation operation = OP_CREATE_SCRIPT;
		String path;
		String extends_type;
		String source;
		String function_name;
		bool overwrite = false;
		bool create_if_missing = false;
		String node_path;
		String script_path;
		Ref<AIToolExecutionContext> execution_context;
		AIScriptEditingResult result;
		Semaphore done;
	};

	mutable Mutex request_mutex;

	static AIScriptEditingService *get_dispatcher_singleton();
	void _execute_request(uint64_t p_request_ptr);
	void _execute_request_ptr(MainThreadRequest *p_request);
	AIScriptEditingResult _dispatch_to_main_thread(MainThreadRequest &r_request);

	bool _is_allowed_script_path(const String &p_path, String &r_error) const;
	bool _read_text_file(const String &p_path, String &r_content, String &r_error) const;
	bool _write_text_file(const String &p_path, const String &p_content, String &r_error) const;
	bool _ensure_parent_directory(const String &p_path, String &r_error) const;
	bool _parse_gdscript(const String &p_path, const String &p_source, Dictionary &r_metadata, String &r_error) const;
	bool _build_script_source(const String &p_extends, const String &p_source, String &r_source, String &r_error) const;
	bool _replace_function_source(const String &p_source, const String &p_function_name, const String &p_function_source, bool p_create_if_missing, String &r_new_source, Dictionary &r_metadata, String &r_error) const;
	Node *_get_edited_scene(String &r_error) const;
	Node *_resolve_node_path(Node *p_scene_root, const String &p_path, bool p_allow_root, String &r_error) const;
	void _refresh_file_system(const String &p_path) const;
	void _update_scene_tree() const;
	bool _save_current_scene_main_thread(Node *p_scene, String &r_saved_path, String &r_error) const;
	AIScriptEditingResult _create_script_main_thread(const String &p_path, const String &p_extends, const String &p_source, bool p_overwrite);
	AIScriptEditingResult _write_script_main_thread(const String &p_path, const String &p_source, bool p_overwrite);
	AIScriptEditingResult _patch_function_main_thread(const String &p_path, const String &p_function_name, const String &p_function_source, bool p_create_if_missing);
	AIScriptEditingResult _delete_script_main_thread(const String &p_path);
	AIScriptEditingResult _bind_to_node_main_thread(const String &p_node_path, const String &p_script_path);
	AIScriptEditingResult _unbind_from_node_main_thread(const String &p_node_path);

protected:
	static void _bind_methods();

public:
	AIScriptEditingResult inspect_script(const String &p_path);
	AIScriptEditingResult create_script(const String &p_path, const String &p_extends, const String &p_source, bool p_overwrite);
	AIScriptEditingResult write_script(const String &p_path, const String &p_source, bool p_overwrite);
	AIScriptEditingResult patch_function(const String &p_path, const String &p_function_name, const String &p_function_source, bool p_create_if_missing);
	AIScriptEditingResult delete_script(const String &p_path);
	AIScriptEditingResult bind_to_node(const String &p_node_path, const String &p_script_path);
	AIScriptEditingResult unbind_from_node(const String &p_node_path);
};
