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

String AIMCPProtocol::make_resources_list_request(int p_id) {
	return JSON::stringify(_make_request(p_id, "resources/list"));
}

String AIMCPProtocol::make_resources_read_request(int p_id, const String &p_uri) {
	Dictionary params;
	params["uri"] = p_uri;
	return JSON::stringify(_make_request(p_id, "resources/read", params));
}

String AIMCPProtocol::make_prompts_list_request(int p_id) {
	return JSON::stringify(_make_request(p_id, "prompts/list"));
}

String AIMCPProtocol::make_prompts_get_request(int p_id, const String &p_prompt_name, const Dictionary &p_arguments) {
	Dictionary params;
	params["name"] = p_prompt_name;
	params["arguments"] = p_arguments;
	return JSON::stringify(_make_request(p_id, "prompts/get", params));
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

bool AIMCPProtocol::parse_resources_list_result(const Dictionary &p_result, const String &p_server_id, const String &p_server_name, Vector<AIMCPResourceDescriptor> &r_resources, String &r_error) {
	r_resources.clear();
	r_error.clear();
	if (!p_result.has("resources") || Variant(p_result["resources"]).get_type() != Variant::ARRAY) {
		r_error = "MCP resources/list result did not contain resources.";
		return false;
	}

	Array resources = p_result["resources"];
	for (int i = 0; i < resources.size(); i++) {
		if (Variant(resources[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary item = resources[i];
		AIMCPResourceDescriptor resource;
		resource.server_id = p_server_id;
		resource.server_name = p_server_name;
		resource.uri = String(item.get("uri", String()));
		if (resource.uri.is_empty()) {
			continue;
		}
		resource.name = String(item.get("name", item.get("title", resource.uri)));
		resource.description = String(item.get("description", String()));
		resource.mime_type = String(item.get("mimeType", item.get("mime", String())));
		resource.metadata = item.duplicate(true);
		r_resources.push_back(resource);
	}
	return true;
}

AIMCPResourceReadResult AIMCPProtocol::parse_resource_read_result(const Dictionary &p_result) {
	AIMCPResourceReadResult result;
	const bool is_error = bool(p_result.get("isError", false));
	result.metadata = p_result.duplicate(true);
	if (is_error) {
		result.error = String(p_result.get("error", "MCP resource read returned an error."));
		if (result.error.is_empty() && p_result.has("text")) {
			result.error = String(p_result["text"]);
		}
		return result;
	}

	if (p_result.has("contents") && Variant(p_result["contents"]).get_type() == Variant::ARRAY) {
		Array contents = p_result["contents"];
		Vector<String> text_parts;
		for (int i = 0; i < contents.size(); i++) {
			if (Variant(contents[i]).get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary item = contents[i];
			if (result.uri.is_empty()) {
				result.uri = String(item.get("uri", String()));
			}
			if (result.mime.is_empty()) {
				result.mime = String(item.get("mimeType", item.get("mime", String())));
			}
			if (item.has("text")) {
				text_parts.push_back(String(item["text"]));
			} else {
				text_parts.push_back(JSON::stringify(item));
			}
		}
		result.text = String("\n").join(text_parts);
		result.content = contents.duplicate(true);
	} else {
		result.uri = String(p_result.get("uri", String()));
		result.mime = String(p_result.get("mimeType", p_result.get("mime", String())));
		if (p_result.has("text")) {
			result.text = String(p_result["text"]);
		} else if (p_result.has("content")) {
			result.text = String(p_result["content"]);
		}
		result.content = p_result.duplicate(true);
	}

	if (result.mime.is_empty()) {
		result.mime = "text/plain";
	}
	result.success = true;
	return result;
}

bool AIMCPProtocol::parse_prompts_list_result(const Dictionary &p_result, const String &p_server_id, const String &p_server_name, Vector<AIMCPPromptDescriptor> &r_prompts, String &r_error) {
	r_prompts.clear();
	r_error.clear();
	if (!p_result.has("prompts") || Variant(p_result["prompts"]).get_type() != Variant::ARRAY) {
		r_error = "MCP prompts/list result did not contain prompts.";
		return false;
	}

	Array prompts = p_result["prompts"];
	for (int i = 0; i < prompts.size(); i++) {
		if (Variant(prompts[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary item = prompts[i];
		AIMCPPromptDescriptor prompt;
		prompt.server_id = p_server_id;
		prompt.server_name = p_server_name;
		prompt.name = String(item.get("name", String()));
		if (prompt.name.is_empty()) {
			continue;
		}
		prompt.description = String(item.get("description", String()));
		if (item.has("arguments") && Variant(item["arguments"]).get_type() == Variant::ARRAY) {
			prompt.arguments = Array(item["arguments"]).duplicate(true);
		}
		prompt.metadata = item.duplicate(true);
		r_prompts.push_back(prompt);
	}
	return true;
}

AIMCPPromptRenderResult AIMCPProtocol::parse_prompt_get_result(const Dictionary &p_result) {
	AIMCPPromptRenderResult result;
	const bool is_error = bool(p_result.get("isError", false));
	result.metadata = p_result.duplicate(true);
	result.name = String(p_result.get("name", String()));
	if (is_error) {
		result.error = String(p_result.get("error", "MCP prompt returned an error."));
		return result;
	}
	if (!p_result.has("messages") || Variant(p_result["messages"]).get_type() != Variant::ARRAY) {
		result.error = "MCP prompts/get result did not contain messages.";
		return result;
	}
	result.messages = Array(p_result["messages"]).duplicate(true);
	result.success = true;
	return result;
}

String AIMCPProtocol::make_agent_tool_name(const String &p_server_id, const String &p_tool_name) {
	return "mcp_" + _sanitize_tool_name_part(p_server_id) + "_" + _sanitize_tool_name_part(p_tool_name);
}
