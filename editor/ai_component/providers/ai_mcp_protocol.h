/**************************************************************************/
/*  ai_mcp_protocol.h                                                     */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

struct AIMCPToolDescriptor {
	String server_id;
	String server_name;
	String name;
	String display_name;
	String description;
	Dictionary input_schema;
};

struct AIMCPToolCallResult {
	bool success = false;
	String content;
	String error;
	Dictionary metadata;
};

struct AIMCPResourceDescriptor {
	String server_id;
	String server_name;
	String uri;
	String name;
	String description;
	String mime_type;
	Dictionary metadata;
};

struct AIMCPResourceReadResult {
	bool success = false;
	String uri;
	String mime;
	String text;
	Variant content;
	String error;
	Dictionary metadata;
};

struct AIMCPPromptDescriptor {
	String server_id;
	String server_name;
	String name;
	String description;
	Array arguments;
	Dictionary metadata;
};

struct AIMCPPromptRenderResult {
	bool success = false;
	String name;
	Array messages;
	String error;
	Dictionary metadata;
};

enum AIMCPResponseParseStatus {
	AI_MCP_RESPONSE_MATCHED,
	AI_MCP_RESPONSE_SKIPPED,
	AI_MCP_RESPONSE_FAILED,
};

class AIMCPProtocol {
	static Dictionary _make_request(int p_id, const String &p_method, const Dictionary &p_params = Dictionary());
	static String _sanitize_tool_name_part(const String &p_text);
	static String _extract_text_content(const Array &p_content);

public:
	static String make_initialize_request(int p_id);
	static String make_initialized_notification();
	static String make_tools_list_request(int p_id);
	static String make_tools_call_request(int p_id, const String &p_tool_name, const Dictionary &p_arguments);
	static String make_resources_list_request(int p_id);
	static String make_resources_read_request(int p_id, const String &p_uri);
	static String make_prompts_list_request(int p_id);
	static String make_prompts_get_request(int p_id, const String &p_prompt_name, const Dictionary &p_arguments);
	static AIMCPResponseParseStatus parse_response_line(const String &p_line, int p_expected_id, Dictionary &r_result, String &r_error);
	static bool parse_response(const String &p_line, int p_expected_id, Dictionary &r_result, String &r_error);
	static bool parse_tools_list_result(const Dictionary &p_result, const String &p_server_id, const String &p_server_name, Vector<AIMCPToolDescriptor> &r_tools, String &r_error);
	static AIMCPToolCallResult parse_tool_call_result(const Dictionary &p_result);
	static bool parse_resources_list_result(const Dictionary &p_result, const String &p_server_id, const String &p_server_name, Vector<AIMCPResourceDescriptor> &r_resources, String &r_error);
	static AIMCPResourceReadResult parse_resource_read_result(const Dictionary &p_result);
	static bool parse_prompts_list_result(const Dictionary &p_result, const String &p_server_id, const String &p_server_name, Vector<AIMCPPromptDescriptor> &r_prompts, String &r_error);
	static AIMCPPromptRenderResult parse_prompt_get_result(const Dictionary &p_result);
	static String make_agent_tool_name(const String &p_server_id, const String &p_tool_name);
};
