/**************************************************************************/
/*  ai_scene_editing_service.cpp                                          */
/**************************************************************************/

#include "ai_scene_editing_service.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/os/thread.h"
#include "core/variant/variant.h"

#include "editor/debugger/editor_debugger_node.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/editor_data.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/scene/scene_tree_editor.h"
#include "editor/scene/editor_scene_tabs.h"
#include "editor/settings/editor_settings.h"
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
	if (Thread::is_main_thread()) {
		_execute_request_ptr(&r_request);
		return r_request.result;
	}

	MutexLock lock(request_mutex);
	if (!MessageQueue::get_main_singleton()) {
		AISceneEditingResult result;
		result.error = "Main thread dispatch is not available.";
		return result;
	}

	AISceneEditingService *dispatcher = get_dispatcher_singleton();
	Variant request_ptr = reinterpret_cast<uint64_t>(&r_request);
	Error err = MessageQueue::get_main_singleton()->push_callable(callable_mp(dispatcher, &AISceneEditingService::_execute_request), request_ptr);
	if (err != OK) {
		AISceneEditingResult result;
		result.error = "Failed to schedule scene editing on the main thread.";
		return result;
	}

	r_request.done.wait();
	return r_request.result;
}

void AISceneEditingService::_execute_request(uint64_t p_request_ptr) {
	_execute_request_ptr(reinterpret_cast<MainThreadRequest *>(p_request_ptr));
}

void AISceneEditingService::_execute_request_ptr(MainThreadRequest *p_request) {
	ERR_FAIL_NULL(p_request);

	switch (p_request->operation) {
		case MainThreadRequest::OP_CREATE_SCENE:
			p_request->result = _create_scene_main_thread(p_request->root_type, p_request->root_name, p_request->scene_path);
			break;
		case MainThreadRequest::OP_ADD_NODE:
			p_request->result = _add_node_main_thread(p_request->parent_path, p_request->node_type, p_request->node_name);
			break;
		case MainThreadRequest::OP_DELETE_NODE:
			p_request->result = _delete_node_main_thread(p_request->node_path);
			break;
	}

	p_request->done.post();
}

Node *AISceneEditingService::_get_edited_scene(String &r_error) const {
	EditorNode *editor = EditorNode::get_singleton();
	if (!editor) {
		r_error = "EditorNode is not available.";
		return nullptr;
	}

	Node *scene = editor->get_edited_scene();
	if (!scene) {
		r_error = "No edited scene is currently open.";
		return nullptr;
	}
	return scene;
}

Node *AISceneEditingService::_resolve_node_path(Node *p_scene_root, const String &p_path, bool p_allow_root, String &r_error) const {
	ERR_FAIL_NULL_V(p_scene_root, nullptr);

	const String stripped_path = p_path.strip_edges();
	if (stripped_path.is_empty()) {
		r_error = "Node path is required.";
		return nullptr;
	}

	if (stripped_path == ".") {
		if (!p_allow_root) {
			r_error = "The scene root cannot be used for this operation.";
			return nullptr;
		}
		return p_scene_root;
	}

	if (stripped_path.begins_with("/") || stripped_path.contains("..")) {
		r_error = "Only paths relative to the edited scene root are allowed.";
		return nullptr;
	}
	if (stripped_path.contains(":")) {
		r_error = "Property subpaths are not allowed; provide a node path only.";
		return nullptr;
	}

	Node *node = p_scene_root->get_node_or_null(NodePath(stripped_path));
	if (!node) {
		r_error = "Node path was not found in the edited scene.";
		return nullptr;
	}
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

void AISceneEditingService::_update_scene_tree() const {
	SceneTreeDock *dock = SceneTreeDock::get_singleton();
	if (dock && dock->get_tree_editor()) {
		dock->get_tree_editor()->update_tree();
	}
}

AISceneEditingResult AISceneEditingService::_create_scene_main_thread(const String &p_root_type, const String &p_root_name, const String &p_path) {
	AISceneEditingResult result;
	String error;
	String scene_path;
	if (!_normalize_scene_save_path(p_path, scene_path, error)) {
		result.error = error;
		return result;
	}
	if (FileAccess::exists(scene_path)) {
		result.error = vformat("Scene file `%s` already exists.", scene_path);
		return result;
	}
	if (!_ensure_scene_save_directory(scene_path, error)) {
		result.error = error;
		return result;
	}

	Node *root = _instantiate_node(p_root_type, error);
	if (!root) {
		result.error = error;
		return result;
	}

	const String normalized_root_name = _normalize_node_name(nullptr, root, p_root_name);

	EditorNode *editor = EditorNode::get_singleton();
	SceneTreeDock *dock = SceneTreeDock::get_singleton();
	if (!editor || !dock) {
		memdelete(root);
		result.error = "Editor scene tree is not available.";
		return result;
	}

	editor->new_scene();
	dock->add_root_node(root);
	_select_node(root);
	_update_scene_tree();

	String saved_path;
	if (!_save_scene_main_thread(root, scene_path, saved_path, error)) {
		result.error = error;
		result.metadata["saved"] = false;
		result.metadata["scene_path"] = scene_path;
		return result;
	}

	result.success = true;
	result.message = vformat("Created and saved scene `%s` with root `%s` (%s).", saved_path, normalized_root_name, root->get_class());
	result.metadata["root_type"] = root->get_class();
	result.metadata["root_name"] = normalized_root_name;
	result.metadata["root_path"] = ".";
	result.metadata["scene_path"] = saved_path;
	result.metadata["saved"] = true;
	return result;
}

AISceneEditingResult AISceneEditingService::_add_node_main_thread(const String &p_parent_path, const String &p_type, const String &p_name) {
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

	Node *parent = _resolve_node_path(scene, p_parent_path, true, error);
	if (!parent) {
		result.error = error;
		return result;
	}

	Node *child = _instantiate_node(p_type, error);
	if (!child) {
		result.error = error;
		return result;
	}
	const String new_name = _normalize_node_name(parent, child, p_name);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (!undo_redo) {
		memdelete(child);
		result.error = "Editor undo/redo manager is not available.";
		return result;
	}

	const NodePath parent_path = scene->get_path_to(parent);
	const NodePath child_path = NodePath(String(parent_path).path_join(new_name));

	undo_redo->create_action_for_history(TTR("AI Create Node"), EditorNode::get_editor_data().get_current_edited_scene_history_id());
	undo_redo->add_do_method(parent, "add_child", child, true);
	undo_redo->add_do_method(child, "set_owner", scene);
	undo_redo->add_do_reference(child);
	undo_redo->add_undo_method(parent, "remove_child", child);

	EditorDebuggerNode *debugger = EditorDebuggerNode::get_singleton();
	if (debugger) {
		undo_redo->add_do_method(debugger, "live_debug_create_node", parent_path, child->get_class(), new_name);
		undo_redo->add_undo_method(debugger, "live_debug_remove_node", child_path);
	}
	undo_redo->commit_action();

	_select_node(child);
	_update_scene_tree();

	String saved_path;
	if (!_save_current_scene_main_thread(scene, saved_path, error)) {
		undo_redo->undo();
		_update_scene_tree();
		result.error = error;
		result.metadata["saved"] = false;
		result.metadata["parent_path"] = p_parent_path;
		result.metadata["node_type"] = child->get_class();
		result.metadata["node_name"] = String(child->get_name());
		result.metadata["node_path"] = String(scene->get_path_to(child));
		return result;
	}

	result.success = true;
	result.message = vformat("Added `%s` (%s) under `%s` and saved `%s`.", child->get_name(), child->get_class(), p_parent_path, saved_path);
	result.metadata["parent_path"] = p_parent_path;
	result.metadata["node_type"] = child->get_class();
	result.metadata["node_name"] = String(child->get_name());
	result.metadata["node_path"] = String(scene->get_path_to(child));
	result.metadata["scene_path"] = saved_path;
	result.metadata["saved"] = true;
	return result;
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

AISceneEditingResult AISceneEditingService::create_scene(const String &p_root_type, const String &p_root_name, const String &p_path) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_CREATE_SCENE;
	request.root_type = p_root_type;
	request.root_name = p_root_name;
	request.scene_path = p_path;
	return _dispatch_to_main_thread(request);
}

AISceneEditingResult AISceneEditingService::add_node(const String &p_parent_path, const String &p_type, const String &p_name) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_ADD_NODE;
	request.parent_path = p_parent_path;
	request.node_type = p_type;
	request.node_name = p_name;
	return _dispatch_to_main_thread(request);
}

AISceneEditingResult AISceneEditingService::delete_node(const String &p_node_path) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_DELETE_NODE;
	request.node_path = p_node_path;
	return _dispatch_to_main_thread(request);
}
