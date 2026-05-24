/**************************************************************************/
/*  ai_mcp_protocol.cpp                                                   */
/**************************************************************************/

#include "ai_mcp_protocol.h"

#include "core/io/json.h"

Dictionary AIMCPProtocol::_make_request(int p_id, const String &p_method, const Dictionary &p_params) {
	Dictionary request;
	request["jsonrpc"] = "2.0";
	request["id"] = p_id;
	request["method"] = p_method;
	if (!p_params.is_empty()) {
		request["params"] = p_params;
	}
	return request;
}

String AIMCPProtocol::_sanitize_tool_name_part(const String &p_text) {
	String sanitized;
	for (int i = 0; i < p_text.length(); i++) {
		const char32_t c = p_text[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
			sanitized += c;
		} else {
			sanitized += "_";
		}
	}
	while (sanitized.contains("__")) {
		sanitized = sanitized.replace("__", "_");
	}
	sanitized = sanitized.strip_edges().trim_prefix("_").trim_suffix("_");
	return sanitized.is_empty() ? String("mcp") : sanitized;
}

String AIMCPProtocol::_extract_text_content(const Array &p_content) {
	Vector<String> parts;
	for (int i = 0; i < p_content.size(); i++) {
		if (Variant(p_content[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary item = p_content[i];
		const String type = String(item.get("type", String()));
		if (type == "text" && item.has("text")) {
			parts.push_back(String(item["text"]));
		} else if (item.has("text")) {
			parts.push_back(String(item["text"]));
		} else {
			parts.push_back(JSON::stringify(item));
		}
	}
	return String("\n").join(parts);
}

String AIMCPProtocol::make_initialize_request(int p_id) {
	Dictionary client_info;
	client_info["name"] = "NEXT Engine";
	client_info["version"] = "0.1";

	Dictionary capabilities;
	Dictionary params;
	params["protocolVersion"] = "2025-06-18";
	params["capabilities"] = capabilities;
	params["clientInfo"] = client_info;
	return JSON::stringify(_make_request(p_id, "initialize", params));
}

String AIMCPProtocol::make_initialized_notification() {
	Dictionary notification;
	notification["jsonrpc"] = "2.0";
	notification["method"] = "notifications/initialized";
	return JSON::stringify(notification);
}

String AIMCPProtocol::make_tools_list_request(int p_id) {
	return JSON::stringify(_make_request(p_id, "tools/list"));
}

String AIMCPProtocol::make_tools_call_request(int p_id, const String &p_tool_name, const Dictionary &p_arguments) {
	Dictionary params;
	params["name"] = p_tool_name;
	params["arguments"] = p_arguments;
	return JSON::stringify(_make_request(p_id, "tools/call", params));
}

AIMCPResponseParseStatus AIMCPProtocol::parse_response_line(const String &p_line, int p_expected_id, Dictionary &r_result, String &r_error) {
	r_result.clear();
	r_error.clear();

	Ref<JSON> parser;
	parser.instantiate();
	if (parser->parse(p_line) != OK || parser->get_data().get_type() != Variant::DICTIONARY) {
		r_error = "MCP server returned invalid JSON-RPC.";
		return AI_MCP_RESPONSE_FAILED;
	}

	Dictionary response = parser->get_data();
	if (String(response.get("jsonrpc", String())) != "2.0") {
		r_error = "MCP server returned an unsupported JSON-RPC version.";
		return AI_MCP_RESPONSE_FAILED;
	}
	if (!response.has("id")) {
		return AI_MCP_RESPONSE_SKIPPED;
	}
	if ((int)response.get("id", -1) != p_expected_id) {
		return AI_MCP_RESPONSE_SKIPPED;
	}
	if (response.has("error") && Variant(response["error"]).get_type() == Variant::DICTIONARY) {
		Dictionary error = response["error"];
		r_error = String(error.get("message", "MCP server returned an error."));
		return AI_MCP_RESPONSE_FAILED;
	}
	if (!response.has("result") || Variant(response["result"]).get_type() != Variant::DICTIONARY) {
		r_error = "MCP server response did not contain a result object.";
		return AI_MCP_RESPONSE_FAILED;
	}

	r_result = response["result"];
	return AI_MCP_RESPONSE_MATCHED;
}

bool AIMCPProtocol::parse_response(const String &p_line, int p_expected_id, Dictionary &r_result, String &r_error) {
	const AIMCPResponseParseStatus status = parse_response_line(p_line, p_expected_id, r_result, r_error);
	if (status == AI_MCP_RESPONSE_MATCHED) {
		return true;
	}
	if (status == AI_MCP_RESPONSE_SKIPPED) {
		r_error = "MCP server did not return the expected response.";
	}
	return false;
}

bool AIMCPProtocol::parse_tools_list_result(const Dictionary &p_result, const String &p_server_id, const String &p_server_name, Vector<AIMCPToolDescriptor> &r_tools, String &r_error) {
	r_tools.clear();
	r_error.clear();
	if (!p_result.has("tools") || Variant(p_result["tools"]).get_type() != Variant::ARRAY) {
		r_error = "MCP tools/list result did not contain tools.";
		return false;
	}

	Array tools = p_result["tools"];
	for (int i = 0; i < tools.size(); i++) {
		if (Variant(tools[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary item = tools[i];
		AIMCPToolDescriptor tool;
		tool.server_id = p_server_id;
		tool.server_name = p_server_name;
		tool.name = String(item.get("name", String()));
		if (tool.name.is_empty()) {
			continue;
		}
		tool.display_name = String(item.get("title", item.get("displayName", tool.name)));
		tool.description = String(item.get("description", String()));
		if (item.has("inputSchema") && Variant(item["inputSchema"]).get_type() == Variant::DICTIONARY) {
			tool.input_schema = item["inputSchema"];
		} else {
			tool.input_schema["type"] = "object";
			tool.input_schema["properties"] = Dictionary();
		}
		r_tools.push_back(tool);
	}
	return true;
}

AIMCPToolCallResult AIMCPProtocol::parse_tool_call_result(const Dictionary &p_result) {
	AIMCPToolCallResult result;
	const bool is_error = bool(p_result.get("isError", false));
	if (p_result.has("content") && Variant(p_result["content"]).get_type() == Variant::ARRAY) {
		result.content = _extract_text_content(p_result["content"]);
	} else {
		result.content = JSON::stringify(p_result);
	}

	result.metadata["mcp_is_error"] = is_error;
	result.success = !is_error;
	if (is_error) {
		result.error = result.content.is_empty() ? String("MCP tool returned an error.") : result.content;
	}
	return result;
}

String AIMCPProtocol::make_agent_tool_name(const String &p_server_id, const String &p_tool_name) {
	return "mcp_" + _sanitize_tool_name_part(p_server_id) + "_" + _sanitize_tool_name_part(p_tool_name);
}
