/**************************************************************************/
/*  ai_shader_editing_service.h                                           */
/**************************************************************************/

#pragma once

#include "core/io/resource.h"
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

class Node;
class Shader;

struct AIShaderEditingResult {
	bool success = false;
	String error;
	String message;
	Dictionary metadata;
};

class AIShaderEditingService : public RefCounted {
	GDCLASS(AIShaderEditingService, RefCounted);

	struct MainThreadRequest {
		enum Operation {
			OP_APPLY_TO_NODE,
		};

		Operation operation = OP_APPLY_TO_NODE;
		String node_path;
		String shader_path;
		String shader_code;
		String material_property;
		Dictionary shader_parameters;
		bool overwrite_shader = false;
		AIShaderEditingResult result;
		Semaphore done;
	};

	mutable Mutex request_mutex;

	static AIShaderEditingService *get_dispatcher_singleton();
	void _execute_request(uint64_t p_request_ptr);
	void _execute_request_ptr(MainThreadRequest *p_request);
	AIShaderEditingResult _dispatch_to_main_thread(MainThreadRequest &r_request);

	bool _normalize_shader_path(const String &p_path, String &r_path, String &r_error) const;
	bool _ensure_parent_directory(const String &p_path, String &r_error) const;
	Node *_get_edited_scene(String &r_error) const;
	Node *_resolve_node_path(Node *p_scene_root, const String &p_path, bool p_allow_root, String &r_error) const;
	String _resolve_material_property(Node *p_node, const String &p_requested_property, String &r_error) const;
	bool _property_accepts_shader_material(Node *p_node, const String &p_property_path) const;
	bool _save_shader_resource(const String &p_path, const String &p_code, bool p_overwrite, bool &r_existed_before, Ref<Shader> &r_shader, String &r_error) const;
	bool _save_current_scene_main_thread(Node *p_scene, String &r_saved_path, String &r_error) const;
	void _refresh_file_system(const String &p_path) const;
	void _update_scene_tree() const;
	AIShaderEditingResult _apply_to_node_main_thread(const String &p_node_path, const String &p_shader_path, const String &p_shader_code, const String &p_material_property, const Dictionary &p_shader_parameters, bool p_overwrite_shader);

protected:
	static void _bind_methods();

public:
	AIShaderEditingResult apply_to_node(const String &p_node_path, const String &p_shader_path, const String &p_shader_code, const String &p_material_property, const Dictionary &p_shader_parameters, bool p_overwrite_shader);
};
