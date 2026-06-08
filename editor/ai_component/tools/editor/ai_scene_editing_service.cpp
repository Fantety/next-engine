/**************************************************************************/
/*  ai_scene_editing_service.cpp                                          */
/**************************************************************************/

#include "ai_scene_editing_service.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/object/property_info.h"
#include "core/os/thread.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/variant.h"
#include "core/variant/variant_utility.h"

#include "editor/ai_component/tools/editor/ai_documentation_service.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/editor_data.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/scene/scene_tree_editor.h"
#include "editor/scene/editor_scene_tabs.h"
#include "editor/settings/editor_settings.h"
#include "scene/2d/node_2d.h"
#include "scene/3d/node_3d.h"
#include "scene/gui/control.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"

void AISceneEditingService::_bind_methods() {
}

AISceneEditingService *AISceneEditingService::get_dispatcher_singleton() {
	static Ref<AISceneEditingService> dispatcher;
	if (dispatcher.is_null()) {
		dispatcher.instantiate();
	}
	return dispatcher.ptr();
}

AISceneEditingResult AISceneEditingService::_dispatch_to_main_thread(MainThreadRequest &r_request) {
	return _dispatch_main_thread_request<AISceneEditingResult>(r_request, get_dispatcher_singleton(), &AISceneEditingService::_execute_request, request_mutex, "Failed to schedule scene editing on the main thread.");
}

void AISceneEditingService::_execute_request(uint64_t p_request_ptr) {
	_execute_request_ptr(reinterpret_cast<MainThreadRequest *>(p_request_ptr));
}

void AISceneEditingService::_execute_request_ptr(MainThreadRequest *p_request) {
	ERR_FAIL_NULL(p_request);

	switch (p_request->operation) {
		case MainThreadRequest::OP_DELETE_NODE:
			p_request->result = _delete_node_main_thread(p_request->node_path);
			break;
		case MainThreadRequest::OP_LIST_PROPERTIES:
			p_request->result = _list_properties_main_thread(p_request->node_path, p_request->property_filter, p_request->max_properties, p_request->include_read_only, p_request->include_current_values);
			break;
		case MainThreadRequest::OP_INSPECT_NODE:
			p_request->result = _inspect_node_main_thread(p_request->node_path, p_request->property_paths);
			break;
		case MainThreadRequest::OP_DESCRIBE_TREE:
			p_request->result = _describe_tree_main_thread(p_request->root_path, p_request->max_depth, p_request->max_nodes, p_request->include_internal);
			break;
		case MainThreadRequest::OP_APPLY_PATCH:
			p_request->result = _apply_patch_main_thread(p_request->patch);
			break;
	}

	p_request->done.post();
}

Node *AISceneEditingService::_resolve_node_path(Node *p_scene_root, const String &p_path, bool p_allow_root, String &r_error) const {
	Node *node = AIEditorToolService::_resolve_node_path(p_scene_root, p_path, p_allow_root, r_error, true, "Node path was not found in the edited scene.");
	if (node == p_scene_root && !p_allow_root) {
		r_error = "The scene root cannot be used for this operation.";
		return nullptr;
	}
	return node;
}

Node *AISceneEditingService::_instantiate_node(const String &p_type, String &r_error) const {
	const String type = p_type.strip_edges();
	if (type.is_empty()) {
		r_error = "Node type is required.";
		return nullptr;
	}
	if (!ClassDB::class_exists(type)) {
		r_error = "Node type does not exist.";
		return nullptr;
	}
	if (!ClassDB::is_parent_class(type, SNAME("Node")) && type != "Node") {
		r_error = "Requested type is not a Node class.";
		return nullptr;
	}
	if (!ClassDB::can_instantiate(type)) {
		r_error = "Requested node type cannot be instantiated.";
		return nullptr;
	}

	Object *object = ClassDB::instantiate(type);
	Node *node = Object::cast_to<Node>(object);
	if (!node) {
		if (object) {
			memdelete(object);
		}
		r_error = "Failed to instantiate requested node type.";
		return nullptr;
	}
	return node;
}

String AISceneEditingService::_normalize_node_name(Node *p_parent, Node *p_node, const String &p_requested_name) const {
	ERR_FAIL_NULL_V(p_node, String());

	String name = p_requested_name.strip_edges();
	if (name.is_empty()) {
		name = p_node->get_class();
	}
	name = name.validate_node_name();
	if (GLOBAL_GET("editor/naming/node_name_casing").operator int() != Node::NAME_CASING_PASCAL_CASE) {
		name = Node::adjust_name_casing(name);
	}

	p_node->set_name(name);
	if (p_parent) {
		name = p_parent->validate_child_name(p_node);
		p_node->set_name(name);
	}
	return name;
}

bool AISceneEditingService::_normalize_scene_save_path(const String &p_path, String &r_path, String &r_error) const {
	String path = p_path.strip_edges().replace_char('\\', '/').simplify_path();
	if (path.is_empty()) {
		r_error = "Scene save path is required.";
		return false;
	}
	if (!path.begins_with("res://")) {
		r_error = "Scene save path must be inside the project and begin with res://.";
		return false;
	}
	if (path.contains("..")) {
		r_error = "Scene save path cannot contain parent directory traversal.";
		return false;
	}

	const String extension = path.get_extension().to_lower();
	if (extension != "tscn" && extension != "scn") {
		r_error = "Scene save path must end with .tscn or .scn.";
		return false;
	}
	if (path.get_file().is_empty()) {
		r_error = "Scene save path must include a file name.";
		return false;
	}

	r_path = path;
	return true;
}

bool AISceneEditingService::_get_current_scene_save_path_main_thread(Node *p_scene, String &r_path, String &r_error) const {
	ERR_FAIL_NULL_V(p_scene, false);

	String scene_path = p_scene->get_scene_file_path().strip_edges();
	if (scene_path.is_empty()) {
		r_error = "Current scene has no save path. Create or save the scene first.";
		return false;
	}
	scene_path = ProjectSettings::get_singleton()->localize_path(scene_path);
	return _normalize_scene_save_path(scene_path, r_path, r_error);
}

bool AISceneEditingService::_ensure_scene_save_directory(const String &p_path, String &r_error) const {
	const String base_dir = p_path.get_base_dir();
	if (base_dir.is_empty() || base_dir == "res://") {
		return true;
	}

	const String absolute_dir = ProjectSettings::get_singleton()->globalize_path(base_dir);
	Error err = DirAccess::make_dir_recursive_absolute(absolute_dir);
	if (err != OK) {
		r_error = vformat("Failed to create scene directory `%s` (error %d).", base_dir, err);
		return false;
	}
	return true;
}

bool AISceneEditingService::_save_scene_main_thread(Node *p_scene, const String &p_path, String &r_saved_path, String &r_error) const {
	ERR_FAIL_NULL_V(p_scene, false);

	String scene_path;
	if (!_normalize_scene_save_path(p_path, scene_path, r_error)) {
		return false;
	}
	if (!_ensure_scene_save_directory(scene_path, r_error)) {
		return false;
	}

	EditorNode *editor = EditorNode::get_singleton();
	if (!editor) {
		r_error = "EditorNode is not available.";
		return false;
	}

	int scene_index = EditorNode::get_editor_data().get_edited_scene();
	if (scene_index < 0) {
		r_error = "No edited scene index is available.";
		return false;
	}

	for (int i = 0; i < EditorNode::get_editor_data().get_edited_scene_count(); i++) {
		if (i != scene_index && EditorNode::get_editor_data().get_scene_path(i) == scene_path) {
			r_error = "Cannot overwrite a scene that is already open in another editor tab.";
			return false;
		}
	}

	p_scene->propagate_notification(Node::NOTIFICATION_EDITOR_PRE_SAVE);
	EditorNode::get_editor_data().apply_changes_in_editors();
	editor->save_default_environment();

	Ref<PackedScene> packed_scene;
	if (ResourceCache::has(scene_path)) {
		packed_scene = ResourceCache::get_ref(scene_path);
		if (packed_scene.is_valid()) {
			packed_scene->recreate_state();
		} else {
			packed_scene.instantiate();
		}
	} else {
		packed_scene.instantiate();
	}

	Error err = packed_scene->pack(p_scene);
	if (err != OK) {
		p_scene->propagate_notification(Node::NOTIFICATION_EDITOR_POST_SAVE);
		r_error = vformat("Failed to pack scene before saving (error %d).", err);
		return false;
	}

	uint32_t flags = ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;
	if (EDITOR_GET("filesystem/on_save/compress_binary_resources")) {
		flags |= ResourceSaver::FLAG_COMPRESS;
	}

	err = ResourceSaver::save(packed_scene, scene_path, flags);
	if (err != OK) {
		p_scene->propagate_notification(Node::NOTIFICATION_EDITOR_POST_SAVE);
		r_error = vformat("Failed to save scene `%s` (error %d).", scene_path, err);
		return false;
	}

	editor->emit_signal(SNAME("scene_saved"), scene_path);
	EditorNode::get_editor_data().notify_scene_saved(scene_path);
	EditorNode::get_editor_data().set_scene_path(scene_index, scene_path);
	EditorNode::get_editor_data().set_scene_as_saved(scene_index);
	EditorNode::get_editor_data().set_scene_modified_time(scene_index, FileAccess::get_modified_time(scene_path));

	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(scene_path);
	}
	if (EditorSceneTabs::get_singleton()) {
		EditorSceneTabs::get_singleton()->update_scene_tabs();
	}

	p_scene->propagate_notification(Node::NOTIFICATION_EDITOR_POST_SAVE);
	r_saved_path = scene_path;
	return true;
}

bool AISceneEditingService::_save_current_scene_main_thread(Node *p_scene, String &r_saved_path, String &r_error) const {
	ERR_FAIL_NULL_V(p_scene, false);

	String scene_path;
	if (!_get_current_scene_save_path_main_thread(p_scene, scene_path, r_error)) {
		return false;
	}
	return _save_scene_main_thread(p_scene, scene_path, r_saved_path, r_error);
}

void AISceneEditingService::_rollback_new_scene_main_thread(Node *p_scene) const {
	ERR_FAIL_NULL(p_scene);

	EditorNode *editor = EditorNode::get_singleton();
	ERR_FAIL_NULL(editor);
	if (editor->get_edited_scene() != p_scene) {
		return;
	}

	editor->close_scene();
}

bool AISceneEditingService::_normalize_property_path(const String &p_property_path, Vector<StringName> &r_names, String &r_error) const {
	const String stripped_path = p_property_path.strip_edges();
	if (stripped_path.is_empty()) {
		r_error = "Property path is required.";
		return false;
	}
	if (stripped_path.begins_with("/") || stripped_path.contains("..")) {
		r_error = "Only property paths relative to the target node are allowed.";
		return false;
	}

	r_names = NodePath(stripped_path).get_as_property_path().get_subnames();
	if (r_names.is_empty()) {
		r_error = "Property path is invalid.";
		return false;
	}

	return true;
}

bool AISceneEditingService::_convert_array_typed_value(const Array &p_value, Variant::Type p_target_type, Variant &r_value, String &r_error) const {
	Array args = p_value;
	Vector<const Variant *> argptrs;
	argptrs.resize(args.size());
	for (int i = 0; i < args.size(); i++) {
		argptrs.write[i] = &args[i];
	}

	Callable::CallError call_error;
	Variant converted;
	Variant::construct(p_target_type, converted, argptrs.is_empty() ? nullptr : const_cast<const Variant **>(argptrs.ptr()), argptrs.size(), call_error);
	if (call_error.error != Callable::CallError::CALL_OK) {
		r_error = vformat("Cannot construct %s from the provided array value.", Variant::get_type_name(p_target_type));
		return false;
	}

	r_value = converted;
	return true;
}

Array AISceneEditingService::_get_resource_hint_types(const String &p_hint_string) const {
	Array types;
	Vector<String> parts = p_hint_string.split(",", false);
	for (int i = 0; i < parts.size(); i++) {
		String type = parts[i].strip_edges();
		if (type.is_empty() || type.begins_with("-")) {
			continue;
		}
		types.push_back(type);
	}
	return types;
}

bool AISceneEditingService::_is_resource_type_allowed(const String &p_type, const String &p_hint_string) const {
	const String type = p_type.strip_edges();
	if (type.is_empty()) {
		return false;
	}
	if (p_hint_string.strip_edges().is_empty()) {
		return true;
	}

	bool explicitly_allowed = false;
	bool saw_known_positive_hint = false;
	Vector<String> parts = p_hint_string.split(",", false);
	for (int i = 0; i < parts.size(); i++) {
		String hint_type = parts[i].strip_edges();
		if (hint_type.is_empty()) {
			continue;
		}

		const bool excluded = hint_type.begins_with("-");
		if (excluded) {
			hint_type = hint_type.substr(1).strip_edges();
		}
		if (hint_type.is_empty()) {
			continue;
		}
		if (!ClassDB::class_exists(hint_type)) {
			continue;
		}
		if (!excluded) {
			saw_known_positive_hint = true;
		}

		const bool matches = type == hint_type || ClassDB::is_parent_class(type, hint_type);
		if (excluded && matches) {
			return false;
		}
		if (!excluded && matches) {
			explicitly_allowed = true;
		}
	}
	if (!saw_known_positive_hint && (type == "Resource" || ClassDB::is_parent_class(type, SNAME("Resource")))) {
		return true;
	}
	return explicitly_allowed;
}

Ref<Resource> AISceneEditingService::_instantiate_resource(const String &p_type, String &r_error) const {
	const String type = p_type.strip_edges();
	if (type.is_empty()) {
		r_error = "Resource type is required.";
		return Ref<Resource>();
	}
	if (!ClassDB::class_exists(type)) {
		r_error = vformat("Resource type `%s` does not exist.", type);
		return Ref<Resource>();
	}
	if (!ClassDB::is_parent_class(type, SNAME("Resource")) && type != "Resource") {
		r_error = vformat("Requested type `%s` is not a Resource class.", type);
		return Ref<Resource>();
	}
	if (!ClassDB::can_instantiate(type)) {
		r_error = vformat("Resource type `%s` cannot be instantiated.", type);
		return Ref<Resource>();
	}

	Object *object = ClassDB::instantiate(type);
	Resource *resource = Object::cast_to<Resource>(object);
	if (!resource) {
		if (object) {
			memdelete(object);
		}
		r_error = vformat("Failed to instantiate resource type `%s`.", type);
		return Ref<Resource>();
	}
	return Ref<Resource>(resource);
}

bool AISceneEditingService::_apply_object_properties(Object *p_object, const Dictionary &p_properties, String &r_error) const {
	ERR_FAIL_NULL_V(p_object, false);

	List<PropertyInfo> property_list;
	p_object->get_property_list(&property_list);

	for (const KeyValue<Variant, Variant> &E : p_properties) {
		const String property_path = String(E.key).strip_edges();
		if (property_path.is_empty()) {
			r_error = "Nested resource property path cannot be empty.";
			return false;
		}
		if (property_path.begins_with("/") || property_path.contains("..")) {
			r_error = vformat("Nested resource property path `%s` is invalid.", property_path);
			return false;
		}

		bool listed_property = false;
		PropertyInfo listed_property_info;
		for (const PropertyInfo &P : property_list) {
			if (String(P.name) == property_path) {
				listed_property = true;
				listed_property_info = P;
				break;
			}
		}

		Vector<StringName> property_names;
		if (listed_property) {
			property_names.push_back(StringName(property_path));
		} else if (!_normalize_property_path(property_path, property_names, r_error)) {
			return false;
		}

		if (listed_property && (listed_property_info.usage & PROPERTY_USAGE_READ_ONLY)) {
			r_error = vformat("Nested resource property `%s` is read-only.", property_path);
			return false;
		}

		bool valid = false;
		Variant current_value = p_object->get_indexed(property_names, &valid);
		if (!valid) {
			r_error = vformat("Nested resource property `%s` was not found.", property_path);
			return false;
		}

		Variant converted_value;
		Variant::Type target_type = listed_property ? listed_property_info.type : current_value.get_type();
		if (target_type == Variant::NIL) {
			target_type = current_value.get_type();
		}
		if (!_convert_value_for_property(E.value, listed_property ? &listed_property_info : nullptr, target_type, converted_value, r_error)) {
			r_error = vformat("Failed to convert nested resource property `%s`: %s", property_path, r_error);
			return false;
		}

		p_object->set_indexed(property_names, converted_value, &valid);
		if (!valid) {
			r_error = vformat("Godot rejected nested resource property `%s`.", property_path);
			return false;
		}
	}

	return true;
}

bool AISceneEditingService::_find_indexed_property_info(Object *p_object, const Vector<StringName> &p_names, PropertyInfo &r_property_info) const {
	ERR_FAIL_NULL_V(p_object, false);
	if (p_names.is_empty()) {
		return false;
	}

	Object *current_object = p_object;
	for (int i = 0; i < p_names.size(); i++) {
		List<PropertyInfo> property_list;
		current_object->get_property_list(&property_list);

		bool found = false;
		PropertyInfo found_info;
		for (const PropertyInfo &E : property_list) {
			if (StringName(E.name) == p_names[i]) {
				found = true;
				found_info = E;
				break;
			}
		}
		if (!found) {
			return false;
		}

		if (i == p_names.size() - 1) {
			r_property_info = found_info;
			return true;
		}

		bool valid = false;
		Variant current_value = current_object->get(StringName(found_info.name), &valid);
		if (!valid || current_value.get_type() != Variant::OBJECT) {
			return false;
		}

		Object *next_object = current_value;
		if (!next_object) {
			return false;
		}
		current_object = next_object;
	}

	return false;
}

bool AISceneEditingService::_convert_value_for_property(const Variant &p_value, const PropertyInfo *p_property_info, Variant::Type p_target_type, Variant &r_value, String &r_error) const {
	if (p_target_type == Variant::OBJECT && p_property_info && p_property_info->hint == PROPERTY_HINT_RESOURCE_TYPE) {
		if (p_value.get_type() == Variant::NIL) {
			r_value = Variant();
			return true;
		}

		Ref<Resource> resource_value = p_value;
		if (resource_value.is_valid()) {
			const String resource_type = resource_value->get_class();
			if (!_is_resource_type_allowed(resource_type, p_property_info->hint_string)) {
				r_error = vformat("Resource type `%s` is not allowed for this property. Expected: %s.", resource_type, p_property_info->hint_string);
				return false;
			}
			r_value = resource_value;
			return true;
		}

		if (p_value.get_type() != Variant::DICTIONARY) {
			r_error = "Resource properties expect null, an existing Resource, or a dictionary with resource_type/resource_path.";
			return false;
		}

		Dictionary dict = p_value;
		if (dict.has("value")) {
			return _convert_value_for_property(dict["value"], p_property_info, p_target_type, r_value, r_error);
		}

		Ref<Resource> resource;
		if (dict.has("resource_path")) {
			String path = String(dict["resource_path"]).strip_edges().replace_char('\\', '/').simplify_path();
			if (path.is_empty()) {
				r_error = "resource_path cannot be empty.";
				return false;
			}
			if (!path.begins_with("res://")) {
				r_error = "resource_path must begin with res://.";
				return false;
			}
			if (path.contains("..")) {
				r_error = "resource_path cannot contain parent directory traversal.";
				return false;
			}

			Error load_error = OK;
			resource = ResourceLoader::load(path, String(), ResourceLoader::CACHE_MODE_REUSE, &load_error);
			if (resource.is_null()) {
				r_error = vformat("Failed to load resource `%s` (error %d).", path, load_error);
				return false;
			}
			if (!_is_resource_type_allowed(resource->get_class(), p_property_info->hint_string)) {
				r_error = vformat("Loaded resource type `%s` is not allowed for this property. Expected: %s.", resource->get_class(), p_property_info->hint_string);
				return false;
			}
			if (dict.has("duplicate") ? bool(dict["duplicate"]) : true) {
				Ref<Resource> duplicate = resource->duplicate(true);
				if (duplicate.is_valid()) {
					resource = duplicate;
				}
			}
		} else {
			const String resource_type = String(dict.get("resource_type", "")).strip_edges();
			if (resource_type.is_empty()) {
				r_error = "Resource dictionary must provide resource_type or resource_path.";
				return false;
			}
			if (!_is_resource_type_allowed(resource_type, p_property_info->hint_string)) {
				r_error = vformat("Resource type `%s` is not allowed for this property. Expected: %s.", resource_type, p_property_info->hint_string);
				return false;
			}
			resource = _instantiate_resource(resource_type, r_error);
			if (resource.is_null()) {
				return false;
			}
		}

		if (dict.has("properties")) {
			if (Variant(dict["properties"]).get_type() != Variant::DICTIONARY) {
				r_error = "Resource dictionary `properties` must be an object.";
				return false;
			}
			Dictionary properties = dict["properties"];
			if (!_apply_object_properties(resource.ptr(), properties, r_error)) {
				return false;
			}
		}

		r_value = resource;
		return true;
	}

	return _convert_value_for_target_type(p_value, p_target_type, r_value, r_error);
}

bool AISceneEditingService::_convert_dictionary_typed_value(const Dictionary &p_value, Variant::Type p_target_type, Variant &r_value, String &r_error) const {
	const String type_name = String(p_value.get("type", "")).strip_edges();
	if (!type_name.is_empty()) {
		const Variant::Type requested_type = Variant::get_type_by_name(type_name);
		if (requested_type == Variant::VARIANT_MAX) {
			r_error = "Unknown typed value type.";
			return false;
		}
		if (requested_type != p_target_type) {
			r_error = vformat("Typed value declares %s but target property expects %s.", type_name, Variant::get_type_name(p_target_type));
			return false;
		}
	}

	if (p_value.has("value")) {
		return _convert_value_for_target_type(p_value["value"], p_target_type, r_value, r_error);
	}

	Array args;
	if (p_value.has("args")) {
		if (Variant(p_value["args"]).get_type() != Variant::ARRAY) {
			r_error = "Typed value args must be an array.";
			return false;
		}
		args = p_value["args"];
	} else {
		switch (p_target_type) {
			case Variant::VECTOR2:
			case Variant::VECTOR2I:
				if (p_value.has("x") && p_value.has("y")) {
					args.push_back(p_value["x"]);
					args.push_back(p_value["y"]);
				}
				break;
			case Variant::VECTOR3:
			case Variant::VECTOR3I:
				if (p_value.has("x") && p_value.has("y") && p_value.has("z")) {
					args.push_back(p_value["x"]);
					args.push_back(p_value["y"]);
					args.push_back(p_value["z"]);
				}
				break;
			case Variant::VECTOR4:
			case Variant::VECTOR4I:
			case Variant::COLOR:
				if (p_value.has("r") && p_value.has("g") && p_value.has("b")) {
					args.push_back(p_value["r"]);
					args.push_back(p_value["g"]);
					args.push_back(p_value["b"]);
					if (p_value.has("a")) {
						args.push_back(p_value["a"]);
					}
				} else if (p_value.has("x") && p_value.has("y") && p_value.has("z") && p_value.has("w")) {
					args.push_back(p_value["x"]);
					args.push_back(p_value["y"]);
					args.push_back(p_value["z"]);
					args.push_back(p_value["w"]);
				}
				break;
			case Variant::RECT2:
			case Variant::RECT2I:
				if (p_value.has("x") && p_value.has("y") && p_value.has("width") && p_value.has("height")) {
					args.push_back(p_value["x"]);
					args.push_back(p_value["y"]);
					args.push_back(p_value["width"]);
					args.push_back(p_value["height"]);
				}
				break;
			default:
				break;
		}
	}

	if (args.is_empty()) {
		r_error = vformat("Typed value for %s must provide `value`, `args`, or recognized component fields.", Variant::get_type_name(p_target_type));
		return false;
	}

	return _convert_array_typed_value(args, p_target_type, r_value, r_error);
}

bool AISceneEditingService::_convert_value_for_target_type(const Variant &p_value, Variant::Type p_target_type, Variant &r_value, String &r_error) const {
	if (p_target_type == Variant::NIL) {
		r_value = p_value;
		return true;
	}

	if (p_value.get_type() == p_target_type) {
		r_value = p_value;
		return true;
	}

	if (p_value.get_type() == Variant::DICTIONARY) {
		Dictionary value_dict = p_value;
		if (value_dict.has("type") || value_dict.has("value") || value_dict.has("args")) {
			return _convert_dictionary_typed_value(value_dict, p_target_type, r_value, r_error);
		}
	}

	if (p_value.get_type() == Variant::ARRAY) {
		switch (p_target_type) {
			case Variant::VECTOR2:
			case Variant::VECTOR2I:
			case Variant::RECT2:
			case Variant::RECT2I:
			case Variant::VECTOR3:
			case Variant::VECTOR3I:
			case Variant::VECTOR4:
			case Variant::VECTOR4I:
			case Variant::COLOR:
			case Variant::PLANE:
			case Variant::QUATERNION:
			case Variant::AABB:
			case Variant::BASIS:
			case Variant::TRANSFORM2D:
			case Variant::TRANSFORM3D:
			case Variant::PROJECTION:
			case Variant::PACKED_BYTE_ARRAY:
			case Variant::PACKED_INT32_ARRAY:
			case Variant::PACKED_INT64_ARRAY:
			case Variant::PACKED_FLOAT32_ARRAY:
			case Variant::PACKED_FLOAT64_ARRAY:
			case Variant::PACKED_STRING_ARRAY:
			case Variant::PACKED_VECTOR2_ARRAY:
			case Variant::PACKED_VECTOR3_ARRAY:
			case Variant::PACKED_COLOR_ARRAY:
			case Variant::PACKED_VECTOR4_ARRAY:
				return _convert_array_typed_value(p_value, p_target_type, r_value, r_error);
			default:
				break;
		}
	}

	if (!Variant::can_convert_strict(p_value.get_type(), p_target_type)) {
		r_error = vformat("Cannot convert value from %s to %s.", Variant::get_type_name(p_value.get_type()), Variant::get_type_name(p_target_type));
		return false;
	}

	const Variant *argptrs[1] = { &p_value };
	Callable::CallError call_error;
	Variant converted;
	Variant::construct(p_target_type, converted, argptrs, 1, call_error);
	if (call_error.error != Callable::CallError::CALL_OK) {
		r_error = vformat("Cannot construct %s from %s.", Variant::get_type_name(p_target_type), Variant::get_type_name(p_value.get_type()));
		return false;
	}

	r_value = converted;
	return true;
}

bool AISceneEditingService::_prepare_property_assignment(Object *p_object, const String &p_object_label, const String &p_property_path, const Variant &p_value, NodePath &r_property_path, Variant &r_current_value, Variant &r_converted_value, String &r_property_type, String &r_error) const {
	ERR_FAIL_NULL_V(p_object, false);

	const String stripped_property_path = p_property_path.strip_edges();
	if (stripped_property_path.is_empty()) {
		r_error = "Property path is required.";
		return false;
	}
	if (stripped_property_path.begins_with("/") || stripped_property_path.contains("..")) {
		r_error = "Only property paths relative to the target node are allowed.";
		return false;
	}

	List<PropertyInfo> property_list;
	p_object->get_property_list(&property_list);
	bool listed_property = false;
	PropertyInfo listed_property_info;
	for (const PropertyInfo &E : property_list) {
		if (String(E.name) == stripped_property_path) {
			listed_property = true;
			listed_property_info = E;
			break;
		}
	}

	Vector<StringName> property_names;
	if (listed_property) {
		property_names.push_back(StringName(stripped_property_path));
	} else if (!_normalize_property_path(stripped_property_path, property_names, r_error)) {
		return false;
	}
	r_property_path = NodePath(Vector<StringName>(), property_names, false);

	bool valid = false;
	r_current_value = p_object->get_indexed(property_names, &valid);
	if (!valid) {
		r_error = _format_property_not_found_error(p_object, p_object_label, stripped_property_path, property_names);
		return false;
	}
	if (listed_property && (listed_property_info.usage & PROPERTY_USAGE_READ_ONLY)) {
		r_error = vformat("Property `%s` on `%s` is read-only.", stripped_property_path, p_object_label);
		return false;
	}

	PropertyInfo indexed_property_info;
	const bool indexed_property_found = listed_property ? true : _find_indexed_property_info(p_object, property_names, indexed_property_info);
	if (listed_property) {
		indexed_property_info = listed_property_info;
	}
	if (indexed_property_found && (indexed_property_info.usage & PROPERTY_USAGE_READ_ONLY)) {
		r_error = vformat("Property `%s` on `%s` is read-only.", stripped_property_path, p_object_label);
		return false;
	}

	Variant::Type target_type = indexed_property_found ? indexed_property_info.type : r_current_value.get_type();
	if (target_type == Variant::NIL) {
		target_type = r_current_value.get_type();
	}
	if (!_convert_value_for_property(p_value, indexed_property_found ? &indexed_property_info : nullptr, target_type, r_converted_value, r_error)) {
		r_error = vformat("Property `%s` on `%s`: %s", stripped_property_path, p_object_label, r_error);
		return false;
	}

	r_property_type = Variant::get_type_name(target_type);
	return true;
}

bool AISceneEditingService::_apply_properties_direct(Node *p_node, const String &p_node_label, const Dictionary &p_properties, Array &r_applied_properties, String &r_error, String *r_failed_property_path) const {
	ERR_FAIL_NULL_V(p_node, false);

	for (const KeyValue<Variant, Variant> &E : p_properties) {
		const String property_path = String(E.key).strip_edges();
		if (r_failed_property_path) {
			*r_failed_property_path = property_path;
		}
		NodePath indexed_path;
		Variant current_value;
		Variant converted_value;
		String property_type;
		if (!_prepare_property_assignment(p_node, p_node_label, property_path, E.value, indexed_path, current_value, converted_value, property_type, r_error)) {
			return false;
		}

		bool valid = false;
		p_node->set_indexed(indexed_path.get_subnames(), converted_value, &valid);
		if (!valid) {
			r_error = vformat("Godot rejected property `%s` on `%s`.", property_path, p_node_label);
			return false;
		}

		Dictionary applied;
		applied["node"] = p_node_label;
		applied["property_path"] = property_path;
		applied["property_type"] = property_type;
		r_applied_properties.push_back(applied);
	}

	if (r_failed_property_path) {
		r_failed_property_path->clear();
	}
	return true;
}

bool AISceneEditingService::_queue_properties_for_undo(EditorUndoRedoManager *p_undo_redo, Node *p_node, const String &p_node_label, const Dictionary &p_properties, Array &r_applied_properties, String &r_error, String *r_failed_property_path) const {
	ERR_FAIL_NULL_V(p_undo_redo, false);
	ERR_FAIL_NULL_V(p_node, false);

	for (const KeyValue<Variant, Variant> &E : p_properties) {
		const String property_path = String(E.key).strip_edges();
		if (r_failed_property_path) {
			*r_failed_property_path = property_path;
		}
		NodePath indexed_path;
		Variant current_value;
		Variant converted_value;
		String property_type;
		if (!_prepare_property_assignment(p_node, p_node_label, property_path, E.value, indexed_path, current_value, converted_value, property_type, r_error)) {
			return false;
		}

		p_undo_redo->add_do_method(p_node, "set_indexed", indexed_path, converted_value);
		p_undo_redo->add_undo_method(p_node, "set_indexed", indexed_path, current_value);

		Dictionary applied;
		applied["node"] = p_node_label;
		applied["property_path"] = property_path;
		applied["property_type"] = property_type;
		r_applied_properties.push_back(applied);
	}

	if (r_failed_property_path) {
		r_failed_property_path->clear();
	}
	return true;
}

bool AISceneEditingService::_set_indexed_property(Object *p_object, const String &p_property_path, const Variant &p_value, String &r_error) const {
	ERR_FAIL_NULL_V(p_object, false);

	Vector<StringName> property_names;
	if (!_normalize_property_path(p_property_path, property_names, r_error)) {
		return false;
	}

	bool valid = false;
	p_object->set_indexed(property_names, p_value, &valid);
	if (!valid) {
		r_error = "Godot rejected the property assignment.";
		return false;
	}

	return true;
}

bool AISceneEditingService::_node_tree_contains_scene_path(Node *p_node, const String &p_scene_path) const {
	ERR_FAIL_NULL_V(p_node, false);

	const String target_scene_path = p_scene_path.strip_edges().simplify_path();
	String node_scene_path = p_node->get_scene_file_path().strip_edges();
	if (!node_scene_path.is_empty()) {
		node_scene_path = ProjectSettings::get_singleton()->localize_path(node_scene_path).simplify_path();
		if (node_scene_path == target_scene_path) {
			return true;
		}
	}

	for (int i = 0; i < p_node->get_child_count(false); i++) {
		if (_node_tree_contains_scene_path(p_node->get_child(i, false), target_scene_path)) {
			return true;
		}
	}
	return false;
}

bool AISceneEditingService::_validate_patch_id(const String &p_id, String &r_error) const {
	if (p_id.is_empty()) {
		return true;
	}
	for (int i = 0; i < p_id.length(); i++) {
		const char32_t c = p_id[i];
		const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
		if (!allowed) {
			r_error = vformat("Patch id `%s` contains an invalid character. Use only letters, numbers, `_`, `-`, or `.`.", p_id);
			return false;
		}
	}
	return true;
}

Node *AISceneEditingService::_resolve_patch_node(Node *p_scene_root, const String &p_locator, const HashMap<String, Node *> &p_created_nodes, bool p_allow_root, String &r_error) const {
	const String locator = p_locator.strip_edges();
	if (locator.is_empty()) {
		r_error = "Node locator is required.";
		return nullptr;
	}

	Node *const *created_node = p_created_nodes.getptr(locator);
	if (created_node && *created_node) {
		if (*created_node == p_scene_root && !p_allow_root) {
			r_error = "The scene root cannot be used for this operation.";
			return nullptr;
		}
		return const_cast<Node *>(*created_node);
	}

	return _resolve_node_path(p_scene_root, locator, p_allow_root, r_error);
}

bool AISceneEditingService::_describe_node_tree(Node *p_scene_root, Node *p_node, int p_depth, int p_max_depth, int p_max_nodes, bool p_include_internal, Array &r_nodes, String &r_content) const {
	ERR_FAIL_NULL_V(p_scene_root, false);
	ERR_FAIL_NULL_V(p_node, false);
	if (r_nodes.size() >= p_max_nodes) {
		return false;
	}

	const String path = p_node == p_scene_root ? String(".") : String(p_scene_root->get_path_to(p_node));
	Dictionary node_data;
	node_data["path"] = path;
	node_data["name"] = String(p_node->get_name());
	node_data["type"] = p_node->get_class();
	node_data["child_count"] = p_node->get_child_count(p_include_internal);
	if (!p_node->get_scene_file_path().is_empty()) {
		node_data["scene_file_path"] = ProjectSettings::get_singleton()->localize_path(p_node->get_scene_file_path());
	}
	if (p_node->has_meta(SNAME("ai_node_id"))) {
		node_data["ai_node_id"] = String(p_node->get_meta(SNAME("ai_node_id")));
	}
	r_nodes.push_back(node_data);

	String indent;
	for (int i = 0; i < p_depth; i++) {
		indent += "  ";
	}
	r_content += vformat("%s- %s: %s (%s)", indent, path, String(p_node->get_name()), p_node->get_class());
	if (node_data.has("ai_node_id")) {
		r_content += vformat(" ai_node_id=%s", String(node_data["ai_node_id"]));
	}
	r_content += "\n";

	if (p_depth >= p_max_depth) {
		return r_nodes.size() < p_max_nodes;
	}

	for (int i = 0; i < p_node->get_child_count(p_include_internal); i++) {
		Node *child = p_node->get_child(i, p_include_internal);
		if (!_describe_node_tree(p_scene_root, child, p_depth + 1, p_max_depth, p_max_nodes, p_include_internal, r_nodes, r_content)) {
			return false;
		}
	}
	return true;
}

String AISceneEditingService::_preview_variant_value(const Variant &p_value) const {
	if (p_value.get_type() == Variant::NIL) {
		return "<null>";
	}

	String text = p_value.stringify();
	text = text.replace("\n", "\\n");
	if (text.length() > 160) {
		text = text.substr(0, 157) + "...";
	}
	return text;
}

String AISceneEditingService::_format_property_not_found_error(Object *p_object, const String &p_object_label, const String &p_property_path, const Vector<StringName> &p_property_names) const {
	ERR_FAIL_NULL_V(p_object, String());
	Object *current_object = p_object;
	String prefix;
	for (int i = 0; i < p_property_names.size() - 1; i++) {
		const String segment = String(p_property_names[i]);
		prefix = prefix.is_empty() ? segment : prefix + ":" + segment;

		List<PropertyInfo> property_list;
		current_object->get_property_list(&property_list);

		bool found = false;
		PropertyInfo found_info;
		for (const PropertyInfo &E : property_list) {
			if (StringName(E.name) == p_property_names[i]) {
				found = true;
				found_info = E;
				break;
			}
		}
		if (!found) {
			break;
		}

		bool valid = false;
		Variant current_value = current_object->get(StringName(found_info.name), &valid);
		if (!valid) {
			break;
		}

		Object *next_object = current_value;
		if (!next_object) {
			if (found_info.type == Variant::OBJECT && found_info.hint == PROPERTY_HINT_RESOURCE_TYPE) {
				return vformat("Property `%s` targets a subproperty of Resource property `%s`, but `%s` on `%s` is currently null. Set `%s` to a Resource dictionary first, for example `%s`; include nested values under `properties` when creating the Resource.", p_property_path, prefix, prefix, p_object_label, prefix, _build_resource_value_example(found_info));
			}
			return vformat("Property `%s` targets subproperty `%s`, but intermediate property `%s` on `%s` is null or not an Object.", p_property_path, String(p_property_names[i + 1]), prefix, p_object_label);
		}

		current_object = next_object;
	}

	return vformat("Property `%s` was not found on `%s` (type `%s`).%s", p_property_path, p_object_label, p_object->get_class(), AIDocumentationService::format_property_suggestions_for_error(p_object, p_object_label, p_property_path));
}

bool AISceneEditingService::_inspect_property_value(Node *p_node, const String &p_node_label, const String &p_property_path, Dictionary &r_property, String &r_error) const {
	ERR_FAIL_NULL_V(p_node, false);
	const String stripped_property_path = p_property_path.strip_edges();
	if (stripped_property_path.is_empty()) {
		r_error = "Property path is required.";
		return false;
	}

	Vector<StringName> property_names;
	if (!_normalize_property_path(stripped_property_path, property_names, r_error)) {
		return false;
	}

	bool valid = false;
	Variant current_value = p_node->get_indexed(property_names, &valid);
	if (!valid) {
		r_error = _format_property_not_found_error(p_node, p_node_label, stripped_property_path, property_names);
		return false;
	}

	PropertyInfo property_info;
	const bool found_property_info = _find_indexed_property_info(p_node, property_names, property_info);

	r_property["property_path"] = stripped_property_path;
	r_property["value"] = _preview_variant_value(current_value);
	r_property["value_type"] = Variant::get_type_name(current_value.get_type());
	if (found_property_info) {
		r_property["declared_type"] = Variant::get_type_name(property_info.type);
		r_property["hint"] = int(property_info.hint);
		r_property["hint_text"] = property_info.hint_string;
		r_property["class_name"] = String(property_info.class_name);
		r_property["read_only"] = bool(property_info.usage & PROPERTY_USAGE_READ_ONLY);
		r_property["subproperty_paths"] = _get_common_subproperty_paths(stripped_property_path, property_info.type);
	} else {
		r_property["declared_type"] = Variant::get_type_name(current_value.get_type());
		r_property["read_only"] = false;
		r_property["subproperty_paths"] = _get_common_subproperty_paths(stripped_property_path, current_value.get_type());
	}

	Ref<Resource> resource = current_value;
	if (resource.is_valid()) {
		r_property["resource_type"] = resource->get_class();
		r_property["resource_path"] = resource->get_path();
	}
	return true;
}

Array AISceneEditingService::_get_default_inspection_property_paths(Node *p_node) const {
	Array paths;
	ERR_FAIL_NULL_V(p_node, paths);

	paths.push_back("name");
	paths.push_back("process_mode");

	if (Object::cast_to<Control>(p_node)) {
		paths.push_back("visible");
		paths.push_back("position");
		paths.push_back("size");
		paths.push_back("rotation");
		paths.push_back("scale");
		paths.push_back("pivot_offset");
		paths.push_back("anchor_left");
		paths.push_back("anchor_top");
		paths.push_back("anchor_right");
		paths.push_back("anchor_bottom");
		paths.push_back("offset_left");
		paths.push_back("offset_top");
		paths.push_back("offset_right");
		paths.push_back("offset_bottom");
		paths.push_back("size_flags_horizontal");
		paths.push_back("size_flags_vertical");
		paths.push_back("text");
	} else if (Object::cast_to<Node2D>(p_node)) {
		paths.push_back("visible");
		paths.push_back("position");
		paths.push_back("rotation");
		paths.push_back("scale");
		paths.push_back("z_index");
	} else if (Object::cast_to<Node3D>(p_node)) {
		paths.push_back("visible");
		paths.push_back("position");
		paths.push_back("rotation");
		paths.push_back("scale");
	}
	return paths;
}

Array AISceneEditingService::_get_common_subproperty_paths(const String &p_property_path, Variant::Type p_type) const {
	Array paths;
	switch (p_type) {
		case Variant::VECTOR2:
		case Variant::VECTOR2I:
			paths.push_back(p_property_path + ":x");
			paths.push_back(p_property_path + ":y");
			break;
		case Variant::VECTOR3:
		case Variant::VECTOR3I:
			paths.push_back(p_property_path + ":x");
			paths.push_back(p_property_path + ":y");
			paths.push_back(p_property_path + ":z");
			break;
		case Variant::VECTOR4:
		case Variant::VECTOR4I:
			paths.push_back(p_property_path + ":x");
			paths.push_back(p_property_path + ":y");
			paths.push_back(p_property_path + ":z");
			paths.push_back(p_property_path + ":w");
			break;
		case Variant::RECT2:
		case Variant::RECT2I:
			paths.push_back(p_property_path + ":position");
			paths.push_back(p_property_path + ":position:x");
			paths.push_back(p_property_path + ":position:y");
			paths.push_back(p_property_path + ":size");
			paths.push_back(p_property_path + ":size:x");
			paths.push_back(p_property_path + ":size:y");
			break;
		case Variant::COLOR:
			paths.push_back(p_property_path + ":r");
			paths.push_back(p_property_path + ":g");
			paths.push_back(p_property_path + ":b");
			paths.push_back(p_property_path + ":a");
			break;
		case Variant::TRANSFORM2D:
			paths.push_back(p_property_path + ":x");
			paths.push_back(p_property_path + ":y");
			paths.push_back(p_property_path + ":origin");
			paths.push_back(p_property_path + ":origin:x");
			paths.push_back(p_property_path + ":origin:y");
			break;
		case Variant::TRANSFORM3D:
			paths.push_back(p_property_path + ":basis");
			paths.push_back(p_property_path + ":origin");
			paths.push_back(p_property_path + ":origin:x");
			paths.push_back(p_property_path + ":origin:y");
			paths.push_back(p_property_path + ":origin:z");
			break;
		case Variant::AABB:
			paths.push_back(p_property_path + ":position");
			paths.push_back(p_property_path + ":size");
			break;
		default:
			break;
	}
	return paths;
}

String AISceneEditingService::_build_resource_value_example(const PropertyInfo &p_property_info) const {
	Array allowed_types = _get_resource_hint_types(p_property_info.hint_string);
	String example_type = allowed_types.is_empty() ? String("Resource") : String(allowed_types[0]);

	if (example_type == "Shape2D" || example_type == "RectangleShape2D" || ClassDB::is_parent_class("RectangleShape2D", example_type)) {
		return "{\"resource_type\":\"RectangleShape2D\",\"properties\":{\"size\":{\"type\":\"Vector2\",\"args\":[64,32]}}}";
	}
	if (example_type == "Shape3D" || example_type == "BoxShape3D" || ClassDB::is_parent_class("BoxShape3D", example_type)) {
		return "{\"resource_type\":\"BoxShape3D\",\"properties\":{\"size\":{\"type\":\"Vector3\",\"args\":[1,1,1]}}}";
	}
	if (example_type == "Material" || example_type == "ShaderMaterial" || ClassDB::is_parent_class("ShaderMaterial", example_type)) {
		return "{\"resource_type\":\"ShaderMaterial\",\"properties\":{\"shader\":{\"resource_type\":\"Shader\",\"properties\":{\"code\":\"shader_type canvas_item;\\nvoid fragment(){ COLOR = vec4(1.0); }\"}}}}";
	}
	if (example_type == "Shader") {
		return "{\"resource_type\":\"Shader\",\"properties\":{\"code\":\"shader_type canvas_item;\\nvoid fragment(){ COLOR = vec4(1.0); }\"}}";
	}

	return vformat("{\"resource_type\":\"%s\",\"properties\":{}} or {\"resource_path\":\"res://path/to/resource.tres\"}", example_type);
}

void AISceneEditingService::_select_node(Node *p_node) const {
	SceneTreeDock *dock = SceneTreeDock::get_singleton();
	if (!dock) {
		return;
	}

	if (p_node) {
		dock->set_selection(Vector<Node *>{ p_node });
		dock->set_selected(p_node);
	} else {
		dock->set_selected(nullptr);
	}
}

AISceneEditingResult AISceneEditingService::_delete_node_main_thread(const String &p_node_path) {
	AISceneEditingResult result;
	String error;
	Node *scene = _get_edited_scene(error);
	if (!scene) {
		result.error = error;
		return result;
	}
	String current_scene_path;
	if (!_get_current_scene_save_path_main_thread(scene, current_scene_path, error)) {
		result.error = error;
		return result;
	}

	Node *node = _resolve_node_path(scene, p_node_path, false, error);
	if (!node) {
		result.error = error;
		return result;
	}
	if (!node->get_parent()) {
		result.error = "Node has no parent.";
		return result;
	}
	if (node->is_internal()) {
		result.error = "Internal nodes cannot be deleted by the AI scene tool.";
		return result;
	}

	Node *parent = node->get_parent();
	const int index = node->get_index(false);
	const NodePath original_path = scene->get_path_to(node);
	const NodePath parent_path = scene->get_path_to(parent);
	const String node_name = node->get_name();
	const String node_type = node->get_class();

	List<Node *> owned;
	node->get_owned_by(node->get_owner(), &owned);
	Array owners;
	for (Node *owned_node : owned) {
		owners.push_back(owned_node);
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (!undo_redo) {
		result.error = "Editor undo/redo manager is not available.";
		return result;
	}

	if (EditorNode::get_singleton()) {
		EditorNode::get_singleton()->hide_unused_editors(SceneTreeDock::get_singleton());
	}

	undo_redo->create_action_for_history(TTR("AI Remove Node"), EditorNode::get_editor_data().get_current_edited_scene_history_id(), UndoRedo::MERGE_DISABLE, false, true);
	undo_redo->add_do_method(parent, "remove_child", node);
	undo_redo->add_undo_method(parent, "add_child", node, true);
	undo_redo->add_undo_method(parent, "move_child", node, index);
	SceneTreeDock *dock = SceneTreeDock::get_singleton();
	if (dock) {
		undo_redo->add_undo_method(dock, "_set_owners", scene, owners);
	}
	undo_redo->add_undo_reference(node);

	EditorDebuggerNode *debugger = EditorDebuggerNode::get_singleton();
	if (debugger) {
		undo_redo->add_do_method(debugger, "live_debug_remove_and_keep_node", original_path, node->get_instance_id());
		undo_redo->add_undo_method(debugger, "live_debug_restore_node", node->get_instance_id(), parent_path, index);
	}
	undo_redo->commit_action();

	_select_node(parent);
	_update_scene_tree();

	String saved_path;
	if (!_save_current_scene_main_thread(scene, saved_path, error)) {
		undo_redo->undo();
		_select_node(node);
		_update_scene_tree();
		result.error = error;
		result.metadata["saved"] = false;
		result.metadata["node_path"] = p_node_path;
		result.metadata["node_name"] = node_name;
		result.metadata["node_type"] = node_type;
		result.metadata["parent_path"] = String(parent_path);
		return result;
	}

	result.success = true;
	result.message = vformat("Deleted node `%s` and saved `%s`.", p_node_path, saved_path);
	result.metadata["node_path"] = p_node_path;
	result.metadata["node_name"] = node_name;
	result.metadata["node_type"] = node_type;
	result.metadata["parent_path"] = String(parent_path);
	result.metadata["scene_path"] = saved_path;
	result.metadata["saved"] = true;
	return result;
}

AISceneEditingResult AISceneEditingService::_list_properties_main_thread(const String &p_node_path, const String &p_filter, int p_max_properties, bool p_include_read_only, bool p_include_current_values) {
	AISceneEditingResult result;
	String error;
	Node *scene = _get_edited_scene(error);
	if (!scene) {
		result.error = error;
		return result;
	}

	Node *node = _resolve_node_path(scene, p_node_path, true, error);
	if (!node) {
		result.error = error;
		return result;
	}

	const String filter = p_filter.strip_edges().to_lower();
	const int max_properties = CLAMP(p_max_properties, 1, 300);

	List<PropertyInfo> property_list;
	node->get_property_list(&property_list);

	Array properties;
	int matched_count = 0;
	int omitted_count = 0;
	for (const PropertyInfo &E : property_list) {
		if (E.name.is_empty()) {
			continue;
		}
		if (E.usage & (PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_INTERNAL)) {
			continue;
		}
		if (!(E.usage & (PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE))) {
			continue;
		}
		const bool read_only = E.usage & PROPERTY_USAGE_READ_ONLY;
		if (read_only && !p_include_read_only) {
			continue;
		}
		const String property_name = String(E.name);
		if (!filter.is_empty() && !property_name.to_lower().contains(filter) && !Variant::get_type_name(E.type).to_lower().contains(filter)) {
			continue;
		}

		matched_count++;
		if (properties.size() >= max_properties) {
			omitted_count++;
			continue;
		}

		Dictionary property;
		property["property_path"] = property_name;
		property["type"] = Variant::get_type_name(E.type);
		property["hint"] = int(E.hint);
		property["hint_text"] = E.hint_string;
		property["class_name"] = String(E.class_name);
		property["read_only"] = read_only;
		property["subproperty_paths"] = _get_common_subproperty_paths(property_name, E.type);
		if (E.type == Variant::OBJECT && E.hint == PROPERTY_HINT_RESOURCE_TYPE) {
			property["is_resource"] = true;
			property["expected_resource_types"] = _get_resource_hint_types(E.hint_string);
			property["resource_value_example"] = _build_resource_value_example(E);
		}

		if (p_include_current_values) {
			bool valid = false;
			Variant current_value = node->get(StringName(property_name), &valid);
			if (valid) {
				property["current_value"] = _preview_variant_value(current_value);
				property["current_value_type"] = Variant::get_type_name(current_value.get_type());
				Ref<Resource> resource = current_value;
				if (resource.is_valid()) {
					property["current_resource_type"] = resource->get_class();

					List<PropertyInfo> resource_property_list;
					resource->get_property_list(&resource_property_list);
					Array resource_properties;
					for (const PropertyInfo &R : resource_property_list) {
						if (resource_properties.size() >= 40) {
							break;
						}
						if (R.name.is_empty()) {
							continue;
						}
						if (R.usage & (PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_READ_ONLY)) {
							continue;
						}
						if (!(R.usage & (PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE))) {
							continue;
						}

						const String nested_name = String(R.name);
						Dictionary nested_property;
						nested_property["property_path"] = property_name + ":" + nested_name;
						nested_property["resource_property_path"] = nested_name;
						nested_property["type"] = Variant::get_type_name(R.type);
						nested_property["hint"] = int(R.hint);
						nested_property["hint_text"] = R.hint_string;
						nested_property["class_name"] = String(R.class_name);
						nested_property["subproperty_paths"] = _get_common_subproperty_paths(property_name + ":" + nested_name, R.type);
						if (R.type == Variant::OBJECT && R.hint == PROPERTY_HINT_RESOURCE_TYPE) {
							nested_property["is_resource"] = true;
							nested_property["expected_resource_types"] = _get_resource_hint_types(R.hint_string);
							nested_property["resource_value_example"] = _build_resource_value_example(R);
						}
						resource_properties.push_back(nested_property);
					}
					property["resource_properties"] = resource_properties;
				}
			}
		}
		properties.push_back(property);
	}

	String content;
	content += vformat("Properties for node `%s` (%s)\n", p_node_path, node->get_class());
	content += "Use `property_path` exactly in scene.apply_patch set_property or set_properties operations. For subproperty edits, use one of the listed subproperty_paths when present.\n";
	content += "Resource properties accept null, {\"resource_path\":\"res://...\"}, or {\"resource_type\":\"Type\",\"properties\":{...}}. For CollisionShape2D/3D.shape and similar complex Resource properties, create or load the Resource first and put nested values under `properties`; paths like shape:size work only after shape already exists.\n";
	for (int i = 0; i < properties.size(); i++) {
		Dictionary property = properties[i];
		content += vformat("- %s: %s", String(property["property_path"]), String(property["type"]));
		if (bool(property["read_only"])) {
			content += " [read-only]";
		}
		if (property.has("current_value")) {
			content += " = " + String(property["current_value"]);
		}
		if (property.has("hint_text") && !String(property["hint_text"]).is_empty()) {
			content += " hint=" + String(property["hint_text"]);
		}
		if (property.has("expected_resource_types")) {
			Array expected_resource_types = property["expected_resource_types"];
			Vector<String> expected_texts;
			for (int j = 0; j < expected_resource_types.size(); j++) {
				expected_texts.push_back(String(expected_resource_types[j]));
			}
			if (!expected_texts.is_empty()) {
				content += " resource_types=" + String(", ").join(expected_texts);
			}
		}
		if (property.has("current_resource_type")) {
			content += " current_resource=" + String(property["current_resource_type"]);
		}
		Array subproperties = property["subproperty_paths"];
		if (!subproperties.is_empty()) {
			Vector<String> subproperty_texts;
			for (int j = 0; j < subproperties.size(); j++) {
				subproperty_texts.push_back(String(subproperties[j]));
			}
			content += " subpaths=" + String(", ").join(subproperty_texts);
		}
		if (property.has("resource_value_example")) {
			content += " example=" + String(property["resource_value_example"]);
		}
		content += "\n";

		if (property.has("resource_properties")) {
			Array resource_properties = property["resource_properties"];
			for (int j = 0; j < resource_properties.size(); j++) {
				Dictionary nested_property = resource_properties[j];
				content += vformat("  - %s: %s", String(nested_property["property_path"]), String(nested_property["type"]));
				if (nested_property.has("hint_text") && !String(nested_property["hint_text"]).is_empty()) {
					content += " hint=" + String(nested_property["hint_text"]);
				}
				Array nested_subproperties = nested_property["subproperty_paths"];
				if (!nested_subproperties.is_empty()) {
					Vector<String> nested_subproperty_texts;
					for (int k = 0; k < nested_subproperties.size(); k++) {
						nested_subproperty_texts.push_back(String(nested_subproperties[k]));
					}
					content += " subpaths=" + String(", ").join(nested_subproperty_texts);
				}
				if (nested_property.has("resource_value_example")) {
					content += " example=" + String(nested_property["resource_value_example"]);
				}
				content += "\n";
			}
		}
	}
	if (omitted_count > 0) {
		content += vformat("... %d more matching properties omitted. Use filter or increase max_properties.\n", omitted_count);
	}

	result.success = true;
	result.message = content;
	result.metadata["node_path"] = p_node_path;
	result.metadata["node_type"] = node->get_class();
	result.metadata["properties"] = properties;
	result.metadata["matched_count"] = matched_count;
	result.metadata["omitted_count"] = omitted_count;
	result.metadata["max_properties"] = max_properties;
	result.metadata["filter"] = filter;
	return result;
}

AISceneEditingResult AISceneEditingService::_inspect_node_main_thread(const String &p_node_path, const Array &p_property_paths) {
	AISceneEditingResult result;
	String error;
	Node *scene = _get_edited_scene(error);
	if (!scene) {
		result.error = error;
		return result;
	}

	Node *node = _resolve_node_path(scene, p_node_path, true, error);
	if (!node) {
		result.error = error;
		return result;
	}

	const bool explicit_properties = !p_property_paths.is_empty();
	Array requested_property_paths = explicit_properties ? p_property_paths : _get_default_inspection_property_paths(node);
	Array properties;
	HashSet<String> seen_properties;
	for (int i = 0; i < requested_property_paths.size(); i++) {
		if (Variant(requested_property_paths[i]).get_type() != Variant::STRING && Variant(requested_property_paths[i]).get_type() != Variant::STRING_NAME && Variant(requested_property_paths[i]).get_type() != Variant::NODE_PATH) {
			result.error = vformat("properties[%d] must be a property path string.", i);
			return result;
		}

		const String property_path = String(requested_property_paths[i]).strip_edges();
		if (property_path.is_empty()) {
			result.error = vformat("properties[%d] must not be empty.", i);
			return result;
		}
		if (seen_properties.has(property_path)) {
			continue;
		}
		seen_properties.insert(property_path);

		Dictionary property;
		if (!_inspect_property_value(node, p_node_path, property_path, property, error)) {
			if (explicit_properties) {
				result.error = vformat("properties[%d] `%s`: %s", i, property_path, error);
				return result;
			}
			continue;
		}
		properties.push_back(property);
	}

	String scene_path = scene->get_scene_file_path().is_empty() ? String("<unsaved>") : ProjectSettings::get_singleton()->localize_path(scene->get_scene_file_path());
	String node_path = node == scene ? String(".") : String(scene->get_path_to(node));
	String content;
	content += vformat("Inspection for node `%s` (%s) in `%s`\n", node_path, node->get_class(), scene_path);
	if (properties.is_empty()) {
		content += "No inspected properties were available. Use scene.list_properties to discover valid property paths.\n";
	} else {
		for (int i = 0; i < properties.size(); i++) {
			Dictionary property = properties[i];
			content += vformat("- %s: %s = %s", String(property["property_path"]), String(property["value_type"]), String(property["value"]));
			if (property.has("declared_type") && String(property["declared_type"]) != String(property["value_type"])) {
				content += " declared=" + String(property["declared_type"]);
			}
			if (property.has("resource_type") && !String(property["resource_type"]).is_empty()) {
				content += " resource_type=" + String(property["resource_type"]);
			}
			content += "\n";
		}
	}

	result.success = true;
	result.message = content.strip_edges();
	result.metadata["scene_path"] = scene_path;
	result.metadata["node_path"] = node_path;
	result.metadata["node_name"] = String(node->get_name());
	result.metadata["node_type"] = node->get_class();
	result.metadata["properties"] = properties;
	result.metadata["property_count"] = properties.size();
	result.metadata["explicit_properties"] = explicit_properties;
	return result;
}

AISceneEditingResult AISceneEditingService::_describe_tree_main_thread(const String &p_root_path, int p_max_depth, int p_max_nodes, bool p_include_internal) {
	AISceneEditingResult result;
	String error;
	Node *scene = _get_edited_scene(error);
	if (!scene) {
		result.error = error;
		return result;
	}

	Node *root = _resolve_node_path(scene, p_root_path.strip_edges().is_empty() ? String(".") : p_root_path, true, error);
	if (!root) {
		result.error = error;
		return result;
	}

	const int max_depth = CLAMP(p_max_depth, 0, 32);
	const int max_nodes = CLAMP(p_max_nodes, 1, 1000);
	Array nodes;
	String content;
	String scene_path = scene->get_scene_file_path().is_empty() ? String("<unsaved>") : ProjectSettings::get_singleton()->localize_path(scene->get_scene_file_path());
	content += vformat("Scene tree for `%s` from `%s`\n", scene_path, root == scene ? String(".") : String(scene->get_path_to(root)));
	const bool complete = _describe_node_tree(scene, root, 0, max_depth, max_nodes, p_include_internal, nodes, content);
	if (!complete) {
		content += vformat("... output truncated at %d nodes or depth %d.\n", max_nodes, max_depth);
	}

	result.success = true;
	result.message = content.strip_edges();
	result.metadata["scene_path"] = scene_path;
	result.metadata["root_path"] = root == scene ? String(".") : String(scene->get_path_to(root));
	result.metadata["nodes"] = nodes;
	result.metadata["node_count"] = nodes.size();
	result.metadata["max_depth"] = max_depth;
	result.metadata["max_nodes"] = max_nodes;
	result.metadata["truncated"] = !complete;
	return result;
}

AISceneEditingResult AISceneEditingService::_apply_patch_main_thread(const Dictionary &p_patch) {
	AISceneEditingResult result;
	Array applied_ops;
	Array property_changes;
	Array created_nodes_metadata;
	Vector<Node *> pending_nodes;
	HashMap<String, Node *> patch_nodes;
	String error;
	bool create_mode = false;
	bool scene_added_to_editor = false;
	EditorUndoRedoManager *undo_redo = nullptr;
	bool undo_action_started = false;
	String target_scene_path;

	auto cleanup_pending = [&]() {
		if (create_mode) {
			return;
		}
		if (undo_action_started && undo_redo) {
			undo_redo->cancel_action();
			undo_action_started = false;
			pending_nodes.clear();
			return;
		}
		for (int i = 0; i < pending_nodes.size(); i++) {
			if (pending_nodes[i]) {
				memdelete(pending_nodes[i]);
			}
		}
		pending_nodes.clear();
	};

	auto fail = [&](const String &p_error) -> AISceneEditingResult {
		cleanup_pending();
		result.error = p_error;
		result.metadata["applied_ops"] = applied_ops;
		result.metadata["property_changes"] = property_changes;
		result.metadata["created_nodes"] = created_nodes_metadata;
		return result;
	};

	auto format_property_failure = [&](int p_op_index, const String &p_op, const String &p_failed_property_path, const String &p_error) {
		if (p_op == "set_property") {
			return vformat("ops[%d].property: %s", p_op_index, p_error);
		}
		if (!p_failed_property_path.is_empty()) {
			return vformat("ops[%d].properties[\"%s\"]: %s", p_op_index, p_failed_property_path, p_error);
		}
		return vformat("ops[%d].properties: %s", p_op_index, p_error);
	};

	if (p_patch.is_empty()) {
		return fail("Patch object is required.");
	}

	Node *scene = nullptr;
	if (p_patch.has("create_scene")) {
		if (Variant(p_patch["create_scene"]).get_type() != Variant::DICTIONARY) {
			return fail("create_scene must be an object.");
		}
		Dictionary create_scene = p_patch["create_scene"];
		const String root_type = String(create_scene.get("root_type", "")).strip_edges();
		const String root_name = String(create_scene.get("root_name", "")).strip_edges();
		const String requested_path = String(create_scene.get("path", "")).strip_edges();
		if (root_type.is_empty()) {
			return fail("create_scene.root_type is required.");
		}
		if (!_normalize_scene_save_path(requested_path, target_scene_path, error)) {
			return fail("create_scene.path: " + error);
		}
		if (FileAccess::exists(target_scene_path)) {
			return fail(vformat("create_scene.path: Scene file `%s` already exists.", target_scene_path));
		}
		if (!_ensure_scene_save_directory(target_scene_path, error)) {
			return fail("create_scene.path: " + error);
		}

		scene = _instantiate_node(root_type, error);
		if (!scene) {
			return fail("create_scene.root_type: " + error);
		}
		create_mode = true;
		const String normalized_root_name = _normalize_node_name(nullptr, scene, root_name);
		patch_nodes.insert("root", scene);

		const String root_id = String(create_scene.get("id", "")).strip_edges();
		if (!_validate_patch_id(root_id, error)) {
			memdelete(scene);
			scene = nullptr;
			return fail("create_scene.id: " + error);
		}
		if (!root_id.is_empty()) {
			if (patch_nodes.has(root_id)) {
				memdelete(scene);
				scene = nullptr;
				return fail(vformat("create_scene.id: Duplicate patch id `%s`.", root_id));
			}
			patch_nodes.insert(root_id, scene);
			scene->set_meta(SNAME("ai_node_id"), root_id);
		}

		if (create_scene.has("properties")) {
			if (Variant(create_scene["properties"]).get_type() != Variant::DICTIONARY) {
				memdelete(scene);
				scene = nullptr;
				return fail("create_scene.properties must be an object.");
			}
			Dictionary root_properties = create_scene["properties"];
			String failed_property_path;
			if (!_apply_properties_direct(scene, ".", root_properties, property_changes, error, &failed_property_path)) {
				memdelete(scene);
				scene = nullptr;
				if (!failed_property_path.is_empty()) {
					return fail(vformat("create_scene.properties[\"%s\"]: %s", failed_property_path, error));
				}
				return fail("create_scene.properties: " + error);
			}
		}

		Dictionary root_info;
		root_info["id"] = root_id.is_empty() ? String("root") : root_id;
		root_info["node_path"] = ".";
		root_info["node_name"] = normalized_root_name;
		root_info["node_type"] = scene->get_class();
		created_nodes_metadata.push_back(root_info);
	} else {
		scene = _get_edited_scene(error);
		if (!scene) {
			return fail(error);
		}
		if (!_get_current_scene_save_path_main_thread(scene, target_scene_path, error)) {
			return fail(error);
		}
		patch_nodes.insert("root", scene);
	}

	if (!p_patch.has("ops")) {
		if (!create_mode) {
			return fail("ops array is required.");
		}
	} else if (Variant(p_patch["ops"]).get_type() != Variant::ARRAY) {
		if (create_mode && scene && !scene_added_to_editor) {
			memdelete(scene);
			scene = nullptr;
		}
		return fail("ops must be an array.");
	}

	Array ops = p_patch.get("ops", Array());
	if (ops.size() > 300) {
		if (create_mode && scene && !scene_added_to_editor) {
			memdelete(scene);
			scene = nullptr;
		}
		return fail("ops contains too many operations. Maximum is 300.");
	}

	if (!create_mode) {
		undo_redo = EditorUndoRedoManager::get_singleton();
		if (!undo_redo) {
			return fail("Editor undo/redo manager is not available.");
		}
		undo_redo->create_action_for_history(TTR("AI Apply Scene Patch"), EditorNode::get_editor_data().get_current_edited_scene_history_id(), UndoRedo::MERGE_DISABLE, false, true);
		undo_action_started = true;
	}

	Node *last_selected_node = scene;
	for (int i = 0; i < ops.size(); i++) {
		if (Variant(ops[i]).get_type() != Variant::DICTIONARY) {
			if (create_mode && scene && !scene_added_to_editor) {
				memdelete(scene);
				scene = nullptr;
			}
			return fail(vformat("ops[%d] must be an object.", i));
		}
		Dictionary op_dict = ops[i];
		const String op = String(op_dict.get("op", "")).strip_edges();
		if (op.is_empty()) {
			if (create_mode && scene && !scene_added_to_editor) {
				memdelete(scene);
				scene = nullptr;
			}
			return fail(vformat("ops[%d].op is required.", i));
		}

		Dictionary op_result;
		op_result["op"] = op;
		op_result["index"] = i;

		if (op == "add_node" || op == "instantiate_scene") {
			const String parent_locator = String(op_dict.get("parent", "")).strip_edges();
			if (parent_locator.is_empty()) {
				if (create_mode && scene && !scene_added_to_editor) {
					memdelete(scene);
					scene = nullptr;
				}
				return fail(vformat("ops[%d].parent is required.", i));
			}
			Node *parent = _resolve_patch_node(scene, parent_locator, patch_nodes, true, error);
			if (!parent) {
				if (create_mode && scene && !scene_added_to_editor) {
					memdelete(scene);
					scene = nullptr;
				}
				return fail(vformat("ops[%d].parent: %s", i, error));
			}
			const int position = int(op_dict.get("position", -1));
			if (position < -1) {
				if (create_mode && scene && !scene_added_to_editor) {
					memdelete(scene);
					scene = nullptr;
				}
				return fail(vformat("ops[%d].position must be -1 or a non-negative child index.", i));
			}
			if (position > parent->get_child_count(false)) {
				if (create_mode && scene && !scene_added_to_editor) {
					memdelete(scene);
					scene = nullptr;
				}
				return fail(vformat("ops[%d].position is outside the parent child range.", i));
			}

			Node *child = nullptr;
			String source_scene_path;
			if (op == "add_node") {
				const String node_type = String(op_dict.get("type", "")).strip_edges();
				if (node_type.is_empty()) {
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(vformat("ops[%d].type is required.", i));
				}
				child = _instantiate_node(node_type, error);
				if (!child) {
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(vformat("ops[%d].type: %s", i, error));
				}
			} else {
				const String requested_scene_path = String(op_dict.get("scene_path", "")).strip_edges();
				if (!_normalize_scene_save_path(requested_scene_path, source_scene_path, error)) {
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(vformat("ops[%d].scene_path: %s", i, error));
				}
				if (!target_scene_path.is_empty() && source_scene_path == target_scene_path) {
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(vformat("ops[%d].scene_path: Cannot instantiate the target scene into itself.", i));
				}
				if (!ResourceLoader::exists(source_scene_path, "PackedScene")) {
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(vformat("ops[%d].scene_path: Scene resource `%s` does not exist.", i, source_scene_path));
				}
				const String resource_type = ResourceLoader::get_resource_type(source_scene_path);
				if (resource_type != "PackedScene" && !ClassDB::is_parent_class(resource_type, SNAME("PackedScene"))) {
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(vformat("ops[%d].scene_path: Resource `%s` is not a PackedScene.", i, source_scene_path));
				}
				Error load_error = OK;
				Ref<PackedScene> packed_scene = ResourceLoader::load(source_scene_path, "PackedScene", ResourceLoader::CACHE_MODE_REUSE, &load_error);
				if (packed_scene.is_null()) {
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(vformat("ops[%d].scene_path: Failed to load scene `%s` (error %d).", i, source_scene_path, load_error));
				}
				child = packed_scene->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
				if (!child) {
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(vformat("ops[%d].scene_path: Failed to instantiate scene `%s`.", i, source_scene_path));
				}
				child->set_scene_file_path(ProjectSettings::get_singleton()->localize_path(source_scene_path));
				if (_node_tree_contains_scene_path(child, target_scene_path)) {
					memdelete(child);
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(vformat("ops[%d].scene_path: Cannot instantiate scene `%s` because it contains the target scene.", i, source_scene_path));
				}
			}

			const String new_name = _normalize_node_name(parent, child, String(op_dict.get("name", "")).strip_edges());
			const String id = String(op_dict.get("id", "")).strip_edges();
			if (!_validate_patch_id(id, error)) {
				memdelete(child);
				if (create_mode && scene && !scene_added_to_editor) {
					memdelete(scene);
					scene = nullptr;
				}
				return fail(vformat("ops[%d].id: %s", i, error));
			}
			if (!id.is_empty()) {
				if (patch_nodes.has(id)) {
					memdelete(child);
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(vformat("ops[%d].id: Duplicate patch id `%s`.", i, id));
				}
				patch_nodes.insert(id, child);
				child->set_meta(SNAME("ai_node_id"), id);
			}

			if (op_dict.has("properties")) {
				if (Variant(op_dict["properties"]).get_type() != Variant::DICTIONARY) {
					memdelete(child);
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(vformat("ops[%d].properties must be an object.", i));
				}
				Dictionary properties = op_dict["properties"];
				String failed_property_path;
				if (!_apply_properties_direct(child, id.is_empty() ? new_name : id, properties, property_changes, error, &failed_property_path)) {
					memdelete(child);
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(format_property_failure(i, op, failed_property_path, error));
				}
			}

			if (create_mode) {
				parent->add_child(child, true);
				if (position >= 0) {
					parent->move_child(child, position);
				}
				child->set_owner(scene);
			} else {
				const NodePath parent_path = scene->get_path_to(parent);
				const NodePath child_path = NodePath(String(parent_path).path_join(new_name));
				undo_redo->add_do_method(parent, "add_child", child, true);
				if (position >= 0) {
					undo_redo->add_do_method(parent, "move_child", child, position);
				}
				undo_redo->add_do_method(child, "set_owner", scene);
				undo_redo->add_do_reference(child);
				undo_redo->add_undo_method(parent, "remove_child", child);

				EditorDebuggerNode *debugger = EditorDebuggerNode::get_singleton();
				if (debugger) {
					if (op == "instantiate_scene") {
						undo_redo->add_do_method(debugger, "live_debug_instantiate_node", parent_path, source_scene_path, new_name);
					} else {
						undo_redo->add_do_method(debugger, "live_debug_create_node", parent_path, child->get_class(), new_name);
					}
					undo_redo->add_undo_method(debugger, "live_debug_remove_node", child_path);
				}
				pending_nodes.push_back(child);
			}

			Dictionary created;
			created["id"] = id;
			created["parent"] = parent_locator;
			created["node_name"] = new_name;
			created["node_type"] = child->get_class();
			if (!source_scene_path.is_empty()) {
				created["source_scene_path"] = source_scene_path;
			}
			created_nodes_metadata.push_back(created);
			op_result["node_name"] = new_name;
			op_result["node_type"] = child->get_class();
			last_selected_node = child;
		} else if (op == "set_property" || op == "set_properties") {
			const String node_locator = String(op_dict.get("node", "")).strip_edges();
			if (node_locator.is_empty()) {
				if (create_mode && scene && !scene_added_to_editor) {
					memdelete(scene);
					scene = nullptr;
				}
				return fail(vformat("ops[%d].node is required.", i));
			}
			Node *node = _resolve_patch_node(scene, node_locator, patch_nodes, true, error);
			if (!node) {
				if (create_mode && scene && !scene_added_to_editor) {
					memdelete(scene);
					scene = nullptr;
				}
				return fail(vformat("ops[%d].node: %s", i, error));
			}

			Dictionary properties;
			if (op == "set_property") {
				const String property_path = String(op_dict.get("property", "")).strip_edges();
				if (property_path.is_empty()) {
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(vformat("ops[%d].property is required.", i));
				}
				if (!op_dict.has("value")) {
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(vformat("ops[%d].value is required.", i));
				}
				properties[property_path] = op_dict["value"];
			} else {
				if (!op_dict.has("properties") || Variant(op_dict["properties"]).get_type() != Variant::DICTIONARY) {
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(vformat("ops[%d].properties must be an object.", i));
				}
				properties = op_dict["properties"];
			}

			if (create_mode || (patch_nodes.has(node_locator) && node != scene)) {
				String failed_property_path;
				if (!_apply_properties_direct(node, node_locator, properties, property_changes, error, &failed_property_path)) {
					if (create_mode && scene && !scene_added_to_editor) {
						memdelete(scene);
						scene = nullptr;
					}
					return fail(format_property_failure(i, op, failed_property_path, error));
				}
			} else {
				String failed_property_path;
				if (!_queue_properties_for_undo(undo_redo, node, node_locator, properties, property_changes, error, &failed_property_path)) {
					return fail(format_property_failure(i, op, failed_property_path, error));
				}
			}
			last_selected_node = node;
		} else if (op == "rename_node") {
			const String node_locator = String(op_dict.get("node", "")).strip_edges();
			const String requested_name = String(op_dict.get("new_name", "")).strip_edges();
			if (node_locator.is_empty() || requested_name.is_empty()) {
				if (create_mode && scene && !scene_added_to_editor) {
					memdelete(scene);
					scene = nullptr;
				}
				return fail(vformat("ops[%d] requires node and new_name.", i));
			}
			Node *node = _resolve_patch_node(scene, node_locator, patch_nodes, false, error);
			if (!node) {
				if (create_mode && scene && !scene_added_to_editor) {
					memdelete(scene);
					scene = nullptr;
				}
				return fail(vformat("ops[%d].node: %s", i, error));
			}
			Node *parent = node->get_parent();
			if (!parent) {
				if (create_mode && scene && !scene_added_to_editor) {
					memdelete(scene);
					scene = nullptr;
				}
				return fail(vformat("ops[%d].node has no parent.", i));
			}
			String new_name = requested_name.validate_node_name().strip_edges();
			if (new_name.is_empty()) {
				if (create_mode && scene && !scene_added_to_editor) {
					memdelete(scene);
					scene = nullptr;
				}
				return fail(vformat("ops[%d].new_name is invalid.", i));
			}
			new_name = parent->prevalidate_child_name(node, new_name);
			const String old_name = node->get_name();
			if (create_mode || (patch_nodes.has(node_locator) && node != scene)) {
				node->set_name(new_name);
			} else {
				undo_redo->add_do_method(node, "set_name", new_name);
				undo_redo->add_undo_method(node, "set_name", old_name);
			}
			op_result["old_name"] = old_name;
			op_result["new_name"] = new_name;
			last_selected_node = node;
		} else if (op == "move_node") {
			if (create_mode) {
				if (scene && !scene_added_to_editor) {
					memdelete(scene);
					scene = nullptr;
				}
				return fail(vformat("ops[%d].op: move_node is only supported for existing current scenes. Set parent on add_node when creating a new scene.", i));
			}
			const String node_locator = String(op_dict.get("node", "")).strip_edges();
			const String parent_locator = String(op_dict.get("new_parent", "")).strip_edges();
			if (node_locator.is_empty() || parent_locator.is_empty()) {
				return fail(vformat("ops[%d] requires node and new_parent.", i));
			}
			if (patch_nodes.has(node_locator) || patch_nodes.has(parent_locator)) {
				return fail(vformat("ops[%d].op: move_node cannot target nodes created earlier in the same patch. Set parent on add_node instead.", i));
			}
			Node *node = _resolve_node_path(scene, node_locator, false, error);
			if (!node) {
				return fail(vformat("ops[%d].node: %s", i, error));
			}
			Node *new_parent = _resolve_node_path(scene, parent_locator, true, error);
			if (!new_parent) {
				return fail(vformat("ops[%d].new_parent: %s", i, error));
			}
			if (node == new_parent || node->is_ancestor_of(new_parent)) {
				return fail(vformat("ops[%d].new_parent: A node cannot be moved under itself or one of its descendants.", i));
			}
			Node *old_parent = node->get_parent();
			if (!old_parent) {
				return fail(vformat("ops[%d].node has no parent.", i));
			}
			const int old_index = node->get_index(false);
			const int target_position = int(op_dict.get("position", -1));
			if (target_position < -1) {
				return fail(vformat("ops[%d].position must be -1 or a non-negative child index.", i));
			}
			const int new_position = target_position < 0 ? new_parent->get_child_count(false) : CLAMP(target_position, 0, new_parent->get_child_count(false));
			const String old_name = node->get_name();
			const String new_name = new_parent->prevalidate_child_name(node, old_name);
			undo_redo->add_do_method(old_parent, "remove_child", node);
			undo_redo->add_do_method(new_parent, "add_child", node, true);
			undo_redo->add_do_method(new_parent, "move_child", node, new_position);
			undo_redo->add_do_method(node, "set_name", new_name);
			undo_redo->add_undo_method(new_parent, "remove_child", node);
			undo_redo->add_undo_method(old_parent, "add_child", node, true);
			undo_redo->add_undo_method(old_parent, "move_child", node, old_index);
			undo_redo->add_undo_method(node, "set_name", old_name);
			op_result["node"] = node_locator;
			op_result["new_parent"] = parent_locator;
			op_result["position"] = new_position;
			last_selected_node = node;
		} else {
			if (create_mode && scene && !scene_added_to_editor) {
				memdelete(scene);
				scene = nullptr;
			}
			return fail(vformat("ops[%d].op `%s` is not supported. Supported ops: add_node, instantiate_scene, set_property, set_properties, rename_node, move_node. Use scene.delete_node for deletion.", i, op));
		}

		applied_ops.push_back(op_result);
	}

	String saved_path;
	if (create_mode) {
		EditorNode *editor = EditorNode::get_singleton();
		SceneTreeDock *dock = SceneTreeDock::get_singleton();
		if (!editor || !dock) {
			if (scene && !scene_added_to_editor) {
				memdelete(scene);
				scene = nullptr;
			}
			return fail("Editor scene tree is not available.");
		}
		editor->new_scene();
		dock->add_root_node(scene);
		scene_added_to_editor = true;
		_select_node(last_selected_node);
		_update_scene_tree();
		if (!_save_scene_main_thread(scene, target_scene_path, saved_path, error)) {
			_rollback_new_scene_main_thread(scene);
			result.error = error;
			result.metadata["saved"] = false;
			result.metadata["applied_ops"] = applied_ops;
			result.metadata["property_changes"] = property_changes;
			result.metadata["created_nodes"] = created_nodes_metadata;
			result.metadata["scene_path"] = target_scene_path;
			return result;
		}
	} else {
		undo_redo->add_do_method(this, "_update_scene_tree");
		undo_redo->add_undo_method(this, "_update_scene_tree");
		undo_redo->commit_action();
		undo_action_started = false;
		pending_nodes.clear();
		_select_node(last_selected_node);
		_update_scene_tree();
		if (!_save_current_scene_main_thread(scene, saved_path, error)) {
			undo_redo->undo();
			_update_scene_tree();
			result.error = error;
			result.metadata["saved"] = false;
			result.metadata["applied_ops"] = applied_ops;
			result.metadata["property_changes"] = property_changes;
			result.metadata["created_nodes"] = created_nodes_metadata;
			result.metadata["scene_path"] = target_scene_path;
			return result;
		}
	}

	result.success = true;
	result.message = vformat("Applied scene patch with %d operation(s) and saved `%s`.", ops.size(), saved_path);
	result.metadata["scene_path"] = saved_path;
	result.metadata["saved"] = true;
	result.metadata["created_scene"] = create_mode;
	result.metadata["applied_ops"] = applied_ops;
	result.metadata["property_changes"] = property_changes;
	result.metadata["created_nodes"] = created_nodes_metadata;
	return result;
}

AISceneEditingResult AISceneEditingService::delete_node(const String &p_node_path) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_DELETE_NODE;
	request.node_path = p_node_path;
	return _dispatch_to_main_thread(request);
}

AISceneEditingResult AISceneEditingService::list_properties(const String &p_node_path, const String &p_filter, int p_max_properties, bool p_include_read_only, bool p_include_current_values) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_LIST_PROPERTIES;
	request.node_path = p_node_path;
	request.property_filter = p_filter;
	request.max_properties = p_max_properties;
	request.include_read_only = p_include_read_only;
	request.include_current_values = p_include_current_values;
	return _dispatch_to_main_thread(request);
}

AISceneEditingResult AISceneEditingService::inspect_node(const String &p_node_path, const Array &p_property_paths) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_INSPECT_NODE;
	request.node_path = p_node_path;
	request.property_paths = p_property_paths;
	return _dispatch_to_main_thread(request);
}

AISceneEditingResult AISceneEditingService::describe_tree(const String &p_root_path, int p_max_depth, int p_max_nodes, bool p_include_internal) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_DESCRIBE_TREE;
	request.root_path = p_root_path;
	request.max_depth = p_max_depth;
	request.max_nodes = p_max_nodes;
	request.include_internal = p_include_internal;
	return _dispatch_to_main_thread(request);
}

AISceneEditingResult AISceneEditingService::apply_patch(const Dictionary &p_patch) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_APPLY_PATCH;
	request.patch = p_patch;
	return _dispatch_to_main_thread(request);
}
