/**************************************************************************/
/*  ai_builtin_tools_v1.cpp                                                */
/**************************************************************************/

#include "ai_builtin_tools_v1.h"

#include "editor/agent_v1/session/service/ai_todo_service.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/templates/list.h"
#include "core/variant/variant.h"

static Dictionary _aiv1_schema_property(const String &p_type, const String &p_description = String()) {
	Dictionary property;
	property["type"] = p_type;
	if (!p_description.is_empty()) {
		property["description"] = p_description;
	}
	return property;
}

static Dictionary _aiv1_object_schema(const Dictionary &p_properties, const Array &p_required) {
	Dictionary schema;
	schema["type"] = "object";
	schema["properties"] = p_properties;
	schema["required"] = p_required;
	return schema;
}

static Dictionary _aiv1_string_enum_property(const String &p_description, const PackedStringArray &p_values) {
	Dictionary property = _aiv1_schema_property("string", p_description);
	Array values;
	for (int i = 0; i < p_values.size(); i++) {
		values.push_back(p_values[i]);
	}
	property["enum"] = values;
	return property;
}

static String _aiv1_resolve_path(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIError &r_error) {
	String path = String(p_arguments.get("path", String())).strip_edges();
	if (path.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Tool argument 'path' is required.");
		return String();
	}

	const String root_dir = p_context.root_dir.strip_edges();
	if (path.begins_with("res://") || path.begins_with("user://")) {
		path = path.simplify_path();
	} else if (!root_dir.is_empty() && !path.is_absolute_path()) {
		path = root_dir.path_join(path).simplify_path();
	}

	if (!root_dir.is_empty()) {
		const String root = root_dir.simplify_path().trim_suffix("/");
		const String resolved = path.simplify_path();
		if (resolved != root && !resolved.begins_with(root + "/") && !resolved.begins_with(root + "\\")) {
			Dictionary details;
			details["path"] = resolved;
			details["root_dir"] = root;
			r_error = AIError::make(AI_ERROR_PERMISSION, "Tool path escapes its location root.", details);
			return String();
		}
		return resolved;
	}

	if (ProjectSettings::get_singleton()) {
		return ProjectSettings::get_singleton()->localize_path(path).simplify_path();
	}
	return path.simplify_path();
}

static bool _aiv1_assert_permission(const AIV1ToolExecutionContext &p_context, const String &p_action, const String &p_resource, const String &p_reason, AIError &r_error) {
	if (p_context.permission_service.is_null()) {
		if (p_action == "file.read") {
			return true;
		}
		r_error = AIError::make(AI_ERROR_PERMISSION, "PermissionService is required for this tool.");
		return false;
	}

	AIPermissionDecision decision;
	if (!p_context.permission_service->assert_permission_struct(p_context.make_permission_input(p_action, p_resource, p_reason), decision, r_error)) {
		if (!r_error.is_error()) {
			r_error = decision.error.is_error() ? decision.error : AIError::make(AI_ERROR_PERMISSION, "Permission denied.");
		}
		return false;
	}
	return true;
}

void AIV1ReadFileTool::_bind_methods() {
}

AIV1ReadFileTool::AIV1ReadFileTool() {
	Dictionary properties;
	properties["path"] = _aiv1_schema_property("string", "Project or location-relative file path.");
	Array required;
	required.push_back("path");
	Dictionary tool_metadata;
	tool_metadata["action"] = "file.read";
	configure("Read a text file from the current location.", _aiv1_object_schema(properties, required), Callable(), tool_metadata);
}

bool AIV1ReadFileTool::execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) {
	const String path = _aiv1_resolve_path(p_arguments, p_context, r_error);
	if (path.is_empty()) {
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}
	if (!_aiv1_assert_permission(p_context, "file.read", path, "Read file content.", r_error)) {
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}
	if (!FileAccess::exists(path)) {
		Dictionary details;
		details["path"] = path;
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "File does not exist.", details);
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		Dictionary details;
		details["path"] = path;
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to open file for reading.", details);
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	const String text = file->get_as_text();
	Dictionary structured;
	structured["path"] = path;
	structured["text"] = text;
	structured["size_bytes"] = text.to_utf8_buffer().size();
	r_result = AIV1ToolExecutionResult::ok(structured, text, structured);
	return true;
}

void AIV1WriteFileTool::_bind_methods() {
}

AIV1WriteFileTool::AIV1WriteFileTool() {
	Dictionary properties;
	properties["path"] = _aiv1_schema_property("string", "Project or location-relative file path.");
	properties["content"] = _aiv1_schema_property("string", "File content to write.");
	properties["overwrite"] = _aiv1_schema_property("boolean", "Whether an existing file may be overwritten.");
	Array required;
	required.push_back("path");
	required.push_back("content");
	Dictionary tool_metadata;
	tool_metadata["action"] = "file.write";
	configure("Write text content to a file after permission approval.", _aiv1_object_schema(properties, required), Callable(), tool_metadata);
}

bool AIV1WriteFileTool::execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) {
	const String path = _aiv1_resolve_path(p_arguments, p_context, r_error);
	if (path.is_empty()) {
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}
	if (!_aiv1_assert_permission(p_context, "file.write", path, "Write file content.", r_error)) {
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	const String content = p_arguments.get("content", String());
	const bool overwrite = bool(p_arguments.get("overwrite", true));
	if (!overwrite && FileAccess::exists(path)) {
		Dictionary details;
		details["path"] = path;
		r_error = AIError::make(AI_ERROR_CONFLICT, "File already exists and overwrite is false.", details);
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	const String base_dir = path.get_base_dir();
	if (!base_dir.is_empty()) {
		const Error dir_err = DirAccess::make_dir_recursive_absolute(ProjectSettings::get_singleton() ? ProjectSettings::get_singleton()->globalize_path(base_dir) : base_dir);
		if (dir_err != OK) {
			r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to create output directory.");
			r_result = AIV1ToolExecutionResult::fail(r_error);
			return false;
		}
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE, &err);
	if (file.is_null() || err != OK) {
		Dictionary details;
		details["path"] = path;
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to open file for writing.", details);
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}
	file->store_string(content);
	file->flush();

	Dictionary structured;
	structured["path"] = path;
	structured["bytes_written"] = content.to_utf8_buffer().size();
	PackedStringArray output_paths;
	output_paths.push_back(path);
	r_result = AIV1ToolExecutionResult::ok(structured, "Wrote file: " + path, structured);
	r_result.output_paths = output_paths;
	return true;
}

void AIV1ShellTool::_bind_methods() {
}

AIV1ShellTool::AIV1ShellTool() {
	Dictionary properties;
	properties["command"] = _aiv1_schema_property("string", "Shell command to run.");
	Array required;
	required.push_back("command");
	Dictionary tool_metadata;
	tool_metadata["action"] = "shell.run";
	configure("Run a shell command after permission approval.", _aiv1_object_schema(properties, required), Callable(), tool_metadata);
}

bool AIV1ShellTool::execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) {
	const String command = String(p_arguments.get("command", String())).strip_edges();
	if (command.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Tool argument 'command' is required.");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}
	if (!_aiv1_assert_permission(p_context, "shell.run", command, "Run shell command.", r_error)) {
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	const String os_name = OS::get_singleton() ? OS::get_singleton()->get_name().to_lower() : String();
	String executable = "/bin/sh";
	List<String> arguments;
	if (os_name.contains("windows")) {
		executable = "cmd";
		arguments.push_back("/C");
		arguments.push_back(command);
	} else {
		arguments.push_back("-lc");
		arguments.push_back(command);
	}

	String output;
	int exit_code = 0;
	const Error err = OS::get_singleton()->execute(executable, arguments, &output, &exit_code, true);
	if (err != OK) {
		Dictionary details;
		details["command"] = command;
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to launch shell command.", details);
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Dictionary structured;
	structured["command"] = command;
	structured["exit_code"] = exit_code;
	structured["output"] = output;
	if (exit_code != 0) {
		Dictionary details = structured.duplicate(true);
		r_error = AIError::make(AI_ERROR_INTERNAL, "Shell command failed.", details);
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	r_result = AIV1ToolExecutionResult::ok(structured, output, structured);
	return true;
}

void AIV1TodoWriteTool::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_todo_service", "service"), &AIV1TodoWriteTool::set_todo_service);
	ClassDB::bind_method(D_METHOD("get_todo_service"), &AIV1TodoWriteTool::get_todo_service);
}

AIV1TodoWriteTool::AIV1TodoWriteTool() {
	Dictionary todo_properties;
	todo_properties["content"] = _aiv1_schema_property("string", "Short task description.");
	PackedStringArray statuses;
	statuses.push_back("pending");
	statuses.push_back("in_progress");
	statuses.push_back("completed");
	statuses.push_back("cancelled");
	todo_properties["status"] = _aiv1_string_enum_property("Task status.", statuses);
	PackedStringArray priorities;
	priorities.push_back("high");
	priorities.push_back("medium");
	priorities.push_back("low");
	todo_properties["priority"] = _aiv1_string_enum_property("Task priority.", priorities);

	Array todo_required;
	todo_required.push_back("content");
	todo_required.push_back("status");
	todo_required.push_back("priority");

	Dictionary todo_item_schema = _aiv1_object_schema(todo_properties, todo_required);

	Dictionary todos_property = _aiv1_schema_property("array", "The complete current todo snapshot for this session, in display order. Replace the whole list every time.");
	todos_property["items"] = todo_item_schema;

	Dictionary properties;
	properties["todos"] = todos_property;
	Array required;
	required.push_back("todos");

	Dictionary tool_metadata;
	tool_metadata["action"] = "todo.write";
	tool_metadata["tool_origin"] = "builtin";
	configure(
			"Maintain the user-visible task list for the current session. Use this tool when the work has three or more clear steps. Always submit the complete todos array, not a patch. Mark exactly one active task as in_progress while working, mark tasks completed immediately after finishing them, mark obsolete tasks cancelled, and do not wait until the end to update progress.",
			_aiv1_object_schema(properties, required),
			Callable(),
			tool_metadata);
}

void AIV1TodoWriteTool::set_todo_service(const Ref<AITodoService> &p_service) {
	todo_service = p_service;
	if (todo_service.is_null()) {
		todo_service.instantiate();
	}
}

Ref<AITodoService> AIV1TodoWriteTool::get_todo_service() const {
	return todo_service;
}

bool AIV1TodoWriteTool::execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) {
	const String session_id = p_context.session_id.strip_edges();
	if (session_id.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "todowrite requires a session id.");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}
	if (p_arguments.get("todos", Variant()).get_type() != Variant::ARRAY) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "todowrite argument 'todos' must be an array.");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}
	if (p_context.permission_service.is_null()) {
		r_error = AIError::make(AI_ERROR_PERMISSION, "PermissionService is required to update todos.");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Dictionary permission_input = p_context.make_permission_input("todo.write", "session:" + session_id + "/todos", "Update session todo list.");
	permission_input["default_effect"] = "allow";
	AIPermissionDecision decision;
	if (!p_context.permission_service->assert_permission_struct(permission_input, decision, r_error)) {
		if (!r_error.is_error()) {
			r_error = decision.error.is_error() ? decision.error : AIError::make(AI_ERROR_PERMISSION, "Permission denied.");
		}
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	if (todo_service.is_null()) {
		todo_service.instantiate();
	}
	if (todo_service->get_event_store().is_null()) {
		todo_service->set_event_store(p_context.permission_service->get_event_store());
	}

	Array todos;
	if (!todo_service->update_todos_struct(session_id, p_arguments["todos"], todos, r_error)) {
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Dictionary structured;
	structured["session_id"] = session_id;
	structured["todos"] = todos.duplicate(true);
	r_result = AIV1ToolExecutionResult::ok(structured, structured, structured);
	r_result.metadata["todo_count"] = todos.size();
	return true;
}

void AIV1BuiltinTools::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_read_file_tool"), &AIV1BuiltinTools::create_read_file_tool);
	ClassDB::bind_method(D_METHOD("create_write_file_tool"), &AIV1BuiltinTools::create_write_file_tool);
	ClassDB::bind_method(D_METHOD("create_shell_tool"), &AIV1BuiltinTools::create_shell_tool);
	ClassDB::bind_method(D_METHOD("create_todo_write_tool"), &AIV1BuiltinTools::create_todo_write_tool);
}

Ref<AIV1ReadFileTool> AIV1BuiltinTools::create_read_file_tool() const {
	Ref<AIV1ReadFileTool> tool;
	tool.instantiate();
	return tool;
}

Ref<AIV1WriteFileTool> AIV1BuiltinTools::create_write_file_tool() const {
	Ref<AIV1WriteFileTool> tool;
	tool.instantiate();
	return tool;
}

Ref<AIV1ShellTool> AIV1BuiltinTools::create_shell_tool() const {
	Ref<AIV1ShellTool> tool;
	tool.instantiate();
	return tool;
}

Ref<AIV1TodoWriteTool> AIV1BuiltinTools::create_todo_write_tool() const {
	Ref<AIV1TodoWriteTool> tool;
	tool.instantiate();
	return tool;
}
