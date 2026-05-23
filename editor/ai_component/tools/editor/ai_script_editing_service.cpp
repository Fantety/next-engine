/**************************************************************************/
/*  ai_script_editing_service.cpp                                         */
/**************************************************************************/

#include "ai_script_editing_service.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/object/script_language.h"
#include "core/os/thread.h"
#include "core/variant/variant.h"

#include "editor/ai_component/tools/project/ai_project_tool_utils.h"
#include "editor/docks/inspector_dock.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/editor_data.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/scene/scene_tree_editor.h"
#include "editor/scene/editor_scene_tabs.h"
#include "modules/gdscript/gdscript_parser.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"

static String _ai_indent_function_source(const String &p_source) {
	PackedStringArray lines = p_source.replace("\r\n", "\n").replace("\r", "\n").split("\n");
	for (int i = 0; i < lines.size(); i++) {
		if (!lines[i].strip_edges().is_empty()) {
			lines.write[i] = lines[i].rstrip("\t ");
		}
	}
	return String("\n").join(lines).strip_edges();
}

void AIScriptEditingService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update_scene_tree"), &AIScriptEditingService::_update_scene_tree);
}

AIScriptEditingService *AIScriptEditingService::get_dispatcher_singleton() {
	static Ref<AIScriptEditingService> dispatcher;
	if (dispatcher.is_null()) {
		dispatcher.instantiate();
	}
	return dispatcher.ptr();
}

AIScriptEditingResult AIScriptEditingService::_dispatch_to_main_thread(MainThreadRequest &r_request) {
	if (Thread::is_main_thread()) {
		_execute_request_ptr(&r_request);
		return r_request.result;
	}

	MutexLock lock(request_mutex);
	if (!MessageQueue::get_main_singleton()) {
		AIScriptEditingResult result;
		result.error = "Main thread dispatch is not available.";
		return result;
	}

	AIScriptEditingService *dispatcher = get_dispatcher_singleton();
	Variant request_ptr = reinterpret_cast<uint64_t>(&r_request);
	Error err = MessageQueue::get_main_singleton()->push_callable(callable_mp(dispatcher, &AIScriptEditingService::_execute_request), request_ptr);
	if (err != OK) {
		AIScriptEditingResult result;
		result.error = "Failed to schedule script editing on the main thread.";
		return result;
	}

	r_request.done.wait();
	return r_request.result;
}

void AIScriptEditingService::_execute_request(uint64_t p_request_ptr) {
	_execute_request_ptr(reinterpret_cast<MainThreadRequest *>(p_request_ptr));
}

void AIScriptEditingService::_execute_request_ptr(MainThreadRequest *p_request) {
	ERR_FAIL_NULL(p_request);

	switch (p_request->operation) {
		case MainThreadRequest::OP_CREATE_SCRIPT:
			p_request->result = _create_script_main_thread(p_request->path, p_request->extends_type, p_request->source, p_request->overwrite);
			break;
		case MainThreadRequest::OP_WRITE_SCRIPT:
			p_request->result = _write_script_main_thread(p_request->path, p_request->source, p_request->overwrite);
			break;
		case MainThreadRequest::OP_PATCH_FUNCTION:
			p_request->result = _patch_function_main_thread(p_request->path, p_request->function_name, p_request->source, p_request->create_if_missing);
			break;
		case MainThreadRequest::OP_DELETE_SCRIPT:
			p_request->result = _delete_script_main_thread(p_request->path);
			break;
		case MainThreadRequest::OP_BIND_TO_NODE:
			p_request->result = _bind_to_node_main_thread(p_request->node_path, p_request->script_path);
			break;
		case MainThreadRequest::OP_UNBIND_FROM_NODE:
			p_request->result = _unbind_from_node_main_thread(p_request->node_path);
			break;
	}

	p_request->done.post();
}

bool AIScriptEditingService::_is_allowed_script_path(const String &p_path, String &r_error) const {
	const String path = p_path.strip_edges();
	if (path.is_empty()) {
		r_error = "Script path is required.";
		return false;
	}
	if (!AIProjectToolUtils::is_allowed_path(path)) {
		r_error = "Script path is outside the allowed project boundary.";
		return false;
	}
	if (path.get_extension().to_lower() != "gd") {
		r_error = "Only GDScript `.gd` files are supported by this tool.";
		return false;
	}
	return true;
}

bool AIScriptEditingService::_read_text_file(const String &p_path, String &r_content, String &r_error) const {
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		r_error = vformat("Failed to open `%s` for reading (error %d).", p_path, err);
		return false;
	}
	r_content = file->get_as_text();
	return true;
}

bool AIScriptEditingService::_write_text_file(const String &p_path, const String &p_content, String &r_error) const {
	if (!_ensure_parent_directory(p_path, r_error)) {
		return false;
	}
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	if (file.is_null() || err != OK) {
		r_error = vformat("Failed to open `%s` for writing (error %d).", p_path, err);
		return false;
	}
	file->store_string(p_content);
	file->close();
	_refresh_file_system(p_path);
	return true;
}

bool AIScriptEditingService::_ensure_parent_directory(const String &p_path, String &r_error) const {
	const String base_dir = p_path.get_base_dir();
	if (base_dir.is_empty() || base_dir == "res://") {
		return true;
	}

	const String absolute_dir = ProjectSettings::get_singleton()->globalize_path(base_dir);
	Error err = DirAccess::make_dir_recursive_absolute(absolute_dir);
	if (err != OK) {
		r_error = vformat("Failed to create script directory `%s` (error %d).", base_dir, err);
		return false;
	}
	return true;
}

bool AIScriptEditingService::_parse_gdscript(const String &p_path, const String &p_source, Dictionary &r_metadata, String &r_error) const {
	GDScriptParser parser;
	Error err = parser.parse(p_source, p_path, false);
	if (err != OK) {
		String error_message = "GDScript parse failed.";
		if (!parser.get_errors().is_empty()) {
			const GDScriptParser::ParserError parse_error = parser.get_errors().front()->get();
			error_message = vformat("GDScript parse failed at line %d: %s", parse_error.start_line, parse_error.message);
		}
		r_error = error_message;
		return false;
	}

	const GDScriptParser::ClassNode *tree = parser.get_tree();
	Array functions;
	if (tree) {
		r_metadata["extends_path"] = tree->extends_path;
		for (int i = 0; i < tree->extends.size(); i++) {
			if (tree->extends[i]) {
				r_metadata["extends"] = String(tree->extends[i]->name);
			}
		}
		for (int i = 0; i < tree->members.size(); i++) {
			const GDScriptParser::ClassNode::Member &member = tree->members[i];
			if (member.type != GDScriptParser::ClassNode::Member::FUNCTION || !member.function || !member.function->identifier) {
				continue;
			}
			Dictionary function_info;
			function_info["name"] = String(member.function->identifier->name);
			function_info["start_line"] = member.function->start_line;
			function_info["end_line"] = member.function->end_line;
			function_info["is_static"] = member.function->is_static;
			function_info["is_abstract"] = member.function->is_abstract;
			functions.push_back(function_info);
		}
	}
	r_metadata["path"] = p_path;
	r_metadata["functions"] = functions;
	r_metadata["function_count"] = functions.size();
	return true;
}

bool AIScriptEditingService::_build_script_source(const String &p_extends, const String &p_source, String &r_source, String &r_error) const {
	const String source = p_source.strip_edges();
	if (!source.is_empty()) {
		r_source = source + "\n";
		return true;
	}

	const String extends_type = p_extends.strip_edges().is_empty() ? String("Node") : p_extends.strip_edges();
	if (extends_type.contains("\n") || extends_type.contains("\r")) {
		r_error = "Extends type must be a single line.";
		return false;
	}

	r_source = "extends " + extends_type + "\n\nfunc _ready() -> void:\n\tpass\n";
	return true;
}

bool AIScriptEditingService::_replace_function_source(const String &p_source, const String &p_function_name, const String &p_function_source, bool p_create_if_missing, String &r_new_source, Dictionary &r_metadata, String &r_error) const {
	const String function_name = p_function_name.strip_edges();
	if (function_name.is_empty()) {
		r_error = "Function name is required.";
		return false;
	}

	const String function_source = _ai_indent_function_source(p_function_source);
	if (function_source.is_empty()) {
		r_error = "Function source is required.";
		return false;
	}
	if (!function_source.begins_with("func ") && !function_source.begins_with("static func ")) {
		r_error = "Function source must start with `func` or `static func`.";
		return false;
	}

	GDScriptParser parser;
	Error err = parser.parse(p_source, "res://__ai_patch_preview.gd", false);
	if (err != OK) {
		r_error = "Existing script cannot be parsed before patching.";
		return false;
	}

	int start_line = -1;
	int end_line = -1;
	const GDScriptParser::ClassNode *tree = parser.get_tree();
	if (tree) {
		for (int i = 0; i < tree->members.size(); i++) {
			const GDScriptParser::ClassNode::Member &member = tree->members[i];
			if (member.type == GDScriptParser::ClassNode::Member::FUNCTION && member.function && member.function->identifier && String(member.function->identifier->name) == function_name) {
				start_line = member.function->start_line;
				end_line = member.function->end_line;
				break;
			}
		}
	}

	PackedStringArray lines = p_source.replace("\r\n", "\n").replace("\r", "\n").split("\n");
	if (start_line > 0 && end_line >= start_line) {
		PackedStringArray output;
		for (int i = 0; i < start_line - 1 && i < lines.size(); i++) {
			output.push_back(lines[i]);
		}
		PackedStringArray replacement = function_source.split("\n");
		for (int i = 0; i < replacement.size(); i++) {
			output.push_back(replacement[i]);
		}
		for (int i = end_line; i < lines.size(); i++) {
			output.push_back(lines[i]);
		}
		r_new_source = String("\n").join(output).strip_edges() + "\n";
		r_metadata["operation"] = "replace";
		r_metadata["start_line"] = start_line;
		r_metadata["end_line"] = end_line;
		return true;
	}

	if (!p_create_if_missing) {
		r_error = vformat("Function `%s` was not found.", function_name);
		return false;
	}

	String base = p_source.strip_edges();
	if (!base.is_empty()) {
		base += "\n\n";
	}
	r_new_source = base + function_source + "\n";
	r_metadata["operation"] = "append";
	return true;
}

Node *AIScriptEditingService::_get_edited_scene(String &r_error) const {
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

Node *AIScriptEditingService::_resolve_node_path(Node *p_scene_root, const String &p_path, bool p_allow_root, String &r_error) const {
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

void AIScriptEditingService::_refresh_file_system(const String &p_path) const {
	if (!Thread::is_main_thread()) {
		if (MessageQueue::get_main_singleton()) {
			MessageQueue::get_main_singleton()->push_callable(callable_mp(get_dispatcher_singleton(), &AIScriptEditingService::_refresh_file_system), p_path);
		}
		return;
	}
	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(p_path);
		EditorFileSystem::get_singleton()->scan_changes();
	}
}

void AIScriptEditingService::_update_scene_tree() const {
	SceneTreeDock *dock = SceneTreeDock::get_singleton();
	if (dock && dock->get_tree_editor()) {
		dock->get_tree_editor()->update_tree();
	}
}

bool AIScriptEditingService::_save_current_scene_main_thread(Node *p_scene, String &r_saved_path, String &r_error) const {
	ERR_FAIL_NULL_V(p_scene, false);

	EditorNode *editor = EditorNode::get_singleton();
	if (!editor) {
		r_error = "EditorNode is not available.";
		return false;
	}
	const String scene_path = p_scene->get_scene_file_path();
	if (scene_path.is_empty()) {
		r_error = "The current scene must be saved before script bindings can be persisted.";
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

AIScriptEditingResult AIScriptEditingService::_bind_to_node_main_thread(const String &p_node_path, const String &p_script_path) {
	AIScriptEditingResult result;
	String error;
	if (!_is_allowed_script_path(p_script_path, error)) {
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

	Error load_error = OK;
	Ref<Script> script = ResourceLoader::load(p_script_path, "Script", ResourceLoader::CACHE_MODE_REPLACE, &load_error);
	if (script.is_null() || load_error != OK) {
		result.error = vformat("Failed to load script `%s` (error %d).", p_script_path, load_error);
		return result;
	}
	if (!ClassDB::is_parent_class(node->get_class_name(), script->get_instance_base_type())) {
		result.error = vformat("Script `%s` extends `%s`, which is not compatible with node `%s` (%s).", p_script_path, script->get_instance_base_type(), p_node_path, node->get_class());
		return result;
	}

	Ref<Script> old_script = node->get_script();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (!undo_redo) {
		result.error = "Editor undo/redo manager is not available.";
		return result;
	}
	undo_redo->create_action(TTR("AI Attach Script"), UndoRedo::MERGE_DISABLE, node);
	if (InspectorDock::get_singleton()) {
		undo_redo->add_do_method(InspectorDock::get_singleton(), "store_script_properties", node);
		undo_redo->add_undo_method(InspectorDock::get_singleton(), "store_script_properties", node);
	}
	undo_redo->add_do_method(node, "set_script", script);
	undo_redo->add_undo_method(node, "set_script", old_script);
	if (InspectorDock::get_singleton()) {
		undo_redo->add_do_method(InspectorDock::get_singleton(), "apply_script_properties", node);
		undo_redo->add_undo_method(InspectorDock::get_singleton(), "apply_script_properties", node);
	}
	undo_redo->add_do_method(this, "_update_scene_tree");
	undo_redo->add_undo_method(this, "_update_scene_tree");
	undo_redo->commit_action();
	_update_scene_tree();

	String saved_path;
	if (!_save_current_scene_main_thread(scene, saved_path, error)) {
		result.error = error;
		result.metadata["saved"] = false;
		return result;
	}

	result.success = true;
	result.message = vformat("Bound script `%s` to node `%s` and saved `%s`.", p_script_path, p_node_path, saved_path);
	result.metadata["node_path"] = p_node_path;
	result.metadata["script_path"] = p_script_path;
	result.metadata["scene_path"] = saved_path;
	result.metadata["saved"] = true;
	return result;
}

AIScriptEditingResult AIScriptEditingService::_unbind_from_node_main_thread(const String &p_node_path) {
	AIScriptEditingResult result;
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

	Ref<Script> old_script = node->get_script();
	Ref<Script> empty = EditorNode::get_singleton()->get_object_custom_type_base(node);
	if (old_script == empty) {
		result.success = true;
		result.message = vformat("Node `%s` does not have a detachable script.", p_node_path);
		result.metadata["node_path"] = p_node_path;
		result.metadata["changed"] = false;
		return result;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (!undo_redo) {
		result.error = "Editor undo/redo manager is not available.";
		return result;
	}
	undo_redo->create_action(TTR("AI Detach Script"), UndoRedo::MERGE_DISABLE, node);
	undo_redo->add_do_method(node, "set_script", empty);
	undo_redo->add_undo_method(node, "set_script", old_script);
	List<PropertyInfo> properties;
	node->get_property_list(&properties);
	for (const PropertyInfo &property : properties) {
		if (property.usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR)) {
			undo_redo->add_undo_property(node, property.name, node->get(property.name));
		}
	}
	undo_redo->add_do_method(this, "_update_scene_tree");
	undo_redo->add_undo_method(this, "_update_scene_tree");
	undo_redo->commit_action();
	_update_scene_tree();

	String saved_path;
	if (!_save_current_scene_main_thread(scene, saved_path, error)) {
		result.error = error;
		result.metadata["saved"] = false;
		return result;
	}

	result.success = true;
	result.message = vformat("Unbound script from node `%s` and saved `%s`.", p_node_path, saved_path);
	result.metadata["node_path"] = p_node_path;
	result.metadata["scene_path"] = saved_path;
	result.metadata["saved"] = true;
	result.metadata["changed"] = true;
	return result;
}

AIScriptEditingResult AIScriptEditingService::inspect_script(const String &p_path) {
	AIScriptEditingResult result;
	String error;
	if (!_is_allowed_script_path(p_path, error)) {
		result.error = error;
		return result;
	}

	String source;
	if (!_read_text_file(p_path, source, error)) {
		result.error = error;
		return result;
	}

	Dictionary metadata;
	if (!_parse_gdscript(p_path, source, metadata, error)) {
		result.error = error;
		return result;
	}

	result.success = true;
	result.message = vformat("Inspected script `%s`. Functions: %d.", p_path, int(metadata.get("function_count", 0)));
	result.metadata = metadata;
	return result;
}

AIScriptEditingResult AIScriptEditingService::_create_script_main_thread(const String &p_path, const String &p_extends, const String &p_source, bool p_overwrite) {
	AIScriptEditingResult result;
	String error;
	if (!_is_allowed_script_path(p_path, error)) {
		result.error = error;
		return result;
	}
	const bool existed_before = FileAccess::exists(p_path);
	if (existed_before && !p_overwrite) {
		result.error = vformat("Script `%s` already exists. Set overwrite=true to replace it.", p_path);
		return result;
	}

	String source;
	if (!_build_script_source(p_extends, p_source, source, error)) {
		result.error = error;
		return result;
	}
	Dictionary metadata;
	if (!_parse_gdscript(p_path, source, metadata, error)) {
		result.error = error;
		return result;
	}
	if (!_write_text_file(p_path, source, error)) {
		result.error = error;
		return result;
	}

	result.success = true;
	result.message = vformat("Created script `%s`.", p_path);
	result.metadata = metadata;
	result.metadata["path"] = p_path;
	result.metadata["created"] = !existed_before;
	result.metadata["overwritten"] = existed_before && p_overwrite;
	return result;
}

AIScriptEditingResult AIScriptEditingService::_write_script_main_thread(const String &p_path, const String &p_source, bool p_overwrite) {
	AIScriptEditingResult result;
	String error;
	if (!_is_allowed_script_path(p_path, error)) {
		result.error = error;
		return result;
	}
	if (p_source.strip_edges().is_empty()) {
		result.error = "Script source is required.";
		return result;
	}
	const bool existed_before = FileAccess::exists(p_path);
	if (existed_before && !p_overwrite) {
		result.error = vformat("Script `%s` already exists. Set overwrite=true to replace it.", p_path);
		return result;
	}

	String source = p_source.strip_edges() + "\n";
	Dictionary metadata;
	if (!_parse_gdscript(p_path, source, metadata, error)) {
		result.error = error;
		return result;
	}
	if (!_write_text_file(p_path, source, error)) {
		result.error = error;
		return result;
	}

	result.success = true;
	result.message = vformat("Wrote script `%s`.", p_path);
	result.metadata = metadata;
	result.metadata["path"] = p_path;
	result.metadata["created"] = !existed_before;
	result.metadata["overwritten"] = existed_before;
	return result;
}

AIScriptEditingResult AIScriptEditingService::_patch_function_main_thread(const String &p_path, const String &p_function_name, const String &p_function_source, bool p_create_if_missing) {
	AIScriptEditingResult result;
	String error;
	if (!_is_allowed_script_path(p_path, error)) {
		result.error = error;
		return result;
	}

	String source;
	if (!_read_text_file(p_path, source, error)) {
		result.error = error;
		return result;
	}

	String new_source;
	Dictionary patch_metadata;
	if (!_replace_function_source(source, p_function_name, p_function_source, p_create_if_missing, new_source, patch_metadata, error)) {
		result.error = error;
		return result;
	}

	Dictionary metadata;
	if (!_parse_gdscript(p_path, new_source, metadata, error)) {
		result.error = error;
		return result;
	}
	if (!_write_text_file(p_path, new_source, error)) {
		result.error = error;
		return result;
	}

	result.success = true;
	result.message = vformat("Patched function `%s` in `%s`.", p_function_name, p_path);
	result.metadata = metadata;
	result.metadata["path"] = p_path;
	result.metadata["function_name"] = p_function_name;
	result.metadata["patch"] = patch_metadata;
	return result;
}

AIScriptEditingResult AIScriptEditingService::_delete_script_main_thread(const String &p_path) {
	AIScriptEditingResult result;
	String error;
	if (!_is_allowed_script_path(p_path, error)) {
		result.error = error;
		return result;
	}
	if (!FileAccess::exists(p_path)) {
		result.error = vformat("Script `%s` does not exist.", p_path);
		return result;
	}

	Error err = DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(p_path));
	if (err != OK) {
		result.error = vformat("Failed to delete script `%s` (error %d).", p_path, err);
		return result;
	}
	_refresh_file_system(p_path);

	result.success = true;
	result.message = vformat("Deleted script `%s`.", p_path);
	result.metadata["path"] = p_path;
	result.metadata["deleted"] = true;
	return result;
}

AIScriptEditingResult AIScriptEditingService::create_script(const String &p_path, const String &p_extends, const String &p_source, bool p_overwrite) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_CREATE_SCRIPT;
	request.path = p_path;
	request.extends_type = p_extends;
	request.source = p_source;
	request.overwrite = p_overwrite;
	return _dispatch_to_main_thread(request);
}

AIScriptEditingResult AIScriptEditingService::write_script(const String &p_path, const String &p_source, bool p_overwrite) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_WRITE_SCRIPT;
	request.path = p_path;
	request.source = p_source;
	request.overwrite = p_overwrite;
	return _dispatch_to_main_thread(request);
}

AIScriptEditingResult AIScriptEditingService::patch_function(const String &p_path, const String &p_function_name, const String &p_function_source, bool p_create_if_missing) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_PATCH_FUNCTION;
	request.path = p_path;
	request.function_name = p_function_name;
	request.source = p_function_source;
	request.create_if_missing = p_create_if_missing;
	return _dispatch_to_main_thread(request);
}

AIScriptEditingResult AIScriptEditingService::delete_script(const String &p_path) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_DELETE_SCRIPT;
	request.path = p_path;
	return _dispatch_to_main_thread(request);
}

AIScriptEditingResult AIScriptEditingService::bind_to_node(const String &p_node_path, const String &p_script_path) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_BIND_TO_NODE;
	request.node_path = p_node_path;
	request.script_path = p_script_path;
	return _dispatch_to_main_thread(request);
}

AIScriptEditingResult AIScriptEditingService::unbind_from_node(const String &p_node_path) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_UNBIND_FROM_NODE;
	request.node_path = p_node_path;
	return _dispatch_to_main_thread(request);
}
