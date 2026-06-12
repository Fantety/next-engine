/**************************************************************************/
/*  ai_shader_editing_service.h                                           */
/**************************************************************************/

#pragma once

#include "core/io/resource.h"
#include "core/object/property_info.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "editor/agent_v1/tools/ai_editor_tools_v1.h"
#include "editor/agent_v1/tools/editor/ai_editor_tool_service.h"

class Node;
class Shader;
class ShaderMaterial;

struct AIV1ShaderEditingResult {
	bool success = false;
	String error;
	String message;
	Dictionary metadata;
};

class AIV1ShaderEditingService : public AIV1EditorToolService {
	GDCLASS(AIV1ShaderEditingService, AIV1EditorToolService);

	struct MainThreadRequest {
		enum Operation {
			OP_CREATE_SHADER,
			OP_EDIT_SHADER,
			OP_DELETE_SHADER,
			OP_APPLY_TO_NODE,
			OP_SET_PARAMETERS,
		};

		Operation operation = OP_CREATE_SHADER;
		String node_path;
		String shader_path;
		String shader_type;
		String shader_code;
		String target_property;
		Dictionary shader_parameters;
		bool overwrite = false;
		Ref<AIV1ToolExecutionState> execution_context;
		AIV1ShaderEditingResult result;
		Semaphore done;
	};

	enum ShaderTargetKind {
		SHADER_TARGET_DIRECT_SHADER,
		SHADER_TARGET_SHADER_MATERIAL,
	};

	mutable Mutex request_mutex;

	static AIV1ShaderEditingService *get_dispatcher_singleton();
	void _execute_request(uint64_t p_request_ptr);
	void _execute_request_ptr(MainThreadRequest *p_request);
	AIV1ShaderEditingResult _dispatch_to_main_thread(MainThreadRequest &r_request);

	bool _normalize_shader_path(const String &p_path, String &r_path, String &r_error) const;
	bool _validate_shader_code(const String &p_code, String &r_error) const;
	bool _build_shader_code(const String &p_shader_type, const String &p_shader_code, String &r_code, String &r_error) const;
	bool _load_shader_resource(const String &p_path, Ref<Shader> &r_shader, String &r_error, bool p_ignore_cache = false) const;
	bool _save_shader_resource_via_editor(const Ref<Shader> &p_shader, const String &p_path, String &r_error) const;
	bool _delete_shader_resource(const String &p_path, String &r_error) const;
	bool _resource_hint_accepts_type(const String &p_hint_string, const StringName &p_type) const;
	bool _is_resource_type_allowed(const String &p_type, const String &p_hint_string) const;
	bool _get_exact_property_info(Object *p_object, const String &p_property_path, PropertyInfo &r_property_info) const;
	bool _resolve_shader_target(Node *p_node, const String &p_target_property, Vector<StringName> &r_property_names, ShaderTargetKind &r_target_kind, Variant &r_old_value, String &r_error) const;
	bool _save_current_scene_main_thread(Node *p_scene, String &r_saved_path, String &r_error) const;
	bool _normalize_property_path(const String &p_property_path, Vector<StringName> &r_names, String &r_error) const;
	Ref<Resource> _instantiate_resource(const String &p_type, String &r_error) const;
	bool _apply_object_properties(Object *p_object, const Dictionary &p_properties, String &r_error) const;
	bool _convert_array_typed_value(const Array &p_value, Variant::Type p_target_type, Variant &r_value, String &r_error) const;
	bool _convert_dictionary_typed_value(const Dictionary &p_value, Variant::Type p_target_type, Variant &r_value, String &r_error) const;
	bool _convert_value_for_target_type(const Variant &p_value, Variant::Type p_target_type, Variant &r_value, String &r_error) const;
	bool _convert_value_for_shader_uniform(const Variant &p_value, const PropertyInfo &p_uniform_info, Variant &r_value, String &r_error) const;
	bool _find_shader_uniform_info(const Ref<Shader> &p_shader, const StringName &p_parameter_name, PropertyInfo &r_uniform_info) const;
	bool _apply_shader_parameters_to_material(const Ref<ShaderMaterial> &p_material, const Dictionary &p_shader_parameters, Array &r_applied_parameters, String &r_error) const;
	AIV1ShaderEditingResult _create_shader_main_thread(const String &p_shader_path, const String &p_shader_type, const String &p_shader_code, bool p_overwrite);
	AIV1ShaderEditingResult _edit_shader_main_thread(const String &p_shader_path, const String &p_shader_code);
	AIV1ShaderEditingResult _delete_shader_main_thread(const String &p_shader_path);
	AIV1ShaderEditingResult _apply_to_node_main_thread(const String &p_node_path, const String &p_shader_path, const String &p_target_property, const Dictionary &p_shader_parameters);
	AIV1ShaderEditingResult _set_parameters_main_thread(const String &p_node_path, const String &p_target_property, const Dictionary &p_shader_parameters);

protected:
	static void _bind_methods();

public:
	AIV1ShaderEditingResult create_shader(const String &p_shader_path, const String &p_shader_type, const String &p_shader_code, bool p_overwrite);
	AIV1ShaderEditingResult edit_shader(const String &p_shader_path, const String &p_shader_code);
	AIV1ShaderEditingResult delete_shader(const String &p_shader_path);
	AIV1ShaderEditingResult apply_to_node(const String &p_node_path, const String &p_shader_path, const String &p_target_property, const Dictionary &p_shader_parameters);
	AIV1ShaderEditingResult set_parameters(const String &p_node_path, const String &p_target_property, const Dictionary &p_shader_parameters);
};
