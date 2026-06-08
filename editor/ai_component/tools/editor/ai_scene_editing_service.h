/**************************************************************************/
/*  ai_scene_editing_service.h                                            */
/**************************************************************************/

#pragma once

#include "core/object/property_info.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/io/resource.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "editor/ai_component/tools/editor/ai_editor_tool_service.h"

class Node;
class EditorUndoRedoManager;

struct AISceneEditingResult {
	bool success = false;
	String error;
	String message;
	Dictionary metadata;
};

class AISceneEditingService : public AIEditorToolService {
	GDCLASS(AISceneEditingService, AIEditorToolService);

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
			OP_INSPECT_NODE,
			OP_DESCRIBE_TREE,
			OP_APPLY_PATCH,
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
		String root_path;
		Array property_paths;
		Dictionary patch;
		Variant value;
		int position = -1;
		int max_properties = 120;
		int max_depth = 8;
		int max_nodes = 200;
		bool include_read_only = false;
		bool include_current_values = true;
		bool include_internal = false;
		AISceneEditingResult result;
		Semaphore done;
	};

	mutable Mutex request_mutex;

	static AISceneEditingService *get_dispatcher_singleton();
	void _execute_request(uint64_t p_request_ptr);
	void _execute_request_ptr(MainThreadRequest *p_request);
	AISceneEditingResult _dispatch_to_main_thread(MainThreadRequest &r_request);

	Node *_resolve_node_path(Node *p_scene_root, const String &p_path, bool p_allow_root, String &r_error) const;
	Node *_instantiate_node(const String &p_type, String &r_error) const;
	String _normalize_node_name(Node *p_parent, Node *p_node, const String &p_requested_name) const;
	bool _normalize_scene_save_path(const String &p_path, String &r_path, String &r_error) const;
	bool _get_current_scene_save_path_main_thread(Node *p_scene, String &r_path, String &r_error) const;
	bool _ensure_scene_save_directory(const String &p_path, String &r_error) const;
	bool _save_scene_main_thread(Node *p_scene, const String &p_path, String &r_saved_path, String &r_error) const;
	bool _save_current_scene_main_thread(Node *p_scene, String &r_saved_path, String &r_error) const;
	void _rollback_new_scene_main_thread(Node *p_scene) const;
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
	bool _prepare_property_assignment(Object *p_object, const String &p_object_label, const String &p_property_path, const Variant &p_value, NodePath &r_property_path, Variant &r_current_value, Variant &r_converted_value, String &r_property_type, String &r_error) const;
	bool _apply_properties_direct(Node *p_node, const String &p_node_label, const Dictionary &p_properties, Array &r_applied_properties, String &r_error, String *r_failed_property_path = nullptr) const;
	bool _queue_properties_for_undo(EditorUndoRedoManager *p_undo_redo, Node *p_node, const String &p_node_label, const Dictionary &p_properties, Array &r_applied_properties, String &r_error, String *r_failed_property_path = nullptr) const;
	bool _set_indexed_property(Object *p_object, const String &p_property_path, const Variant &p_value, String &r_error) const;
	bool _node_tree_contains_scene_path(Node *p_node, const String &p_scene_path) const;
	bool _validate_patch_id(const String &p_id, String &r_error) const;
	Node *_resolve_patch_node(Node *p_scene_root, const String &p_locator, const HashMap<String, Node *> &p_created_nodes, bool p_allow_root, String &r_error) const;
	bool _describe_node_tree(Node *p_scene_root, Node *p_node, int p_depth, int p_max_depth, int p_max_nodes, bool p_include_internal, Array &r_nodes, String &r_content) const;
	String _preview_variant_value(const Variant &p_value) const;
	String _format_property_not_found_error(Object *p_object, const String &p_object_label, const String &p_property_path, const Vector<StringName> &p_property_names) const;
	bool _inspect_property_value(Node *p_node, const String &p_node_label, const String &p_property_path, Dictionary &r_property, String &r_error) const;
	Array _get_default_inspection_property_paths(Node *p_node) const;
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
	AISceneEditingResult _inspect_node_main_thread(const String &p_node_path, const Array &p_property_paths);
	AISceneEditingResult _describe_tree_main_thread(const String &p_root_path, int p_max_depth, int p_max_nodes, bool p_include_internal);
	AISceneEditingResult _apply_patch_main_thread(const Dictionary &p_patch);
	AISceneEditingResult _save_current_scene_request_main_thread();
	AISceneEditingResult _open_scene_main_thread(const String &p_path);
	void _select_node(Node *p_node) const;

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
	AISceneEditingResult inspect_node(const String &p_node_path, const Array &p_property_paths);
	AISceneEditingResult describe_tree(const String &p_root_path, int p_max_depth, int p_max_nodes, bool p_include_internal);
	AISceneEditingResult apply_patch(const Dictionary &p_patch);
	AISceneEditingResult save_current_scene();
	AISceneEditingResult open_scene(const String &p_path);
};
