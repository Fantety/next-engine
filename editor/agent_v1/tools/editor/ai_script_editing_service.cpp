/**************************************************************************/
/*  ai_script_editing_service.cpp                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
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
#include "editor/agent_v1/tools/editor/ai_change_set_store.h"
#include "editor/agent_v1/tools/editor/ai_diff_service.h"
#include "editor/agent_v1/tools/project/ai_project_tool_utils.h"
#include "editor/docks/inspector_dock.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/editor_data.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/next_file_logger.h"
#include "editor/scene/editor_scene_tabs.h"
#include "editor/scene/scene_tree_editor.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"

#include "modules/gdscript/gdscript_parser.h"

static String _ai_indent_function_source(const String &p_source) {
	PackedStringArray lines = p_source.replace("\r\n", "\n").replace("\r", "\n").split("\n");
	for (int i = 0; i < lines.size(); i++) {
		if (!lines[i].strip_edges().is_empty()) {
			lines.write[i] = lines[i].rstrip("\t ");
		}
	}
	return String("\n").join(lines).strip_edges();
}

static void _ai_register_script_review_change(const String &p_title, const String &p_path, const String &p_change_type, const String &p_old_text, const String &p_new_text, Dictionary &r_metadata) {
	Ref<AIV1ToolExecutionState> context = AIV1ToolExecutionState::get_current();
	if (context.is_null() || !context->should_review_changes()) {
		return;
	}

	Array changes;
	changes.push_back(AIDiffService::build_text_change(p_path, p_change_type, p_old_text, p_new_text, "gdscript"));
	Ref<AIChangeSetStore> store = AIChangeSetStore::get_singleton();
	const String change_set_id = store->add_change_set(p_title, context->get_session_id(), context->get_tool_call_id(), changes);
	if (change_set_id.is_empty()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Review] Skipped script change. path=%s type=%s", p_path, p_change_type));
		return;
	}
	r_metadata["review_change_set_id"] = change_set_id;
	r_metadata["review_status"] = "pending";
	r_metadata["review_mode"] = true;
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Review] Recorded script change. id=%s path=%s type=%s", change_set_id, p_path, p_change_type));
}

void AIV1ScriptEditingService::_bind_methods() {
}

AIV1ScriptEditingService *AIV1ScriptEditingService::get_dispatcher_singleton() {
	static Ref<AIV1ScriptEditingService> dispatcher;
	if (dispatcher.is_null()) {
		dispatcher.instantiate();
	}
	return dispatcher.ptr();
}

AIV1ScriptEditingResult AIV1ScriptEditingService::_dispatch_to_main_thread(MainThreadRequest &r_request) {
	r_request.execution_context = AIV1ToolExecutionState::get_current();
	return _dispatch_main_thread_request<AIV1ScriptEditingResult>(r_request, get_dispatcher_singleton(), &AIV1ScriptEditingService::_execute_request, request_mutex, "Failed to schedule script editing on the main thread.");
}

void AIV1ScriptEditingService::_execute_request(uint64_t p_request_ptr) {
	_execute_request_ptr(reinterpret_cast<MainThreadRequest *>(p_request_ptr));
}

void AIV1ScriptEditingService::_execute_request_ptr(MainThreadRequest *p_request) {
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

	if (previous_context.is_valid()) {
		AIV1ToolExecutionState::set_current(previous_context);
	} else {
		AIV1ToolExecutionState::clear_current();
	}

	p_request->done.post();
}

bool AIV1ScriptEditingService::_is_allowed_script_path(const String &p_path, String &r_error) const {
	const String path = p_path.strip_edges();
	if (path.is_empty()) {
		r_error = "Script path is required.";
		return false;
	}
	if (!AIV1ProjectToolUtils::is_allowed_path(path)) {
		r_error = "Script path is outside the allowed project boundary.";
		return false;
	}
	if (path.get_extension().to_lower() != "gd") {
		r_error = "Only GDScript `.gd` files are supported by this tool.";
		return false;
	}
	return true;
}

bool AIV1ScriptEditingService::_read_text_file(const String &p_path, String &r_content, String &r_error) const {
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		r_error = vformat("Failed to open `%s` for reading (error %d).", p_path, err);
		return false;
	}
	r_content = file->get_as_text();
	return true;
}

bool AIV1ScriptEditingService::_write_text_file(const String &p_path, const String &p_content, String &r_error) const {
	if (!_ensure_project_parent_directory(p_path, "script", r_error)) {
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

bool AIV1ScriptEditingService::_parse_gdscript(const String &p_path, const String &p_source, Dictionary &r_metadata, String &r_error) const {
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

bool AIV1ScriptEditingService::_build_script_source(const String &p_extends, const String &p_source, String &r_source, String &r_error) const {
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

bool AIV1ScriptEditingService::_replace_function_source(const String &p_source, const String &p_function_name, const String &p_function_source, bool p_create_if_missing, String &r_new_source, Dictionary &r_metadata, String &r_error) const {
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

bool AIV1ScriptEditingService::_save_current_scene_main_thread(Node *p_scene, String &r_saved_path, String &r_error) const {
	return _save_current_scene_with_packed_scene_main_thread(p_scene, "The current scene must be saved before script bindings can be persisted.", r_saved_path, r_error);
}

AIV1ScriptEditingResult AIV1ScriptEditingService::_bind_to_node_main_thread(const String &p_node_path, const String &p_script_path) {
	AIV1ScriptEditingResult result;
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

AIV1ScriptEditingResult AIV1ScriptEditingService::_unbind_from_node_main_thread(const String &p_node_path) {
	AIV1ScriptEditingResult result;
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

AIV1ScriptEditingResult AIV1ScriptEditingService::inspect_script(const String &p_path) {
	AIV1ScriptEditingResult result;
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

	Dictionary parsed_metadata;
	if (!_parse_gdscript(p_path, source, parsed_metadata, error)) {
		result.error = error;
		return result;
	}

	result.success = true;
	result.message = vformat("Inspected script `%s`. Functions: %d.", p_path, int(parsed_metadata.get("function_count", 0)));
	result.metadata = parsed_metadata;
	return result;
}

AIV1ScriptEditingResult AIV1ScriptEditingService::_create_script_main_thread(const String &p_path, const String &p_extends, const String &p_source, bool p_overwrite) {
	AIV1ScriptEditingResult result;
	String error;
	if (!_is_allowed_script_path(p_path, error)) {
		result.error = error;
		return result;
	}
	const bool existed_before = FileAccess::exists(p_path);
	String old_source;
	if (existed_before && !_read_text_file(p_path, old_source, error)) {
		result.error = error;
		return result;
	}
	if (existed_before && !p_overwrite) {
		result.error = vformat("Script `%s` already exists. Set overwrite=true to replace it.", p_path);
		return result;
	}

	String source;
	if (!_build_script_source(p_extends, p_source, source, error)) {
		result.error = error;
		return result;
	}
	Dictionary parsed_metadata;
	if (!_parse_gdscript(p_path, source, parsed_metadata, error)) {
		result.error = error;
		return result;
	}
	if (!_write_text_file(p_path, source, error)) {
		result.error = error;
		return result;
	}

	result.success = true;
	result.message = vformat("Created script `%s`.", p_path);
	result.metadata = parsed_metadata;
	result.metadata["path"] = p_path;
	result.metadata["created"] = !existed_before;
	result.metadata["overwritten"] = existed_before && p_overwrite;
	_ai_register_script_review_change(result.message, p_path, existed_before ? String("modify") : String("create"), old_source, source, result.metadata);
	return result;
}

AIV1ScriptEditingResult AIV1ScriptEditingService::_write_script_main_thread(const String &p_path, const String &p_source, bool p_overwrite) {
	AIV1ScriptEditingResult result;
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
	String old_source;
	if (existed_before && !_read_text_file(p_path, old_source, error)) {
		result.error = error;
		return result;
	}
	if (existed_before && !p_overwrite) {
		result.error = vformat("Script `%s` already exists. Set overwrite=true to replace it.", p_path);
		return result;
	}

	String source = p_source.strip_edges() + "\n";
	Dictionary parsed_metadata;
	if (!_parse_gdscript(p_path, source, parsed_metadata, error)) {
		result.error = error;
		return result;
	}
	if (!_write_text_file(p_path, source, error)) {
		result.error = error;
		return result;
	}

	result.success = true;
	result.message = vformat("Wrote script `%s`.", p_path);
	result.metadata = parsed_metadata;
	result.metadata["path"] = p_path;
	result.metadata["created"] = !existed_before;
	result.metadata["overwritten"] = existed_before;
	_ai_register_script_review_change(result.message, p_path, existed_before ? String("modify") : String("create"), old_source, source, result.metadata);
	return result;
}

AIV1ScriptEditingResult AIV1ScriptEditingService::_patch_function_main_thread(const String &p_path, const String &p_function_name, const String &p_function_source, bool p_create_if_missing) {
	AIV1ScriptEditingResult result;
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

	Dictionary parsed_metadata;
	if (!_parse_gdscript(p_path, new_source, parsed_metadata, error)) {
		result.error = error;
		return result;
	}
	if (!_write_text_file(p_path, new_source, error)) {
		result.error = error;
		return result;
	}

	result.success = true;
	result.message = vformat("Patched function `%s` in `%s`.", p_function_name, p_path);
	result.metadata = parsed_metadata;
	result.metadata["path"] = p_path;
	result.metadata["function_name"] = p_function_name;
	result.metadata["patch"] = patch_metadata;
	_ai_register_script_review_change(result.message, p_path, "modify", source, new_source, result.metadata);
	return result;
}

AIV1ScriptEditingResult AIV1ScriptEditingService::_delete_script_main_thread(const String &p_path) {
	AIV1ScriptEditingResult result;
	String error;
	if (!_is_allowed_script_path(p_path, error)) {
		result.error = error;
		return result;
	}
	if (!FileAccess::exists(p_path)) {
		result.error = vformat("Script `%s` does not exist.", p_path);
		return result;
	}
	String old_source;
	if (!_read_text_file(p_path, old_source, error)) {
		result.error = error;
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
	_ai_register_script_review_change(result.message, p_path, "delete", old_source, String(), result.metadata);
	return result;
}

AIV1ScriptEditingResult AIV1ScriptEditingService::create_script(const String &p_path, const String &p_extends, const String &p_source, bool p_overwrite) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_CREATE_SCRIPT;
	request.path = p_path;
	request.extends_type = p_extends;
	request.source = p_source;
	request.overwrite = p_overwrite;
	return _dispatch_to_main_thread(request);
}

AIV1ScriptEditingResult AIV1ScriptEditingService::write_script(const String &p_path, const String &p_source, bool p_overwrite) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_WRITE_SCRIPT;
	request.path = p_path;
	request.source = p_source;
	request.overwrite = p_overwrite;
	return _dispatch_to_main_thread(request);
}

AIV1ScriptEditingResult AIV1ScriptEditingService::patch_function(const String &p_path, const String &p_function_name, const String &p_function_source, bool p_create_if_missing) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_PATCH_FUNCTION;
	request.path = p_path;
	request.function_name = p_function_name;
	request.source = p_function_source;
	request.create_if_missing = p_create_if_missing;
	return _dispatch_to_main_thread(request);
}

AIV1ScriptEditingResult AIV1ScriptEditingService::delete_script(const String &p_path) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_DELETE_SCRIPT;
	request.path = p_path;
	return _dispatch_to_main_thread(request);
}

AIV1ScriptEditingResult AIV1ScriptEditingService::bind_to_node(const String &p_node_path, const String &p_script_path) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_BIND_TO_NODE;
	request.node_path = p_node_path;
	request.script_path = p_script_path;
	return _dispatch_to_main_thread(request);
}

AIV1ScriptEditingResult AIV1ScriptEditingService::unbind_from_node(const String &p_node_path) {
	MainThreadRequest request;
	request.operation = MainThreadRequest::OP_UNBIND_FROM_NODE;
	request.node_path = p_node_path;
	return _dispatch_to_main_thread(request);
}
