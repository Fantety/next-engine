/**************************************************************************/
/*  ai_mcp_stdio_client.cpp                                               */
/**************************************************************************/

#include "ai_mcp_stdio_client.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/templates/hash_map.h"

namespace {

Mutex mcp_process_start_mutex;

class ScopedMCPProcessEnvironment {
	HashMap<String, String> previous_values;
	Vector<String> missing_values;
	String previous_cwd;
	bool changed_cwd = false;

public:
	bool apply(const String &p_working_directory, const String &p_environment, String &r_error);
	void restore();
	~ScopedMCPProcessEnvironment();
};

} // namespace

void AIMCPStdioClient::_bind_methods() {
}

Vector<String> AIMCPStdioClient::_split_arguments(const String &p_arguments) {
	Vector<String> args;
	String current;
	bool in_quote = false;
	char32_t quote_char = 0;
	bool escaped = false;

	for (int i = 0; i < p_arguments.length(); i++) {
		const char32_t c = p_arguments[i];
		if (escaped) {
			current += c;
			escaped = false;
			continue;
		}
		if (c == '\\') {
			escaped = true;
			continue;
		}
		if (in_quote) {
			if (c == quote_char) {
				in_quote = false;
			} else {
				current += c;
			}
			continue;
		}
		if (c == '"' || c == '\'') {
			in_quote = true;
			quote_char = c;
			continue;
		}
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
			if (!current.is_empty()) {
				args.push_back(current);
				current.clear();
			}
			continue;
		}
		current += c;
	}
	if (escaped) {
		current += '\\';
	}
	if (!current.is_empty()) {
		args.push_back(current);
	}
	return args;
}

bool ScopedMCPProcessEnvironment::apply(const String &p_working_directory, const String &p_environment, String &r_error) {
	previous_cwd = OS::get_singleton()->get_cwd();
	if (!p_working_directory.is_empty()) {
		Error cwd_err = OS::get_singleton()->set_cwd(p_working_directory);
		if (cwd_err != OK) {
			r_error = "Failed to enter MCP server working directory: " + p_working_directory;
			return false;
		}
		changed_cwd = true;
	}

	Vector<String> lines = p_environment.split("\n", false);
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (line.is_empty() || line.begins_with("#")) {
			continue;
		}
		const int separator = line.find("=");
		if (separator <= 0) {
			continue;
		}
		const String key = line.substr(0, separator).strip_edges();
		const String value = line.substr(separator + 1).strip_edges();
		if (!key.is_empty()) {
			if (!previous_values.has(key) && !missing_values.has(key)) {
				if (OS::get_singleton()->has_environment(key)) {
					previous_values.insert(key, OS::get_singleton()->get_environment(key));
				} else {
					missing_values.push_back(key);
				}
			}
			OS::get_singleton()->set_environment(key, value);
		}
	}
	return true;
}

void ScopedMCPProcessEnvironment::restore() {
	for (int i = 0; i < missing_values.size(); i++) {
		OS::get_singleton()->unset_environment(missing_values[i]);
	}
	for (const KeyValue<String, String> &env : previous_values) {
		OS::get_singleton()->set_environment(env.key, env.value);
	}
	if (changed_cwd) {
		const Error restore_cwd_err = OS::get_singleton()->set_cwd(previous_cwd);
		ERR_FAIL_COND_MSG(restore_cwd_err != OK, "Failed to restore working directory after starting MCP server.");
		changed_cwd = false;
	}
}

ScopedMCPProcessEnvironment::~ScopedMCPProcessEnvironment() {
	restore();
}

String AIMCPStdioClient::_resolve_working_directory(const String &p_working_directory) {
	String working_directory = p_working_directory.strip_edges();
	if (working_directory.is_empty()) {
		return String();
	}
	if (working_directory.begins_with("res://")) {
		ProjectSettings *project_settings = ProjectSettings::get_singleton();
		if (project_settings) {
			return working_directory.replace("res://", project_settings->get_resource_path() + "/").simplify_path();
		}
	}
	return OS::get_singleton()->expand_path(working_directory).simplify_path();
}

bool AIMCPStdioClient::_start_process(Ref<FileAccess> &r_stdio, ProcessID &r_pid, String &r_error) const {
	r_stdio.unref();
	r_pid = 0;
	r_error.clear();
	if (server.command.strip_edges().is_empty()) {
		r_error = "MCP server command is empty.";
		return false;
	}

	MutexLock lock(mcp_process_start_mutex);

	const String working_directory = _resolve_working_directory(server.working_directory);
	ScopedMCPProcessEnvironment scoped_environment;
	if (!scoped_environment.apply(working_directory, server.environment, r_error)) {
		return false;
	}

	List<String> arguments;
	Vector<String> split_arguments = _split_arguments(server.arguments);
	for (int i = 0; i < split_arguments.size(); i++) {
		arguments.push_back(split_arguments[i]);
	}

	Dictionary pipe_info = OS::get_singleton()->execute_with_pipe(server.command, arguments, false);
	scoped_environment.restore();
	if (pipe_info.is_empty() || !pipe_info.has("stdio")) {
		r_error = "Failed to start MCP server: " + server.command;
		return false;
	}

	r_stdio = pipe_info["stdio"];
	if (r_stdio.is_null()) {
		r_error = "MCP server stdio pipe is unavailable.";
		return false;
	}
	if (pipe_info.has("pid")) {
		r_pid = (ProcessID)(int64_t)pipe_info["pid"];
	}
	return true;
}

void AIMCPStdioClient::_stop_process(ProcessID p_pid) const {
	if (p_pid != 0 && OS::get_singleton()->is_process_running(p_pid)) {
		OS::get_singleton()->kill(p_pid);
	}
}

void AIMCPStdioClient::_close_pipe(const Ref<FileAccess> &p_pipe) const {
	if (p_pipe.is_valid()) {
		p_pipe->close();
	}
}

bool AIMCPStdioClient::_send_line(const Ref<FileAccess> &p_pipe, const String &p_json, String &r_error) const {
	String request = p_json;
	if (!request.ends_with("\n")) {
		request += "\n";
	}
	PackedByteArray request_bytes = request.to_utf8_buffer();
	if (!p_pipe->store_buffer(request_bytes.ptr(), request_bytes.size())) {
		r_error = "Failed to write MCP request.";
		return false;
	}
	return true;
}

bool AIMCPStdioClient::_send_request(const Ref<FileAccess> &p_pipe, const String &p_request_json, int p_request_id, Dictionary &r_result, String &r_error) const {
	r_result.clear();
	r_error.clear();
	if (!_send_line(p_pipe, p_request_json, r_error)) {
		return false;
	}
	return _read_response(p_pipe, p_request_id, r_result, r_error);
}

bool AIMCPStdioClient::_initialize_session(const Ref<FileAccess> &p_pipe, String &r_error) {
	const int request_id = next_request_id++;
	Dictionary initialize_result;
	if (!_send_request(p_pipe, AIMCPProtocol::make_initialize_request(request_id), request_id, initialize_result, r_error)) {
		return false;
	}
	return _send_line(p_pipe, AIMCPProtocol::make_initialized_notification(), r_error);
}

bool AIMCPStdioClient::_read_response(const Ref<FileAccess> &p_pipe, int p_expected_id, Dictionary &r_result, String &r_error) const {
	String line;
	const uint64_t start = OS::get_singleton()->get_ticks_msec();
	bool process_exited = false;
	while (OS::get_singleton()->get_ticks_msec() - start < (uint64_t)timeout_msec) {
		if (_is_cancel_requested()) {
			r_error = "Tool execution cancelled.";
			return false;
		}

		const uint64_t available = p_pipe->get_length();
		if (available == 0) {
			if (p_pipe->get_error() == ERR_FILE_CANT_READ) {
				process_exited = true;
				break;
			}
			OS::get_singleton()->delay_usec(10000);
			continue;
		}

		PackedByteArray bytes;
		bytes.resize(available);
		const uint64_t read = p_pipe->get_buffer(bytes.ptrw(), available);
		if (read == 0) {
			if (p_pipe->get_error() == ERR_FILE_CANT_READ) {
				process_exited = true;
				break;
			}
			OS::get_singleton()->delay_usec(10000);
			continue;
		}
		if (read < (uint64_t)bytes.size()) {
			bytes.resize(read);
		}
		line += String::utf8((const char *)bytes.ptr(), bytes.size());

		int newline_index = line.find("\n");
		while (newline_index >= 0) {
			const String candidate = line.substr(0, newline_index).strip_edges();
			line = line.substr(newline_index + 1);
			if (!candidate.is_empty()) {
				const AIMCPResponseParseStatus status = AIMCPProtocol::parse_response_line(candidate, p_expected_id, r_result, r_error);
				if (status == AI_MCP_RESPONSE_MATCHED) {
					return true;
				}
				if (status == AI_MCP_RESPONSE_FAILED) {
					return false;
				}
			}
			newline_index = line.find("\n");
		}
	}

	r_error = process_exited ? String("MCP server exited before returning a response.") : String("MCP server request timed out.");
	return false;
}

bool AIMCPStdioClient::initialize(String &r_error) {
	Ref<FileAccess> stdio;
	ProcessID pid = 0;
	if (!_start_process(stdio, pid, r_error)) {
		return false;
	}
	const bool ok = _initialize_session(stdio, r_error);
	_close_pipe(stdio);
	_stop_process(pid);
	return ok;
}

bool AIMCPStdioClient::list_tools(Vector<AIMCPToolDescriptor> &r_tools, String &r_error) {
	Ref<FileAccess> stdio;
	ProcessID pid = 0;
	if (!_start_process(stdio, pid, r_error)) {
		return false;
	}
	if (!_initialize_session(stdio, r_error)) {
		_close_pipe(stdio);
		_stop_process(pid);
		return false;
	}

	Dictionary result;
	const int request_id = next_request_id++;
	if (!_send_request(stdio, AIMCPProtocol::make_tools_list_request(request_id), request_id, result, r_error)) {
		_close_pipe(stdio);
		_stop_process(pid);
		return false;
	}
	const bool ok = AIMCPProtocol::parse_tools_list_result(result, server.id, server.display_name, r_tools, r_error);
	_close_pipe(stdio);
	_stop_process(pid);
	return ok;
}

AIMCPToolCallResult AIMCPStdioClient::call_tool(const String &p_tool_name, const Dictionary &p_arguments) {
	AIMCPToolCallResult result;
	Ref<FileAccess> stdio;
	ProcessID pid = 0;
	String error;
	if (!_start_process(stdio, pid, error)) {
		result.error = error;
		return result;
	}
	if (!_initialize_session(stdio, error)) {
		_close_pipe(stdio);
		_stop_process(pid);
		result.error = error;
		return result;
	}

	Dictionary response;
	const int request_id = next_request_id++;
	if (!_send_request(stdio, AIMCPProtocol::make_tools_call_request(request_id, p_tool_name, p_arguments), request_id, response, error)) {
		_close_pipe(stdio);
		_stop_process(pid);
		result.error = error;
		return result;
	}
	result = AIMCPProtocol::parse_tool_call_result(response);
	_close_pipe(stdio);
	_stop_process(pid);
	result.metadata["mcp_server_id"] = server.id;
	result.metadata["mcp_server_name"] = server.display_name;
	result.metadata["mcp_tool_name"] = p_tool_name;
	result.metadata["mcp_transport"] = server.transport;
	return result;
}
