/**************************************************************************/
/*  ai_shader_editing_service.cpp                                         */
/**************************************************************************/

#include "ai_shader_editing_service.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/variant/variant.h"

#include "editor/ai_component/review/ai_change_set_store.h"
#include "editor/ai_component/review/ai_diff_service.h"
#include "editor/ai_component/tools/project/ai_project_tool_utils.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/editor_data.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/scene/scene_tree_editor.h"
#include "scene/main/node.h"
#include "scene/resources/material.h"
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

void _register_shader_review_change(const String &p_title, const String &p_path, const String &p_change_type, const String &p_old_text, const String &p_new_text,
		Dictionary &r_metadata) {
	Ref<AIToolExecutionContext> context = AIToolExecutionContext::get_current();
	if (context.is_null() || !context->should_review_changes()) {
		return;
	}

	Array changes;
	Dictionary metadata;
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
		case MainThreadRequest::OP_CREATE_SHADER:
			p_request->result = _create_shader_main_thread(p_request->shader_path, p_request->shader_type, p_request->shader_code, p_request->overwrite);
			break;
		case MainThreadRequest::OP_EDIT_SHADER:
			p_request->result = _edit_shader_main_thread(p_request->shader_path, p_request->shader_code);
			break;
		case MainThreadRequest::OP_DELETE_SHADER:
			p_request->result = _delete_shader_main_thread(p_request->shader_path);
			break;
		case MainThreadRequest::OP_APPLY_TO_NODE:
			p_request->result = _apply_to_node_main_thread(p_request->node_path, p_request->shader_path, p_request->target_property, p_request->shader_parameters);
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

bool AIShaderEditingService::_validate_shader_code(const String &p_code, String &r_error) const {
	const String code = p_code.strip_edges();
	if (code.is_empty()) {
		r_error = "Shader code is required.";
		return false;
	}
	if (!code.contains("shader_type")) {
		r_error = "Shader code must include a shader_type declaration.";
		return false;
	}
	return true;
}

bool AIShaderEditingService::_build_shader_code(const String &p_shader_type, const String &p_shader_code, String &r_code, String &r_error) const {
	const String code = p_shader_code.strip_edges();
	if (!code.is_empty()) {
		r_code = code + "\n";
		return _validate_shader_code(r_code, r_error);
	}

	const String shader_type = p_shader_type.strip_edges().is_empty() ? String("canvas_item") : p_shader_type.strip_edges();
	if (shader_type.contains("\n") || shader_type.contains("\r") || shader_type.contains(";")) {
		r_error = "Shader type must be a single shader_type value such as canvas_item or spatial.";
		return false;
	}

	r_code = "shader_type " + shader_type + ";\n";
	return _validate_shader_code(r_code, r_error);
}

bool AIShaderEditingService::_load_shader_resource(const String &p_path, Ref<Shader> &r_shader, String &r_error, bool p_ignore_cache) const {
	Ref<Resource> resource = ResourceLoader::load(p_path, "Shader", p_ignore_cache ? ResourceFormatLoader::CACHE_MODE_IGNORE : ResourceFormatLoader::CACHE_MODE_REUSE);
	Ref<Shader> shader = resource;
	if (shader.is_null()) {
		r_error = vformat("Failed to load shader `%s`.", p_path);
		return false;
	}
	r_shader = shader;
	return true;
}

bool AIShaderEditingService::_save_shader_resource_via_editor(const Ref<Shader> &p_shader, const String &p_path, String &r_error) const {
	if (p_shader.is_null()) {
		r_error = "Shader resource is invalid.";
		return false;
	}
	if (!_ensure_parent_directory(p_path, r_error)) {
		return false;
	}

	EditorNode *editor = EditorNode::get_singleton();
	if (!editor) {
		r_error = "EditorNode is not available.";
		return false;
	}

	const String localized_path = ProjectSettings::get_singleton()->localize_path(p_path);
	p_shader->set_path(localized_path, true);
	editor->save_resource_in_path(p_shader, localized_path);
	_refresh_file_system(localized_path);
	if (FileSystemDock::get_singleton()) {
		FileSystemDock::get_singleton()->select_file(localized_path);
	}
	editor->push_item(p_shader.ptr());
	return true;
}

bool AIShaderEditingService::_delete_shader_resource(const String &p_path, String &r_error) const {
	const String absolute_path = ProjectSettings::get_singleton()->globalize_path(p_path);
	OS *os = OS::get_singleton();
	if (!os) {
		r_error = "OS file service is not available.";
		return false;
	}

	Error err = os->move_to_trash(absolute_path);
	if (err != OK) {
		r_error = vformat("Failed to delete shader `%s` (error %d).", p_path, err);
		return false;
	}
	if (ResourceCache::has(p_path)) {
		Ref<Resource> cached_resource = ResourceCache::get_ref(p_path);
		if (cached_resource.is_valid()) {
			if (FileSystemDock::get_singleton()) {
				FileSystemDock::get_singleton()->emit_signal(SNAME("resource_removed"), cached_resource);
			}
			cached_resource->set_path("");
		}
	}
	_refresh_file_system(p_path);
	if (FileSystemDock::get_singleton()) {
		FileSystemDock::get_singleton()->emit_signal(SNAME("file_removed"), p_path);
		FileSystemDock::get_singleton()->select_file(p_path.get_base_dir());
	}
	return true;
}

bool AIShaderEditingService::_resource_hint_accepts_type(const String &p_hint_string, const StringName &p_type) const {
	PackedStringArray hint_types = p_hint_string.split(",");
	for (int i = 0; i < hint_types.size(); i++) {
		const String hint_type = hint_types[i].strip_edges();
		if (hint_type.is_empty()) {
			continue;
		}
		if (hint_type == p_type || ClassDB::is_parent_class(p_type, StringName(hint_type))) {
			return true;
		}
	}
	return false;
}

bool AIShaderEditingService::_get_exact_property_info(Object *p_object, const String &p_property_path, PropertyInfo &r_property_info) const {
	ERR_FAIL_NULL_V(p_object, false);

	List<PropertyInfo> property_list;
	p_object->get_property_list(&property_list);
	for (const PropertyInfo &property : property_list) {
		if (String(property.name) == p_property_path) {
			r_property_info = property;
			return true;
		}
	}
	return false;
}

bool AIShaderEditingService::_resolve_shader_target(Node *p_node, const String &p_target_property, Vector<StringName> &r_property_names,
		ShaderTargetKind &r_target_kind, Variant &r_old_value, String &r_error) const {
	ERR_FAIL_NULL_V(p_node, false);

	const String target_property = p_target_property.strip_edges();
	if (target_property.is_empty()) {
		r_error = "Target property is required.";
		return false;
	}
	if (target_property.begins_with("/") || target_property.contains("..")) {
		r_error = "Only target property paths relative to the target node are allowed.";
		return false;
	}

	if (!_get_property_names_for_assignment(p_node, target_property, r_property_names)) {
		r_error = "Target property path is invalid.";
		return false;
	}

	bool valid = false;
	r_old_value = p_node->get_indexed(r_property_names, &valid);
	if (!valid) {
		r_error = vformat("Property `%s` was not found on node `%s`.", target_property, p_node->get_name());
		return false;
	}

	PropertyInfo property_info;
	if (_get_exact_property_info(p_node, target_property, property_info)) {
		if (property_info.type != Variant::OBJECT || property_info.hint != PROPERTY_HINT_RESOURCE_TYPE) {
			r_error = vformat("Property `%s` on node `%s` is not a resource property.", target_property, p_node->get_name());
			return false;
		}
		if (_resource_hint_accepts_type(property_info.hint_string, Shader::get_class_static())) {
			r_target_kind = SHADER_TARGET_DIRECT_SHADER;
			return true;
		}
		if (_resource_hint_accepts_type(property_info.hint_string, ShaderMaterial::get_class_static()) ||
				_resource_hint_accepts_type(property_info.hint_string, Material::get_class_static())) {
			r_target_kind = SHADER_TARGET_SHADER_MATERIAL;
			return true;
		}

		r_error = vformat("Property `%s` on node `%s` does not accept Shader or ShaderMaterial resources.", target_property, p_node->get_name());
		return false;
	}

	Object *old_object = Object::cast_to<Object>(r_old_value);
	if (Object::cast_to<Shader>(old_object)) {
		r_target_kind = SHADER_TARGET_DIRECT_SHADER;
		return true;
	}
	if (Object::cast_to<ShaderMaterial>(old_object) || Object::cast_to<Material>(old_object)) {
		r_target_kind = SHADER_TARGET_SHADER_MATERIAL;
		return true;
	}

	r_error = vformat("Cannot infer whether property `%s` on node `%s` accepts Shader or ShaderMaterial. Use a concrete resource property such as "
					   "shader, shader_override, material, material_override, or surface_material_override/0.",
			target_property, p_node->get_name());
	return false;
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

	editor->save_scene_if_open(scene_path);
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
		EditorFileSystem::get_singleton()->call_deferred("scan_changes");
	}
}

void AIShaderEditingService::_update_scene_tree() const {
	SceneTreeDock *dock = SceneTreeDock::get_singleton();
	if (dock && dock->get_tree_editor()) {
		dock->get_tree_editor()->update_tree();
	}
}

AIShaderEditingResult AIShaderEditingService::_create_shader_main_thread(const String &p_shader_path, const String &p_shader_type, const String &p_shader_code, bool p_overwrite) {
	AIShaderEditingResult result;
	String error;

	String shader_path;
	if (!_normalize_shader_path(p_shader_path, shader_path, error)) {
		result.error = error;
		return result;
	}

	const bool existed_before = ResourceLoader::exists(shader_path);
	if (existed_before && !p_overwrite) {
		result.error = vformat("Shader `%s` already exists. Set overwrite=true to replace it.", shader_path);
		return result;
	}

	String shader_code;
	if (!_build_shader_code(p_shader_type, p_shader_code, shader_code, error)) {
		result.error = error;
		return result;
	}

	String old_shader_code;
	Ref<Shader> shader;
	if (existed_before) {
		if (!_load_shader_resource(shader_path, shader, error, true)) {
			result.error = error;
			return result;
		}
		old_shader_code = shader->get_code();
	} else {
		shader.instantiate();
	}
	shader->set_code(shader_code);

	if (!_save_shader_resource_via_editor(shader, shader_path, error)) {
		result.error = error;
		return result;
	}

	result.success = true;
	result.message = vformat("%s shader `%s`.", existed_before ? "Overwrote" : "Created", shader_path);
	result.metadata["shader_path"] = shader_path;
	result.metadata["created"] = !existed_before;
	result.metadata["overwritten"] = existed_before;
	result.metadata["shader_code_chars"] = shader_code.length();
	_register_shader_review_change(result.message, shader_path, existed_before ? String("modify") : String("create"), old_shader_code, shader_code, result.metadata);
	return result;
}

AIShaderEditingResult AIShaderEditingService::_edit_shader_main_thread(const String &p_shader_path, const String &p_shader_code) {
	AIShaderEditingResult result;
	String error;

	String shader_path;
	if (!_normalize_shader_path(p_shader_path, shader_path, error)) {
		result.error = error;
		return result;
	}
	if (!ResourceLoader::exists(shader_path)) {
		result.error = vformat("Shader `%s` does not exist.", shader_path);
		return result;
	}
	const String shader_code = p_shader_code.strip_edges() + "\n";
	if (!_validate_shader_code(shader_code, error)) {
		result.error = error;
		return result;
	}

	Ref<Shader> shader;
	if (!_load_shader_resource(shader_path, shader, error, true)) {
		result.error = error;
		return result;
	}
	const String old_shader_code = shader->get_code();
	shader->set_code(shader_code);

	if (!_save_shader_resource_via_editor(shader, shader_path, error)) {
		result.error = error;
		return result;
	}

	result.success = true;
	result.message = vformat("Edited shader `%s`.", shader_path);
	result.metadata["shader_path"] = shader_path;
	result.metadata["shader_code_chars"] = shader_code.length();
	_register_shader_review_change(result.message, shader_path, "modify", old_shader_code, shader_code, result.metadata);
	return result;
}

AIShaderEditingResult AIShaderEditingService::_delete_shader_main_thread(const String &p_shader_path) {
	AIShaderEditingResult result;
	String error;

	String shader_path;
	if (!_normalize_shader_path(p_shader_path, shader_path, error)) {
		result.error = error;
		return result;
	}
	if (!ResourceLoader::exists(shader_path)) {
		result.error = vformat("Shader `%s` does not exist.", shader_path);
		return result;
	}

	String old_shader_code;
	Ref<Shader> shader;
	if (_load_shader_resource(shader_path, shader, error, true)) {
		old_shader_code = shader->get_code();
	} else {
		result.error = error;
		return result;
	}

	if (!_delete_shader_resource(shader_path, error)) {
		result.error = error;
		return result;
	}

	result.success = true;
	result.message = vformat("Deleted shader `%s`.", shader_path);
	result.metadata["shader_path"] = shader_path;
	result.metadata["deleted"] = true;
	_register_shader_review_change(result.message, shader_path, "delete", old_shader_code, String(), result.metadata);
	return result;
}

AIShaderEditingResult AIShaderEditingService::_apply_to_node_main_thread(const String &p_node_path, const String &p_shader_path,
		const String &p_target_property, const Dictionary &p_shader_parameters) {
	AIShaderEditingResult result;
	String error;

	String shader_path;
	if (!_normalize_shader_path(p_shader_path, shader_path, error)) {
		result.error = error;
		return result;
	}
	Ref<Shader> shader;
	if (!_load_shader_resource(shader_path, shader, error)) {
		result.error = error;
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

	Vector<StringName> property_names;
	ShaderTargetKind target_kind = SHADER_TARGET_SHADER_MATERIAL;
	Variant old_value;
	if (!_resolve_shader_target(node, p_target_property, property_names, target_kind, old_value, error)) {
		result.error = error;
		return result;
	}
	if (target_kind == SHADER_TARGET_DIRECT_SHADER && !p_shader_parameters.is_empty()) {
		result.error = "shader_parameters can only be used when target_property accepts a ShaderMaterial or Material.";
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

	NodePath property_path(Vector<StringName>(), property_names, false);
	Variant new_value = shader;
	if (target_kind == SHADER_TARGET_SHADER_MATERIAL) {
		Ref<ShaderMaterial> material = old_value;
		if (material.is_null()) {
			material.instantiate();
		} else {
			Ref<Resource> duplicated = material->duplicate();
			material = duplicated;
			if (material.is_null()) {
				material.instantiate();
			}
		}
		material->set_shader(shader);
		for (const KeyValue<Variant, Variant> &E : p_shader_parameters) {
			const String parameter_name = String(E.key).strip_edges();
			if (parameter_name.is_empty()) {
				continue;
			}
			material->set_shader_parameter(StringName(parameter_name), E.value);
		}
		new_value = material;
	}

	undo_redo->create_action_for_history(TTR("AI Apply Shader"), EditorNode::get_editor_data().get_current_edited_scene_history_id(), UndoRedo::MERGE_DISABLE, false, true);
	undo_redo->add_do_method(node, "set_indexed", property_path, new_value);
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
		result.metadata["target_property"] = p_target_property;
		return result;
	}

	result.success = true;
	result.message = vformat("Applied shader `%s` to `%s:%s` and saved `%s`.", shader_path, p_node_path, p_target_property, saved_path);
	result.metadata["node_path"] = p_node_path;
	result.metadata["shader_path"] = shader_path;
	result.metadata["target_property"] = p_target_property;
	result.metadata["target_kind"] = target_kind == SHADER_TARGET_DIRECT_SHADER ? "shader" : "shader_material";
	result.metadata["shader_parameter_count"] = p_shader_parameters.size();
	result.metadata["scene_path"] = saved_path;
	result.metadata["saved"] = true;
	return result;
}

AIShaderEditingResult AIShaderEditingService::create_shader(const String &p_shader_path, const String &p_shader_type, const String &p_shader_code, bool p_overwrite) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_CREATE_SHADER;
	request.shader_path = p_shader_path;
	request.shader_type = p_shader_type;
	request.shader_code = p_shader_code;
	request.overwrite = p_overwrite;
	return _dispatch_to_main_thread(request);
}

AIShaderEditingResult AIShaderEditingService::edit_shader(const String &p_shader_path, const String &p_shader_code) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_EDIT_SHADER;
	request.shader_path = p_shader_path;
	request.shader_code = p_shader_code;
	return _dispatch_to_main_thread(request);
}

AIShaderEditingResult AIShaderEditingService::delete_shader(const String &p_shader_path) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_DELETE_SHADER;
	request.shader_path = p_shader_path;
	return _dispatch_to_main_thread(request);
}

AIShaderEditingResult AIShaderEditingService::apply_to_node(const String &p_node_path, const String &p_shader_path, const String &p_target_property,
		const Dictionary &p_shader_parameters) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_APPLY_TO_NODE;
	request.node_path = p_node_path;
	request.shader_path = p_shader_path;
	request.target_property = p_target_property;
	request.shader_parameters = p_shader_parameters;
	return _dispatch_to_main_thread(request);
}
