/**************************************************************************/
/*  ai_scene_editing_service.h                                            */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/object/property_info.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/io/resource.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class Node;

struct AISceneEditingResult {
	bool success = false;
	String error;
	String message;
	Dictionary metadata;
};

class AISceneEditingService : public RefCounted {
	GDCLASS(AISceneEditingService, RefCounted);

	struct MainThreadRequest {
		enum Operation {
			OP_CREATE_SCENE,
			OP_ADD_NODE,
			OP_INSTANTIATE_SCENE,
			OP_DELETE_NODE,
			OP_RENAME_NODE,
			OP_MOVE_NODE,
			OP_SET_PROPERTY,
			OP_LIST_PROPERTIES,
			OP_SAVE_CURRENT_SCENE,
			OP_OPEN_SCENE,
		};

		Operation operation = OP_CREATE_SCENE;
		String root_type;
		String root_name;
		String scene_path;
		String parent_path;
		String node_type;
		String node_name;
		String node_path;
		String new_name;
		String new_parent_path;
		String property_path;
		String property_filter;
		Variant value;
		int position = -1;
		int max_properties = 120;
		bool include_read_only = false;
		bool include_current_values = true;
		AISceneEditingResult result;
		Semaphore done;
	};

	mutable Mutex request_mutex;

	static AISceneEditingService *get_dispatcher_singleton();
	void _execute_request(uint64_t p_request_ptr);
	void _execute_request_ptr(MainThreadRequest *p_request);
	AISceneEditingResult _dispatch_to_main_thread(MainThreadRequest &r_request);

	Node *_get_edited_scene(String &r_error) const;
	Node *_resolve_node_path(Node *p_scene_root, const String &p_path, bool p_allow_root, String &r_error) const;
	Node *_instantiate_node(const String &p_type, String &r_error) const;
	String _normalize_node_name(Node *p_parent, Node *p_node, const String &p_requested_name) const;
	bool _normalize_scene_save_path(const String &p_path, String &r_path, String &r_error) const;
	bool _get_current_scene_save_path_main_thread(Node *p_scene, String &r_path, String &r_error) const;
	bool _ensure_scene_save_directory(const String &p_path, String &r_error) const;
	bool _save_scene_main_thread(Node *p_scene, const String &p_path, String &r_saved_path, String &r_error) const;
	bool _save_current_scene_main_thread(Node *p_scene, String &r_saved_path, String &r_error) const;
	bool _normalize_property_path(const String &p_property_path, Vector<StringName> &r_names, String &r_error) const;
	Array _get_resource_hint_types(const String &p_hint_string) const;
	bool _is_resource_type_allowed(const String &p_type, const String &p_hint_string) const;
	Ref<Resource> _instantiate_resource(const String &p_type, String &r_error) const;
	bool _apply_object_properties(Object *p_object, const Dictionary &p_properties, String &r_error) const;
	bool _find_indexed_property_info(Object *p_object, const Vector<StringName> &p_names, PropertyInfo &r_property_info) const;
	bool _convert_value_for_property(const Variant &p_value, const PropertyInfo *p_property_info, Variant::Type p_target_type, Variant &r_value, String &r_error) const;
	bool _convert_value_for_target_type(const Variant &p_value, Variant::Type p_target_type, Variant &r_value, String &r_error) const;
	bool _convert_dictionary_typed_value(const Dictionary &p_value, Variant::Type p_target_type, Variant &r_value, String &r_error) const;
	bool _convert_array_typed_value(const Array &p_value, Variant::Type p_target_type, Variant &r_value, String &r_error) const;
	bool _set_indexed_property(Object *p_object, const String &p_property_path, const Variant &p_value, String &r_error) const;
	bool _node_tree_contains_scene_path(Node *p_node, const String &p_scene_path) const;
	String _preview_variant_value(const Variant &p_value) const;
	Array _get_common_subproperty_paths(const String &p_property_path, Variant::Type p_type) const;
	String _build_resource_value_example(const PropertyInfo &p_property_info) const;
	AISceneEditingResult _create_scene_main_thread(const String &p_root_type, const String &p_root_name, const String &p_path);
	AISceneEditingResult _add_node_main_thread(const String &p_parent_path, const String &p_type, const String &p_name);
	AISceneEditingResult _instantiate_scene_main_thread(const String &p_parent_path, const String &p_scene_path, const String &p_name, int p_position);
	AISceneEditingResult _delete_node_main_thread(const String &p_node_path);
	AISceneEditingResult _rename_node_main_thread(const String &p_node_path, const String &p_new_name);
	AISceneEditingResult _move_node_main_thread(const String &p_node_path, const String &p_new_parent_path, int p_position);
	AISceneEditingResult _set_property_main_thread(const String &p_node_path, const String &p_property_path, const Variant &p_value);
	AISceneEditingResult _list_properties_main_thread(const String &p_node_path, const String &p_filter, int p_max_properties, bool p_include_read_only, bool p_include_current_values);
	AISceneEditingResult _save_current_scene_request_main_thread();
	AISceneEditingResult _open_scene_main_thread(const String &p_path);
	void _select_node(Node *p_node) const;
	void _update_scene_tree() const;

protected:
	static void _bind_methods();

public:
	AISceneEditingResult create_scene(const String &p_root_type, const String &p_root_name, const String &p_path);
	AISceneEditingResult add_node(const String &p_parent_path, const String &p_type, const String &p_name);
	AISceneEditingResult instantiate_scene(const String &p_parent_path, const String &p_scene_path, const String &p_name, int p_position);
	AISceneEditingResult delete_node(const String &p_node_path);
	AISceneEditingResult rename_node(const String &p_node_path, const String &p_new_name);
	AISceneEditingResult move_node(const String &p_node_path, const String &p_new_parent_path, int p_position);
	AISceneEditingResult set_property(const String &p_node_path, const String &p_property_path, const Variant &p_value);
	AISceneEditingResult list_properties(const String &p_node_path, const String &p_filter, int p_max_properties, bool p_include_read_only, bool p_include_current_values);
	AISceneEditingResult save_current_scene();
	AISceneEditingResult open_scene(const String &p_path);
};
