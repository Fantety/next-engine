/**************************************************************************/
/*  ai_shader_editing_service.cpp                                         */
/**************************************************************************/

#include "ai_shader_editing_service.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/os/thread.h"
#include "core/variant/variant.h"

#include "editor/ai_component/review/ai_change_set_store.h"
#include "editor/ai_component/review/ai_diff_service.h"
#include "editor/ai_component/tools/project/ai_project_tool_utils.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/editor_data.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/scene/editor_scene_tabs.h"
#include "editor/scene/scene_tree_editor.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/main/canvas_item.h"
#include "scene/main/node.h"
#include "scene/resources/material.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/shader.h"

namespace {

bool _get_property_names_for_assignment(Object *p_object, const String &p_property_path, Vector<StringName> &r_names) {
	ERR_FAIL_NULL_V(p_object, false);

	List<PropertyInfo> property_list;
	p_object->get_property_list(&property_list);
	for (const PropertyInfo &property : property_list) {
		if (String(property.name) == p_property_path) {
			r_names.clear();
			r_names.push_back(StringName(p_property_path));
			return true;
		}
	}

	r_names = NodePath(p_property_path).get_as_property_path().get_subnames();
	return !r_names.is_empty();
}

bool _read_shader_text(const String &p_path, String &r_text, String &r_error) {
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		r_error = vformat("Failed to read shader `%s` (error %d).", p_path, err);
		return false;
	}
	r_text = file->get_as_text();
	return true;
}

void _register_shader_review_change(const String &p_title, const String &p_path, const String &p_change_type, const String &p_old_text, const String &p_new_text, Dictionary &r_metadata) {
	Ref<AIToolExecutionContext> context = AIToolExecutionContext::get_current();
	if (context.is_null() || !context->is_review_mode()) {
		return;
	}

	Array changes;
	Dictionary metadata;
	metadata["scene_binding_not_reverted"] = true;
	changes.push_back(AIDiffService::build_text_change(p_path, p_change_type, p_old_text, p_new_text, "gdshader", metadata));
	Ref<AIChangeSetStore> store = AIChangeSetStore::get_singleton();
	const String change_set_id = store->add_change_set(p_title, context->get_session_id(), context->get_tool_call_id(), changes, metadata);
	if (change_set_id.is_empty()) {
		print_line(vformat("[AI Agent][Review] Skipped shader change. path=%s type=%s", p_path, p_change_type));
		return;
	}
	r_metadata["review_change_set_id"] = change_set_id;
	r_metadata["review_status"] = "pending";
	r_metadata["review_mode"] = true;
	r_metadata["review_note"] = "Reverting this review change restores the shader file. Scene material binding is not reverted in the first review implementation.";
	print_line(vformat("[AI Agent][Review] Recorded shader change. id=%s path=%s type=%s", change_set_id, p_path, p_change_type));
}

} // namespace

void AIShaderEditingService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update_scene_tree"), &AIShaderEditingService::_update_scene_tree);
}

AIShaderEditingService *AIShaderEditingService::get_dispatcher_singleton() {
	static Ref<AIShaderEditingService> dispatcher;
	if (dispatcher.is_null()) {
		dispatcher.instantiate();
	}
	return dispatcher.ptr();
}

AIShaderEditingResult AIShaderEditingService::_dispatch_to_main_thread(MainThreadRequest &r_request) {
	r_request.execution_context = AIToolExecutionContext::get_current();
	if (Thread::is_main_thread()) {
		_execute_request_ptr(&r_request);
		return r_request.result;
	}

	MutexLock lock(request_mutex);
	if (!MessageQueue::get_main_singleton()) {
		AIShaderEditingResult result;
		result.error = "Main thread dispatch is not available.";
		return result;
	}

	AIShaderEditingService *dispatcher = get_dispatcher_singleton();
	Variant request_ptr = reinterpret_cast<uint64_t>(&r_request);
	Error err = MessageQueue::get_main_singleton()->push_callable(callable_mp(dispatcher, &AIShaderEditingService::_execute_request), request_ptr);
	if (err != OK) {
		AIShaderEditingResult result;
		result.error = "Failed to schedule shader editing on the main thread.";
		return result;
	}

	r_request.done.wait();
	return r_request.result;
}

void AIShaderEditingService::_execute_request(uint64_t p_request_ptr) {
	_execute_request_ptr(reinterpret_cast<MainThreadRequest *>(p_request_ptr));
}

void AIShaderEditingService::_execute_request_ptr(MainThreadRequest *p_request) {
	ERR_FAIL_NULL(p_request);

	Ref<AIToolExecutionContext> previous_context = AIToolExecutionContext::get_current();
	if (p_request->execution_context.is_valid()) {
		AIToolExecutionContext::set_current(p_request->execution_context);
	}

	switch (p_request->operation) {
		case MainThreadRequest::OP_APPLY_TO_NODE:
			p_request->result = _apply_to_node_main_thread(p_request->node_path, p_request->shader_path, p_request->shader_code, p_request->material_property, p_request->shader_parameters, p_request->overwrite_shader);
			break;
	}

	if (previous_context.is_valid()) {
		AIToolExecutionContext::set_current(previous_context);
	} else {
		AIToolExecutionContext::clear_current();
	}

	p_request->done.post();
}

bool AIShaderEditingService::_normalize_shader_path(const String &p_path, String &r_path, String &r_error) const {
	const String path = p_path.strip_edges().replace_char('\\', '/').simplify_path();
	if (path.is_empty()) {
		r_error = "Shader path is required.";
		return false;
	}
	if (!AIProjectToolUtils::is_allowed_path(path)) {
		r_error = "Shader path is outside the allowed project boundary.";
		return false;
	}
	if (path.get_extension().to_lower() != "gdshader") {
		r_error = "Only `.gdshader` files are supported by this tool.";
		return false;
	}
	if (path.get_file().is_empty()) {
		r_error = "Shader path must include a file name.";
		return false;
	}

	r_path = path;
	return true;
}

bool AIShaderEditingService::_ensure_parent_directory(const String &p_path, String &r_error) const {
	const String base_dir = p_path.get_base_dir();
	if (base_dir.is_empty() || base_dir == "res://") {
		return true;
	}

	const String absolute_dir = ProjectSettings::get_singleton()->globalize_path(base_dir);
	Error err = DirAccess::make_dir_recursive_absolute(absolute_dir);
	if (err != OK) {
		r_error = vformat("Failed to create shader directory `%s` (error %d).", base_dir, err);
		return false;
	}
	return true;
}

Node *AIShaderEditingService::_get_edited_scene(String &r_error) const {
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

Node *AIShaderEditingService::_resolve_node_path(Node *p_scene_root, const String &p_path, bool p_allow_root, String &r_error) const {
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

	Node *node = p_scene_root->get_node_or_null(NodePath(stripped_path));
	if (!node) {
		r_error = vformat("Node `%s` was not found in the edited scene.", stripped_path);
		return nullptr;
	}
	return node;
}

String AIShaderEditingService::_resolve_material_property(Node *p_node, const String &p_requested_property, String &r_error) const {
	ERR_FAIL_NULL_V(p_node, String());

	const String requested_property = p_requested_property.strip_edges();
	if (!requested_property.is_empty()) {
		if (requested_property.begins_with("/") || requested_property.contains("..")) {
			r_error = "Only material property paths relative to the target node are allowed.";
			return String();
		}
		return requested_property;
	}

	if (Object::cast_to<CanvasItem>(p_node)) {
		return "material";
	}
	if (Object::cast_to<GeometryInstance3D>(p_node)) {
		return "material_override";
	}

	r_error = vformat("Node `%s` does not expose a default shader material property. Provide material_property explicitly.", p_node->get_name());
	return String();
}

bool AIShaderEditingService::_property_accepts_shader_material(Node *p_node, const String &p_property_path) const {
	ERR_FAIL_NULL_V(p_node, false);

	List<PropertyInfo> property_list;
	p_node->get_property_list(&property_list);
	for (const PropertyInfo &property : property_list) {
		if (String(property.name) != p_property_path) {
			continue;
		}
		if (property.type != Variant::OBJECT || property.hint != PROPERTY_HINT_RESOURCE_TYPE) {
			return false;
		}
		PackedStringArray hint_types = property.hint_string.split(",");
		for (int i = 0; i < hint_types.size(); i++) {
			if (hint_types[i].strip_edges() == "ShaderMaterial") {
				return true;
			}
		}
		return false;
	}
	return false;
}

bool AIShaderEditingService::_save_shader_resource(const String &p_path, const String &p_code, bool p_overwrite, bool &r_existed_before, Ref<Shader> &r_shader, String &r_error) const {
	r_existed_before = ResourceLoader::exists(p_path);
	if (r_existed_before && !p_overwrite) {
		r_error = vformat("Shader `%s` already exists. Set overwrite_shader=true to replace it.", p_path);
		return false;
	}
	if (!_ensure_parent_directory(p_path, r_error)) {
		return false;
	}

	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(p_code.strip_edges() + "\n");
	shader->set_path(p_path, true);

	Error err = ResourceSaver::save(shader, p_path, ResourceSaver::FLAG_CHANGE_PATH);
	if (err != OK) {
		r_error = vformat("Failed to save shader resource `%s` (error %d).", p_path, err);
		return false;
	}

	r_shader = shader;
	_refresh_file_system(p_path);
	return true;
}

bool AIShaderEditingService::_save_current_scene_main_thread(Node *p_scene, String &r_saved_path, String &r_error) const {
	ERR_FAIL_NULL_V(p_scene, false);

	EditorNode *editor = EditorNode::get_singleton();
	if (!editor) {
		r_error = "EditorNode is not available.";
		return false;
	}
	const String scene_path = p_scene->get_scene_file_path();
	if (scene_path.is_empty()) {
		r_error = "The current scene must be saved before shader material bindings can be persisted.";
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
	}
	editor->emit_signal(SNAME("scene_saved"), scene_path);
	if (EditorSceneTabs::get_singleton()) {
		EditorSceneTabs::get_singleton()->update_scene_tabs();
	}
	_refresh_file_system(scene_path);
	r_saved_path = scene_path;
	return true;
}

void AIShaderEditingService::_refresh_file_system(const String &p_path) const {
	if (!Thread::is_main_thread()) {
		if (MessageQueue::get_main_singleton()) {
			MessageQueue::get_main_singleton()->push_callable(callable_mp(get_dispatcher_singleton(), &AIShaderEditingService::_refresh_file_system), p_path);
		}
		return;
	}
	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(p_path);
		EditorFileSystem::get_singleton()->scan_changes();
	}
}

void AIShaderEditingService::_update_scene_tree() const {
	SceneTreeDock *dock = SceneTreeDock::get_singleton();
	if (dock && dock->get_tree_editor()) {
		dock->get_tree_editor()->update_tree();
	}
}

AIShaderEditingResult AIShaderEditingService::_apply_to_node_main_thread(const String &p_node_path, const String &p_shader_path, const String &p_shader_code, const String &p_material_property, const Dictionary &p_shader_parameters, bool p_overwrite_shader) {
	AIShaderEditingResult result;
	String error;

	String shader_path;
	if (!_normalize_shader_path(p_shader_path, shader_path, error)) {
		result.error = error;
		return result;
	}
	const String shader_code = p_shader_code.strip_edges();
	if (shader_code.is_empty()) {
		result.error = "Shader code is required.";
		return result;
	}
	if (!shader_code.contains("shader_type")) {
		result.error = "Shader code must include a shader_type declaration.";
		return result;
	}

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

	const String material_property = _resolve_material_property(node, p_material_property, error);
	if (material_property.is_empty()) {
		result.error = error;
		return result;
	}
	if (!_property_accepts_shader_material(node, material_property)) {
		result.error = vformat("Property `%s` on node `%s` does not accept ShaderMaterial.", material_property, p_node_path);
		return result;
	}

	Vector<StringName> property_names;
	if (!_get_property_names_for_assignment(node, material_property, property_names)) {
		result.error = "Material property path is invalid.";
		return result;
	}
	NodePath property_path(Vector<StringName>(), property_names, false);

	bool valid = false;
	Variant old_value = node->get_indexed(property_names, &valid);
	if (!valid) {
		result.error = vformat("Property `%s` was not found on node `%s`.", material_property, p_node_path);
		return result;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (!undo_redo) {
		result.error = "Editor undo/redo manager is not available.";
		return result;
	}
	if (scene->get_scene_file_path().is_empty()) {
		result.error = "The current scene must be saved before shader material bindings can be persisted.";
		return result;
	}

	bool existed_before = false;
	String old_shader_code;
	if (ResourceLoader::exists(shader_path) && !_read_shader_text(shader_path, old_shader_code, error)) {
		result.error = error;
		return result;
	}
	Ref<Shader> shader;
	if (!_save_shader_resource(shader_path, shader_code, p_overwrite_shader, existed_before, shader, error)) {
		result.error = error;
		return result;
	}

	Ref<ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);
	for (const KeyValue<Variant, Variant> &E : p_shader_parameters) {
		const String parameter_name = String(E.key).strip_edges();
		if (parameter_name.is_empty()) {
			continue;
		}
		material->set_shader_parameter(StringName(parameter_name), E.value);
	}

	undo_redo->create_action_for_history(TTR("AI Apply Shader Material"), EditorNode::get_editor_data().get_current_edited_scene_history_id(), UndoRedo::MERGE_DISABLE, false, true);
	undo_redo->add_do_method(node, "set_indexed", property_path, material);
	undo_redo->add_undo_method(node, "set_indexed", property_path, old_value);
	undo_redo->add_do_method(this, "_update_scene_tree");
	undo_redo->add_undo_method(this, "_update_scene_tree");
	undo_redo->commit_action();
	_update_scene_tree();

	String saved_path;
	if (!_save_current_scene_main_thread(scene, saved_path, error)) {
		undo_redo->undo();
		_update_scene_tree();
		result.error = error;
		result.metadata["saved"] = false;
		result.metadata["node_path"] = p_node_path;
		result.metadata["shader_path"] = shader_path;
		result.metadata["material_property"] = material_property;
		return result;
	}

	result.success = true;
	result.message = vformat("Applied ShaderMaterial using `%s` to `%s:%s` and saved `%s`.", shader_path, p_node_path, material_property, saved_path);
	result.metadata["node_path"] = p_node_path;
	result.metadata["shader_path"] = shader_path;
	result.metadata["shader_created"] = !existed_before;
	result.metadata["shader_overwritten"] = existed_before;
	result.metadata["material_property"] = material_property;
	result.metadata["shader_parameter_count"] = p_shader_parameters.size();
	result.metadata["scene_path"] = saved_path;
	result.metadata["saved"] = true;
	_register_shader_review_change(result.message, shader_path, existed_before ? String("modify") : String("create"), old_shader_code, shader_code.strip_edges() + "\n", result.metadata);
	return result;
}

AIShaderEditingResult AIShaderEditingService::apply_to_node(const String &p_node_path, const String &p_shader_path, const String &p_shader_code, const String &p_material_property, const Dictionary &p_shader_parameters, bool p_overwrite_shader) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_APPLY_TO_NODE;
	request.node_path = p_node_path;
	request.shader_path = p_shader_path;
	request.shader_code = p_shader_code;
	request.material_property = p_material_property;
	request.shader_parameters = p_shader_parameters;
	request.overwrite_shader = p_overwrite_shader;
	return _dispatch_to_main_thread(request);
}
