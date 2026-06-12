/**************************************************************************/
/*  ai_editor_tool_service.cpp                                            */
/**************************************************************************/

#include "ai_editor_tool_service.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/editor_data.h"
#include "editor/editor_node.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/scene/editor_scene_tabs.h"
#include "editor/scene/scene_tree_editor.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"

Mutex AIV1EditorToolService::main_thread_dispatch_mutex;
Vector<AIV1EditorToolService::MainThreadDispatchItem> AIV1EditorToolService::main_thread_dispatch_items;
uint64_t AIV1EditorToolService::main_thread_dispatch_next_id = 0;

void AIV1EditorToolService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update_scene_tree"), &AIV1EditorToolService::_update_scene_tree);
	ClassDB::bind_method(D_METHOD("_refresh_file_system", "path"), &AIV1EditorToolService::_refresh_file_system);
}

Error AIV1EditorToolService::_queue_main_thread_dispatch(const Callable &p_callable, const Variant &p_argument, uint64_t &r_item_id) {
	r_item_id = 0;
	CallQueue *message_queue = MessageQueue::get_main_singleton();
	if (!message_queue) {
		return ERR_UNAVAILABLE;
	}

	{
		MutexLock lock(main_thread_dispatch_mutex);
		MainThreadDispatchItem item;
		item.id = ++main_thread_dispatch_next_id;
		item.callable = p_callable;
		item.argument = p_argument;
		r_item_id = item.id;
		main_thread_dispatch_items.push_back(item);
	}

	const Error err = message_queue->push_callable(callable_mp_static(&AIV1EditorToolService::flush_pending_main_thread_dispatches_for_wait));
	if (err != OK) {
		bool removed_item = false;
		MutexLock lock(main_thread_dispatch_mutex);
		for (int i = main_thread_dispatch_items.size() - 1; i >= 0; i--) {
			if (main_thread_dispatch_items[i].id == r_item_id) {
				main_thread_dispatch_items.remove_at(i);
				removed_item = true;
				break;
			}
		}
		if (removed_item) {
			r_item_id = 0;
		}
		return removed_item ? err : OK;
	}

	return OK;
}

bool AIV1EditorToolService::_remove_queued_main_thread_dispatch(uint64_t p_item_id) {
	if (p_item_id == 0) {
		return false;
	}

	MutexLock lock(main_thread_dispatch_mutex);
	for (int i = main_thread_dispatch_items.size() - 1; i >= 0; i--) {
		if (main_thread_dispatch_items[i].id == p_item_id) {
			main_thread_dispatch_items.remove_at(i);
			return true;
		}
	}
	return false;
}

void AIV1EditorToolService::flush_pending_main_thread_dispatches_for_wait() {
	if (!Thread::is_main_thread()) {
		return;
	}

	while (true) {
		Vector<MainThreadDispatchItem> dispatch_items;
		{
			MutexLock lock(main_thread_dispatch_mutex);
			if (main_thread_dispatch_items.is_empty()) {
				return;
			}
			dispatch_items = main_thread_dispatch_items;
			main_thread_dispatch_items.clear();
		}

		for (int i = 0; i < dispatch_items.size(); i++) {
			const Variant *argptrs[1] = { &dispatch_items[i].argument };
			Callable::CallError ce;
			Variant ret;
			dispatch_items[i].callable.callp(argptrs, 1, ret, ce);
			if (ce.error != Callable::CallError::CALL_OK) {
				ERR_PRINT("Failed to dispatch AI editor tool request: " + Variant::get_callable_error_text(dispatch_items[i].callable, argptrs, 1, ce) + ".");
			}
		}
	}
}

Node *AIV1EditorToolService::_get_edited_scene(String &r_error) const {
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

Node *AIV1EditorToolService::_resolve_node_path(Node *p_scene_root, const String &p_path, bool p_allow_root, String &r_error, bool p_disallow_property_subpaths, const String &p_not_found_error) const {
	ERR_FAIL_NULL_V(p_scene_root, nullptr);

	r_error.clear();
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
	if (p_disallow_property_subpaths && stripped_path.contains(":")) {
		r_error = "Property subpaths are not allowed; provide a node path only.";
		return nullptr;
	}

	Node *node = p_scene_root->get_node_or_null(NodePath(stripped_path));
	if (!node) {
		r_error = p_not_found_error.is_empty() ? vformat("Node `%s` was not found in the edited scene.", stripped_path) : p_not_found_error;
		return nullptr;
	}
	return node;
}

bool AIV1EditorToolService::_ensure_project_parent_directory(const String &p_path, const String &p_resource_label, String &r_error) const {
	const String base_dir = p_path.get_base_dir();
	if (base_dir.is_empty() || base_dir == "res://") {
		return true;
	}

	const String absolute_dir = ProjectSettings::get_singleton()->globalize_path(base_dir);
	Error err = DirAccess::make_dir_recursive_absolute(absolute_dir);
	if (err != OK) {
		r_error = vformat("Failed to create %s directory `%s` (error %d).", p_resource_label, base_dir, err);
		return false;
	}
	return true;
}

bool AIV1EditorToolService::_ensure_editor_filesystem_parent_directory(const String &p_path, const String &p_resource_label, String &r_error) const {
	const String base_dir = p_path.get_base_dir();
	if (base_dir.is_empty() || base_dir == "res://") {
		return true;
	}

	EditorFileSystem *file_system = EditorFileSystem::get_singleton();
	if (!file_system) {
		r_error = "EditorFileSystem is not available.";
		return false;
	}

	if (file_system->get_filesystem_path(base_dir)) {
		return true;
	}
	Error err = file_system->make_dir_recursive(base_dir);
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		r_error = vformat("Failed to create %s directory `%s` (error %d).", p_resource_label, base_dir, err);
		return false;
	}
	return true;
}

bool AIV1EditorToolService::_save_current_scene_with_packed_scene_main_thread(Node *p_scene, const String &p_unsaved_error, String &r_saved_path, String &r_error) const {
	ERR_FAIL_NULL_V(p_scene, false);

	EditorNode *editor = EditorNode::get_singleton();
	if (!editor) {
		r_error = "EditorNode is not available.";
		return false;
	}
	const String scene_path = p_scene->get_scene_file_path();
	if (scene_path.is_empty()) {
		r_error = p_unsaved_error;
		return false;
	}

	Ref<PackedScene> packed_scene;
	packed_scene.instantiate();
	Error err = packed_scene->pack(p_scene);
	if (err != OK) {
		r_error = vformat("Failed to pack scene before saving (error %d).", err);
		return false;
	}

	uint32_t flags = ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;
	err = ResourceSaver::save(packed_scene, scene_path, flags);
	if (err != OK) {
		r_error = vformat("Failed to save scene `%s` (error %d).", scene_path, err);
		return false;
	}

	int scene_index = EditorNode::get_editor_data().get_edited_scene();
	if (scene_index >= 0) {
		EditorNode::get_editor_data().notify_scene_saved(scene_path);
		EditorNode::get_editor_data().set_scene_as_saved(scene_index);
		EditorNode::get_editor_data().set_scene_modified_time(scene_index, FileAccess::get_modified_time(scene_path));
	}
	editor->emit_signal(SNAME("scene_saved"), scene_path);
	if (EditorSceneTabs::get_singleton()) {
		EditorSceneTabs::get_singleton()->update_scene_tabs();
	}
	_refresh_file_system(scene_path);
	r_saved_path = scene_path;
	return true;
}

bool AIV1EditorToolService::_save_current_scene_with_editor_main_thread(Node *p_scene, const String &p_unsaved_error, String &r_saved_path, String &r_error) const {
	ERR_FAIL_NULL_V(p_scene, false);

	EditorNode *editor = EditorNode::get_singleton();
	if (!editor) {
		r_error = "EditorNode is not available.";
		return false;
	}
	const String scene_path = p_scene->get_scene_file_path();
	if (scene_path.is_empty()) {
		r_error = p_unsaved_error;
		return false;
	}

	editor->save_scene_if_open(scene_path);
	_refresh_file_system(scene_path);
	r_saved_path = scene_path;
	return true;
}

void AIV1EditorToolService::_refresh_file_system(const String &p_path) const {
	if (!Thread::is_main_thread()) {
		if (MessageQueue::get_main_singleton()) {
			MessageQueue::get_main_singleton()->push_callable(callable_mp(const_cast<AIV1EditorToolService *>(this), &AIV1EditorToolService::_refresh_file_system), p_path);
		}
		return;
	}
	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(p_path);
		EditorFileSystem::get_singleton()->call_deferred("scan_changes");
	}
}

void AIV1EditorToolService::_update_scene_tree() const {
	SceneTreeDock *dock = SceneTreeDock::get_singleton();
	if (dock && dock->get_tree_editor()) {
		dock->get_tree_editor()->update_tree();
	}
}

Node *AIV1EditorToolService::resolve_scene_node_path_for_test(Node *p_scene_root, const String &p_path, bool p_allow_root, String &r_error) {
	Ref<AIV1EditorToolService> service;
	service.instantiate();
	return service->_resolve_node_path(p_scene_root, p_path, p_allow_root, r_error);
}
