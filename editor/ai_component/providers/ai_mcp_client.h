/**************************************************************************/
/*  ai_mcp_client.h                                                       */
/**************************************************************************/

#pragma once

#include "editor/ai_component/providers/ai_mcp_protocol.h"
#include "editor/ai_component/providers/ai_mcp_settings.h"

#include "core/object/ref_counted.h"
#include "core/templates/vector.h"

class AIMCPClient : public RefCounted {
	GDCLASS(AIMCPClient, RefCounted);

protected:
	AIMCPServerConfig server;
	int next_request_id = 1;
	int timeout_msec = 10000;

	static void _bind_methods();
	bool _is_cancel_requested() const;

public:
	void set_server_config(const AIMCPServerConfig &p_server);
	AIMCPServerConfig get_server_config() const;
	void set_timeout_msec(int p_timeout_msec);
	int get_timeout_msec() const;

	virtual bool initialize(String &r_error);
	virtual bool list_tools(Vector<AIMCPToolDescriptor> &r_tools, String &r_error);
	virtual AIMCPToolCallResult call_tool(const String &p_tool_name, const Dictionary &p_arguments);
	virtual bool list_resources(Vector<AIMCPResourceDescriptor> &r_resources, String &r_error);
	virtual AIMCPResourceReadResult read_resource(const String &p_uri);
	virtual bool list_prompts(Vector<AIMCPPromptDescriptor> &r_prompts, String &r_error);
	virtual AIMCPPromptRenderResult render_prompt(const String &p_prompt_name, const Dictionary &p_arguments);
};

class AIMCPClientFactory {
public:
	static Ref<AIMCPClient> create_client(const AIMCPServerConfig &p_server);
};
