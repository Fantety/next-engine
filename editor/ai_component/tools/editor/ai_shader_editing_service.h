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
#include "editor/ai_component/tools/ai_tool_execution_context.h"

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
			OP_CREATE_SHADER,
			OP_EDIT_SHADER,
			OP_DELETE_SHADER,
			OP_APPLY_TO_NODE,
		};

		Operation operation = OP_CREATE_SHADER;
		String node_path;
		String shader_path;
		String shader_type;
		String shader_code;
		String target_property;
		Dictionary shader_parameters;
		bool overwrite = false;
		Ref<AIToolExecutionContext> execution_context;
		AIShaderEditingResult result;
		Semaphore done;
	};

	enum ShaderTargetKind {
		SHADER_TARGET_DIRECT_SHADER,
		SHADER_TARGET_SHADER_MATERIAL,
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
	bool _validate_shader_code(const String &p_code, String &r_error) const;
	bool _build_shader_code(const String &p_shader_type, const String &p_shader_code, String &r_code, String &r_error) const;
	bool _load_shader_resource(const String &p_path, Ref<Shader> &r_shader, String &r_error, bool p_ignore_cache = false) const;
	bool _save_shader_resource_via_editor(const Ref<Shader> &p_shader, const String &p_path, String &r_error) const;
	bool _delete_shader_resource(const String &p_path, String &r_error) const;
	bool _resource_hint_accepts_type(const String &p_hint_string, const StringName &p_type) const;
	bool _get_exact_property_info(Object *p_object, const String &p_property_path, PropertyInfo &r_property_info) const;
	bool _resolve_shader_target(Node *p_node, const String &p_target_property, Vector<StringName> &r_property_names, ShaderTargetKind &r_target_kind, Variant &r_old_value, String &r_error) const;
	bool _save_current_scene_main_thread(Node *p_scene, String &r_saved_path, String &r_error) const;
	void _refresh_file_system(const String &p_path) const;
	void _update_scene_tree() const;
	AIShaderEditingResult _create_shader_main_thread(const String &p_shader_path, const String &p_shader_type, const String &p_shader_code, bool p_overwrite);
	AIShaderEditingResult _edit_shader_main_thread(const String &p_shader_path, const String &p_shader_code);
	AIShaderEditingResult _delete_shader_main_thread(const String &p_shader_path);
	AIShaderEditingResult _apply_to_node_main_thread(const String &p_node_path, const String &p_shader_path, const String &p_target_property, const Dictionary &p_shader_parameters);

protected:
	static void _bind_methods();

public:
	AIShaderEditingResult create_shader(const String &p_shader_path, const String &p_shader_type, const String &p_shader_code, bool p_overwrite);
	AIShaderEditingResult edit_shader(const String &p_shader_path, const String &p_shader_code);
	AIShaderEditingResult delete_shader(const String &p_shader_path);
	AIShaderEditingResult apply_to_node(const String &p_node_path, const String &p_shader_path, const String &p_target_property, const Dictionary &p_shader_parameters);
};
