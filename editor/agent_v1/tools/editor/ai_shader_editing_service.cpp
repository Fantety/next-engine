/**************************************************************************/
/*  ai_shader_editing_service.cpp                                         */
/**************************************************************************/

#include "ai_shader_editing_service.h"

#include "core/config/project_settings.h"
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/object/property_info.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/variant/variant.h"

#include "editor/ai_component/review/ai_change_set_store.h"
#include "editor/ai_component/review/ai_diff_service.h"
#include "editor/agent_v1/tools/project/ai_project_tool_utils.h"
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
	Ref<AIV1ToolExecutionState> context = AIV1ToolExecutionState::get_current();
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

void AIV1ShaderEditingService::_bind_methods() {
}

AIV1ShaderEditingService *AIV1ShaderEditingService::get_dispatcher_singleton() {
	static Ref<AIV1ShaderEditingService> dispatcher;
	if (dispatcher.is_null()) {
		dispatcher.instantiate();
	}
	return dispatcher.ptr();
}

AIV1ShaderEditingResult AIV1ShaderEditingService::_dispatch_to_main_thread(MainThreadRequest &r_request) {
	r_request.execution_context = AIV1ToolExecutionState::get_current();
	return _dispatch_main_thread_request<AIV1ShaderEditingResult>(r_request, get_dispatcher_singleton(), &AIV1ShaderEditingService::_execute_request, request_mutex, "Failed to schedule shader editing on the main thread.");
}

void AIV1ShaderEditingService::_execute_request(uint64_t p_request_ptr) {
	_execute_request_ptr(reinterpret_cast<MainThreadRequest *>(p_request_ptr));
}

void AIV1ShaderEditingService::_execute_request_ptr(MainThreadRequest *p_request) {
	ERR_FAIL_NULL(p_request);

	Ref<AIV1ToolExecutionState> previous_context = AIV1ToolExecutionState::get_current();
	if (p_request->execution_context.is_valid()) {
		AIV1ToolExecutionState::set_current(p_request->execution_context);
	}
	if (AIV1ToolExecutionState::is_current_cancel_requested()) {
		p_request->result.error = "Tool execution cancelled.";
		p_request->result.metadata["cancelled"] = true;
		if (previous_context.is_valid()) {
			AIV1ToolExecutionState::set_current(previous_context);
		} else {
			AIV1ToolExecutionState::clear_current();
		}
		p_request->done.post();
		return;
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
		case MainThreadRequest::OP_SET_PARAMETERS:
			p_request->result = _set_parameters_main_thread(p_request->node_path, p_request->target_property, p_request->shader_parameters);
			break;
	}

	if (previous_context.is_valid()) {
		AIV1ToolExecutionState::set_current(previous_context);
	} else {
		AIV1ToolExecutionState::clear_current();
	}

	p_request->done.post();
}

bool AIV1ShaderEditingService::_normalize_shader_path(const String &p_path, String &r_path, String &r_error) const {
	const String path = p_path.strip_edges().replace_char('\\', '/').simplify_path();
	if (path.is_empty()) {
		r_error = "Shader path is required.";
		return false;
	}
	if (!AIV1ProjectToolUtils::is_allowed_path(path)) {
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

bool AIV1ShaderEditingService::_validate_shader_code(const String &p_code, String &r_error) const {
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

bool AIV1ShaderEditingService::_build_shader_code(const String &p_shader_type, const String &p_shader_code, String &r_code, String &r_error) const {
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

bool AIV1ShaderEditingService::_load_shader_resource(const String &p_path, Ref<Shader> &r_shader, String &r_error, bool p_ignore_cache) const {
	Ref<Resource> resource = ResourceLoader::load(p_path, "Shader", p_ignore_cache ? ResourceFormatLoader::CACHE_MODE_IGNORE : ResourceFormatLoader::CACHE_MODE_REUSE);
	Ref<Shader> shader = resource;
	if (shader.is_null()) {
		r_error = vformat("Failed to load shader `%s`.", p_path);
		return false;
	}
	r_shader = shader;
	return true;
}

bool AIV1ShaderEditingService::_save_shader_resource_via_editor(const Ref<Shader> &p_shader, const String &p_path, String &r_error) const {
	if (p_shader.is_null()) {
		r_error = "Shader resource is invalid.";
		return false;
	}
	if (!_ensure_editor_filesystem_parent_directory(p_path, "shader", r_error)) {
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

bool AIV1ShaderEditingService::_delete_shader_resource(const String &p_path, String &r_error) const {
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

bool AIV1ShaderEditingService::_resource_hint_accepts_type(const String &p_hint_string, const StringName &p_type) const {
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

bool AIV1ShaderEditingService::_is_resource_type_allowed(const String &p_type, const String &p_hint_string) const {
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
		if (hint_type.is_empty() || !ClassDB::class_exists(hint_type)) {
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

bool AIV1ShaderEditingService::_get_exact_property_info(Object *p_object, const String &p_property_path, PropertyInfo &r_property_info) const {
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

bool AIV1ShaderEditingService::_normalize_property_path(const String &p_property_path, Vector<StringName> &r_names, String &r_error) const {
	const String stripped_path = p_property_path.strip_edges();
	if (stripped_path.is_empty()) {
		r_error = "Property path is required.";
		return false;
	}
	if (stripped_path.begins_with("/") || stripped_path.contains("..")) {
		r_error = "Only property paths relative to the target object are allowed.";
		return false;
	}

	r_names = NodePath(stripped_path).get_as_property_path().get_subnames();
	if (r_names.is_empty()) {
		r_error = "Property path is invalid.";
		return false;
	}
	return true;
}

Ref<Resource> AIV1ShaderEditingService::_instantiate_resource(const String &p_type, String &r_error) const {
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

bool AIV1ShaderEditingService::_apply_object_properties(Object *p_object, const Dictionary &p_properties, String &r_error) const {
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
		PropertyInfo fallback_info(target_type, property_path);
		const PropertyInfo &property_info = listed_property ? listed_property_info : fallback_info;
		if (!_convert_value_for_shader_uniform(E.value, property_info, converted_value, r_error)) {
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

bool AIV1ShaderEditingService::_convert_array_typed_value(const Array &p_value, Variant::Type p_target_type, Variant &r_value, String &r_error) const {
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

bool AIV1ShaderEditingService::_convert_dictionary_typed_value(const Dictionary &p_value, Variant::Type p_target_type, Variant &r_value, String &r_error) const {
	const String type_name = String(p_value.get("type", "")).strip_edges();
	if (!type_name.is_empty()) {
		const Variant::Type requested_type = Variant::get_type_by_name(type_name);
		if (requested_type == Variant::VARIANT_MAX) {
			r_error = "Unknown typed value type.";
			return false;
		}
		if (requested_type != p_target_type) {
			r_error = vformat("Typed value declares %s but shader uniform expects %s.", type_name, Variant::get_type_name(p_target_type));
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

bool AIV1ShaderEditingService::_convert_value_for_target_type(const Variant &p_value, Variant::Type p_target_type, Variant &r_value, String &r_error) const {
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

bool AIV1ShaderEditingService::_convert_value_for_shader_uniform(const Variant &p_value, const PropertyInfo &p_uniform_info, Variant &r_value, String &r_error) const {
	if (p_uniform_info.type == Variant::OBJECT && p_uniform_info.hint == PROPERTY_HINT_RESOURCE_TYPE) {
		if (p_value.get_type() == Variant::NIL) {
			r_value = Variant();
			return true;
		}

		Ref<Resource> resource_value = p_value;
		if (resource_value.is_valid()) {
			const String resource_type = resource_value->get_class();
			if (!_is_resource_type_allowed(resource_type, p_uniform_info.hint_string)) {
				r_error = vformat("Resource type `%s` is not allowed for this shader uniform. Expected: %s.", resource_type, p_uniform_info.hint_string);
				return false;
			}
			r_value = resource_value;
			return true;
		}

		if (p_value.get_type() != Variant::DICTIONARY) {
			r_error = "Resource shader uniforms expect null, an existing Resource, or a dictionary with resource_type/resource_path.";
			return false;
		}

		Dictionary dict = p_value;
		if (dict.has("value")) {
			return _convert_value_for_shader_uniform(dict["value"], p_uniform_info, r_value, r_error);
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
			if (!_is_resource_type_allowed(resource->get_class(), p_uniform_info.hint_string)) {
				r_error = vformat("Loaded resource type `%s` is not allowed for this shader uniform. Expected: %s.", resource->get_class(), p_uniform_info.hint_string);
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
			if (!_is_resource_type_allowed(resource_type, p_uniform_info.hint_string)) {
				r_error = vformat("Resource type `%s` is not allowed for this shader uniform. Expected: %s.", resource_type, p_uniform_info.hint_string);
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

	return _convert_value_for_target_type(p_value, p_uniform_info.type, r_value, r_error);
}

bool AIV1ShaderEditingService::_find_shader_uniform_info(const Ref<Shader> &p_shader, const StringName &p_parameter_name, PropertyInfo &r_uniform_info) const {
	if (p_shader.is_null()) {
		return false;
	}

	List<PropertyInfo> uniforms;
	p_shader->get_shader_uniform_list(&uniforms);
	for (const PropertyInfo &E : uniforms) {
		if (E.usage == PROPERTY_USAGE_GROUP || E.usage == PROPERTY_USAGE_SUBGROUP) {
			continue;
		}
		if (StringName(E.name) == p_parameter_name) {
			r_uniform_info = E;
			return true;
		}
	}
	return false;
}

bool AIV1ShaderEditingService::_apply_shader_parameters_to_material(const Ref<ShaderMaterial> &p_material, const Dictionary &p_shader_parameters, Array &r_applied_parameters, String &r_error) const {
	if (p_material.is_null()) {
		r_error = "ShaderMaterial is invalid.";
		return false;
	}

	Ref<Shader> shader = p_material->get_shader();
	if (shader.is_null()) {
		r_error = "ShaderMaterial has no shader. Use shader.apply_to_node first or assign a ShaderMaterial with a shader.";
		return false;
	}

	for (const KeyValue<Variant, Variant> &E : p_shader_parameters) {
		const String parameter_name = String(E.key).strip_edges();
		if (parameter_name.is_empty()) {
			r_error = "Shader uniform name cannot be empty.";
			return false;
		}

		PropertyInfo uniform_info;
		if (!_find_shader_uniform_info(shader, StringName(parameter_name), uniform_info)) {
			r_error = vformat("Shader uniform `%s` was not found on the material shader.", parameter_name);
			return false;
		}

		Variant converted_value;
		if (!_convert_value_for_shader_uniform(E.value, uniform_info, converted_value, r_error)) {
			r_error = vformat("Failed to convert shader uniform `%s`: %s", parameter_name, r_error);
			return false;
		}

		p_material->set_shader_parameter(StringName(parameter_name), converted_value);

		Dictionary parameter_metadata;
		parameter_metadata["name"] = parameter_name;
		parameter_metadata["uniform_type"] = Variant::get_type_name(uniform_info.type);
		parameter_metadata["value_type"] = Variant::get_type_name(converted_value.get_type());
		Ref<Resource> resource = converted_value;
		if (resource.is_valid()) {
			parameter_metadata["resource_type"] = resource->get_class();
		}
		r_applied_parameters.push_back(parameter_metadata);
	}

	return true;
}

bool AIV1ShaderEditingService::_resolve_shader_target(Node *p_node, const String &p_target_property, Vector<StringName> &r_property_names,
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

bool AIV1ShaderEditingService::_save_current_scene_main_thread(Node *p_scene, String &r_saved_path, String &r_error) const {
	return _save_current_scene_with_editor_main_thread(p_scene, "The current scene must be saved before shader material bindings can be persisted.", r_saved_path, r_error);
}

AIV1ShaderEditingResult AIV1ShaderEditingService::_create_shader_main_thread(const String &p_shader_path, const String &p_shader_type, const String &p_shader_code, bool p_overwrite) {
	AIV1ShaderEditingResult result;
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
		if (!_load_shader_resource(shader_path, shader, error)) {
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

AIV1ShaderEditingResult AIV1ShaderEditingService::_edit_shader_main_thread(const String &p_shader_path, const String &p_shader_code) {
	AIV1ShaderEditingResult result;
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
	if (!_load_shader_resource(shader_path, shader, error)) {
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

AIV1ShaderEditingResult AIV1ShaderEditingService::_delete_shader_main_thread(const String &p_shader_path) {
	AIV1ShaderEditingResult result;
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

AIV1ShaderEditingResult AIV1ShaderEditingService::_apply_to_node_main_thread(const String &p_node_path, const String &p_shader_path,
		const String &p_target_property, const Dictionary &p_shader_parameters) {
	AIV1ShaderEditingResult result;
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
	Array applied_parameters;
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
		if (!_apply_shader_parameters_to_material(material, p_shader_parameters, applied_parameters, error)) {
			result.error = error;
			return result;
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
	result.metadata["shader_parameters"] = applied_parameters;
	result.metadata["scene_path"] = saved_path;
	result.metadata["saved"] = true;
	return result;
}

AIV1ShaderEditingResult AIV1ShaderEditingService::_set_parameters_main_thread(const String &p_node_path, const String &p_target_property, const Dictionary &p_shader_parameters) {
	AIV1ShaderEditingResult result;
	String error;

	if (p_shader_parameters.is_empty()) {
		result.error = "shader_parameters must include at least one uniform value.";
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
	if (target_kind != SHADER_TARGET_SHADER_MATERIAL) {
		result.error = "shader.set_parameters requires a target_property that contains a ShaderMaterial or Material.";
		return result;
	}

	Ref<ShaderMaterial> material = old_value;
	if (material.is_null()) {
		result.error = "target_property must currently contain a ShaderMaterial. Use shader.apply_to_node first.";
		return result;
	}

	Ref<Resource> duplicated = material->duplicate(true);
	Ref<ShaderMaterial> new_material = duplicated;
	if (new_material.is_null()) {
		result.error = "Failed to duplicate the target ShaderMaterial.";
		return result;
	}

	Array applied_parameters;
	if (!_apply_shader_parameters_to_material(new_material, p_shader_parameters, applied_parameters, error)) {
		result.error = error;
		return result;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (!undo_redo) {
		result.error = "Editor undo/redo manager is not available.";
		return result;
	}
	if (scene->get_scene_file_path().is_empty()) {
		result.error = "The current scene must be saved before shader parameter changes can be persisted.";
		return result;
	}

	NodePath property_path(Vector<StringName>(), property_names, false);
	Variant new_value = new_material;
	undo_redo->create_action_for_history(TTR("AI Set Shader Parameters"), EditorNode::get_editor_data().get_current_edited_scene_history_id(), UndoRedo::MERGE_DISABLE, false, true);
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
		result.metadata["target_property"] = p_target_property;
		return result;
	}

	result.success = true;
	result.message = vformat("Set %d shader parameter(s) on `%s:%s` and saved `%s`.", p_shader_parameters.size(), p_node_path, p_target_property, saved_path);
	result.metadata["node_path"] = p_node_path;
	result.metadata["target_property"] = p_target_property;
	result.metadata["shader_parameter_count"] = p_shader_parameters.size();
	result.metadata["shader_parameters"] = applied_parameters;
	result.metadata["scene_path"] = saved_path;
	result.metadata["saved"] = true;
	return result;
}

AIV1ShaderEditingResult AIV1ShaderEditingService::create_shader(const String &p_shader_path, const String &p_shader_type, const String &p_shader_code, bool p_overwrite) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_CREATE_SHADER;
	request.shader_path = p_shader_path;
	request.shader_type = p_shader_type;
	request.shader_code = p_shader_code;
	request.overwrite = p_overwrite;
	return _dispatch_to_main_thread(request);
}

AIV1ShaderEditingResult AIV1ShaderEditingService::edit_shader(const String &p_shader_path, const String &p_shader_code) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_EDIT_SHADER;
	request.shader_path = p_shader_path;
	request.shader_code = p_shader_code;
	return _dispatch_to_main_thread(request);
}

AIV1ShaderEditingResult AIV1ShaderEditingService::delete_shader(const String &p_shader_path) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_DELETE_SHADER;
	request.shader_path = p_shader_path;
	return _dispatch_to_main_thread(request);
}

AIV1ShaderEditingResult AIV1ShaderEditingService::apply_to_node(const String &p_node_path, const String &p_shader_path, const String &p_target_property,
		const Dictionary &p_shader_parameters) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_APPLY_TO_NODE;
	request.node_path = p_node_path;
	request.shader_path = p_shader_path;
	request.target_property = p_target_property;
	request.shader_parameters = p_shader_parameters;
	return _dispatch_to_main_thread(request);
}

AIV1ShaderEditingResult AIV1ShaderEditingService::set_parameters(const String &p_node_path, const String &p_target_property, const Dictionary &p_shader_parameters) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_SET_PARAMETERS;
	request.node_path = p_node_path;
	request.target_property = p_target_property;
	request.shader_parameters = p_shader_parameters;
	return _dispatch_to_main_thread(request);
}
