/**************************************************************************/
/*  ai_mcp_stdio_client.h                                                 */
/**************************************************************************/

#pragma once

#include "editor/ai_component/providers/ai_mcp_client.h"

#include "core/io/file_access.h"
#include "core/os/process_id.h"
#include "core/templates/vector.h"

class AIMCPStdioClient : public AIMCPClient {
	GDCLASS(AIMCPStdioClient, AIMCPClient);

	static Vector<String> _split_arguments(const String &p_arguments);
	static String _resolve_working_directory(const String &p_working_directory);
	bool _start_process(Ref<FileAccess> &r_stdio, ProcessID &r_pid, String &r_error) const;
	void _stop_process(ProcessID p_pid) const;
	bool _send_line(const Ref<FileAccess> &p_pipe, const String &p_json, String &r_error) const;
	bool _send_request(const Ref<FileAccess> &p_pipe, const String &p_request_json, int p_request_id, Dictionary &r_result, String &r_error) const;
	bool _initialize_session(const Ref<FileAccess> &p_pipe, String &r_error);
	bool _read_response(const Ref<FileAccess> &p_pipe, int p_expected_id, Dictionary &r_result, String &r_error) const;
	void _close_pipe(const Ref<FileAccess> &p_pipe) const;

protected:
	static void _bind_methods();

public:
	virtual bool initialize(String &r_error) override;
	virtual bool list_tools(Vector<AIMCPToolDescriptor> &r_tools, String &r_error) override;
	virtual AIMCPToolCallResult call_tool(const String &p_tool_name, const Dictionary &p_arguments) override;
};
