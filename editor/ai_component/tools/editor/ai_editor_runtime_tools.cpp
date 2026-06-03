/**************************************************************************/
/*  ai_editor_runtime_tools.cpp                                           */
/**************************************************************************/

#include "ai_editor_runtime_tools.h"

#include "core/io/file_access.h"
#include "core/object/callable_mp.h"
#include "core/object/message_queue.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/variant/variant.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/run/editor_run_bar.h"

namespace {

struct EditorRuntimeToolResult {
	bool success = false;
	String error;
	String message;
	Dictionary metadata;
};

struct EditorRuntimeToolRequest {
	enum Operation {
		OP_RUN_SCENE,
		OP_STOP_RUNNING_SCENE,
		OP_GET_TERMINAL_ERRORS,
	};

	Operation operation = OP_RUN_SCENE;
	String mode;
	String scene_path;
	Vector<String> play_args;
	bool include_warnings = true;
	int max_entries = 20;
	EditorRuntimeToolResult result;
	Semaphore done;
};

void _finish_runtime_request(EditorRuntimeToolRequest *p_request) {
	if (p_request) {
		p_request->done.post();
	}
}

EditorRuntimeToolResult _make_error(const String &p_error) {
	EditorRuntimeToolResult result;
	result.error = p_error;
	return result;
}

String _normalize_scene_path(const String &p_path) {
	return p_path.strip_edges().replace_char('\\', '/').simplify_path();
}

void _execute_runtime_request(uint64_t p_request_ptr) {
	EditorRuntimeToolRequest *request = reinterpret_cast<EditorRuntimeToolRequest *>(p_request_ptr);
	if (!request) {
		return;
	}

	EditorRunBar *run_bar = EditorRunBar::get_singleton();
	if (!run_bar) {
		request->result = _make_error("Editor run bar is not available.");
		_finish_runtime_request(request);
		return;
	}

	switch (request->operation) {
		case EditorRuntimeToolRequest::OP_RUN_SCENE: {
			const String mode = request->mode.strip_edges().to_lower();
			if (mode == "main") {
				run_bar->play_main_scene(false, request->play_args);
			} else if (mode == "current") {
				run_bar->play_current_scene(false, request->play_args);
			} else if (mode == "custom") {
				const String scene_path = _normalize_scene_path(request->scene_path);
				if (scene_path.is_empty()) {
					request->result = _make_error("scene_path is required when mode is custom.");
					_finish_runtime_request(request);
					return;
				}
				if (!scene_path.begins_with("res://")) {
					request->result = _make_error("scene_path must be a res:// project path.");
					_finish_runtime_request(request);
					return;
				}
				if (!FileAccess::exists(scene_path)) {
					request->result = _make_error("scene_path does not exist.");
					_finish_runtime_request(request);
					return;
				}
				run_bar->play_custom_scene(scene_path, request->play_args);
			} else {
				request->result = _make_error("mode must be one of: main, current, custom.");
				_finish_runtime_request(request);
				return;
			}

			Dictionary metadata;
			metadata["mode"] = mode;
			metadata["scene_path"] = run_bar->get_playing_scene();
			metadata["is_playing"] = run_bar->is_playing();
			request->result.success = true;
			request->result.metadata = metadata;
			request->result.message = run_bar->is_playing() ? "Scene run requested." : "Scene run request completed, but no scene is currently playing.";
		} break;

		case EditorRuntimeToolRequest::OP_STOP_RUNNING_SCENE: {
			const bool was_playing = run_bar->is_playing();
			const String playing_scene = run_bar->get_playing_scene();
			run_bar->stop_playing();

			Dictionary metadata;
			metadata["was_playing"] = was_playing;
			metadata["scene_path"] = playing_scene;
			metadata["is_playing"] = run_bar->is_playing();
			request->result.success = true;
			request->result.metadata = metadata;
			request->result.message = was_playing ? "Running scene stopped." : "No scene was running.";
		} break;

		case EditorRuntimeToolRequest::OP_GET_TERMINAL_ERRORS: {
			EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
			if (!debugger_node) {
				request->result = _make_error("Editor debugger is not available.");
				break;
			}

			ScriptEditorDebugger *debugger = debugger_node->get_current_debugger();
			if (!debugger) {
				debugger = debugger_node->get_default_debugger();
			}
			if (!debugger) {
				request->result = _make_error("Script debugger is not available.");
				break;
			}

			const Array entries = debugger->get_error_messages_snapshot(request->max_entries, request->include_warnings);
			Dictionary metadata;
			metadata["entry_count"] = entries.size();
			metadata["error_count"] = debugger->get_error_count();
			metadata["warning_count"] = debugger->get_warning_count();
			metadata["include_warnings"] = request->include_warnings;
			metadata["max_entries"] = request->max_entries;

			String content;
			if (entries.is_empty()) {
				content = "No terminal errors found.";
			} else {
				content = "Terminal Errors\n";
				for (int i = 0; i < entries.size(); i++) {
					Dictionary entry = entries[i];
					content += vformat("- [%s] %s %s\n", String(entry.get("type", "error")), String(entry.get("time", "")), String(entry.get("message", "")));
					Array details = entry.get("details", Array());
					for (int j = 0; j < details.size(); j++) {
						content += "  " + String(details[j]) + "\n";
					}
				}
			}

			request->result.success = true;
			request->result.metadata = metadata;
			request->result.metadata["entries"] = entries;
			request->result.message = content.strip_edges();
		} break;
	}

	_finish_runtime_request(request);
}

EditorRuntimeToolResult _dispatch_runtime_request(EditorRuntimeToolRequest &r_request) {
	if (Thread::is_main_thread()) {
		_execute_runtime_request(reinterpret_cast<uint64_t>(&r_request));
		return r_request.result;
	}

	CallQueue *message_queue = MessageQueue::get_main_singleton();
	if (!message_queue) {
		return _make_error("Main thread dispatch is not available.");
	}

	const Variant request_ptr = reinterpret_cast<uint64_t>(&r_request);
	const Error err = message_queue->push_callable(callable_mp_static(_execute_runtime_request), request_ptr);
	if (err != OK) {
		return _make_error("Failed to schedule editor runtime operation on the main thread.");
	}

	r_request.done.wait();
	return r_request.result;
}

Array _read_string_array_argument(const Dictionary &p_arguments, const StringName &p_name, String &r_error) {
	Array values;
	if (!p_arguments.has(p_name)) {
		return values;
	}

	Variant raw_value = p_arguments[p_name];
	if (raw_value.get_type() != Variant::ARRAY) {
		r_error = vformat("%s must be an array of strings.", String(p_name));
		return values;
	}

	Array raw_array = raw_value;
	for (int i = 0; i < raw_array.size(); i++) {
		if (raw_array[i].get_type() != Variant::STRING) {
			r_error = vformat("%s must contain only strings.", String(p_name));
			return Array();
		}
		values.push_back(String(raw_array[i]));
	}
	return values;
}

void _apply_tool_result(AIToolResult &r_tool_result, const EditorRuntimeToolResult &p_result) {
	r_tool_result.metadata = p_result.metadata;
	if (!p_result.success) {
		r_tool_result.error = p_result.error;
		return;
	}
	r_tool_result.content = p_result.message;
}

} // namespace

String AIEditorRunSceneTool::get_name() const {
	return "editor.run_scene";
}

String AIEditorRunSceneTool::get_description() const {
	return "Runs a project scene through the Godot editor run bar. Supports main, current, and custom scene modes.";
}

Dictionary AIEditorRunSceneTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary mode_property;
	mode_property["type"] = "string";
	Array mode_enum;
	mode_enum.push_back("current");
	mode_enum.push_back("main");
	mode_enum.push_back("custom");
	mode_property["enum"] = mode_enum;
	mode_property["description"] = "Scene run mode. Use current for the edited scene, main for the project main scene, custom for scene_path.";
	properties["mode"] = mode_property;

	Dictionary scene_path_property;
	scene_path_property["type"] = "string";
	scene_path_property["description"] = "Required when mode is custom. Must be a res:// scene path.";
	properties["scene_path"] = scene_path_property;

	Dictionary play_args_property;
	play_args_property["type"] = "array";
	Dictionary play_arg_items;
	play_arg_items["type"] = "string";
	play_args_property["items"] = play_arg_items;
	play_args_property["description"] = "Optional command-line arguments passed to the running scene.";
	properties["play_args"] = play_args_property;

	Array required;
	required.push_back("mode");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIEditorRunSceneTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String mode = String(p_arguments.get("mode", "current")).strip_edges();
	const String scene_path = String(p_arguments.get("scene_path", "")).strip_edges();
	print_line(vformat("[AI Agent][Tool:editor.run_scene] Start. mode=%s scene_path=%s", mode, scene_path));

	String args_error;
	Array play_args_array = _read_string_array_argument(p_arguments, "play_args", args_error);
	if (!args_error.is_empty()) {
		result.error = args_error;
		print_line(vformat("[AI Agent][Tool:editor.run_scene] Failed: %s", result.error));
		return result;
	}

	EditorRuntimeToolRequest request;
	request.operation = EditorRuntimeToolRequest::OP_RUN_SCENE;
	request.mode = mode;
	request.scene_path = scene_path;
	for (int i = 0; i < play_args_array.size(); i++) {
		request.play_args.push_back(String(play_args_array[i]));
	}

	_apply_tool_result(result, _dispatch_runtime_request(request));
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:editor.run_scene] Failed: %s", result.error));
	} else {
		print_line(vformat("[AI Agent][Tool:editor.run_scene] Completed. %s", result.content));
	}
	return result;
}

String AIEditorStopRunningSceneTool::get_name() const {
	return "editor.stop_running_scene";
}

String AIEditorStopRunningSceneTool::get_description() const {
	return "Stops the scene currently running from the Godot editor.";
}

Dictionary AIEditorStopRunningSceneTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";
	schema["properties"] = Dictionary();
	return schema;
}

AIToolResult AIEditorStopRunningSceneTool::execute(const Dictionary &p_arguments) {
	(void)p_arguments;
	AIToolResult result;
	print_line("[AI Agent][Tool:editor.stop_running_scene] Start.");

	EditorRuntimeToolRequest request;
	request.operation = EditorRuntimeToolRequest::OP_STOP_RUNNING_SCENE;
	_apply_tool_result(result, _dispatch_runtime_request(request));
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:editor.stop_running_scene] Failed: %s", result.error));
	} else {
		print_line(vformat("[AI Agent][Tool:editor.stop_running_scene] Completed. %s", result.content));
	}
	return result;
}

String AIEditorGetTerminalErrorsTool::get_name() const {
	return "editor.get_terminal_errors";
}

String AIEditorGetTerminalErrorsTool::get_description() const {
	return "Reads recent errors from the running scene debugger's terminal/error output.";
}

Dictionary AIEditorGetTerminalErrorsTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary max_entries_property;
	max_entries_property["type"] = "integer";
	max_entries_property["description"] = "Maximum number of recent entries to return.";
	max_entries_property["minimum"] = 1;
	max_entries_property["maximum"] = 100;
	properties["max_entries"] = max_entries_property;

	Dictionary include_warnings_property;
	include_warnings_property["type"] = "boolean";
	include_warnings_property["description"] = "Whether warning entries should be included with errors.";
	properties["include_warnings"] = include_warnings_property;

	schema["properties"] = properties;
	return schema;
}

AIToolResult AIEditorGetTerminalErrorsTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const int max_entries = CLAMP(int(p_arguments.get("max_entries", 20)), 1, 100);
	const bool include_warnings = bool(p_arguments.get("include_warnings", true));
	print_line(vformat("[AI Agent][Tool:editor.get_terminal_errors] Start. max_entries=%d include_warnings=%s", max_entries, include_warnings ? "yes" : "no"));

	EditorRuntimeToolRequest request;
	request.operation = EditorRuntimeToolRequest::OP_GET_TERMINAL_ERRORS;
	request.max_entries = max_entries;
	request.include_warnings = include_warnings;
	_apply_tool_result(result, _dispatch_runtime_request(request));
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:editor.get_terminal_errors] Failed: %s", result.error));
	} else {
		print_line(vformat("[AI Agent][Tool:editor.get_terminal_errors] Completed. entries=%d", int(result.metadata.get("entry_count", 0))));
	}
	return result;
}
