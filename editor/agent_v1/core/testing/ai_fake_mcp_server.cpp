/**************************************************************************/
/*  ai_fake_mcp_server.cpp                                                */
/**************************************************************************/

#include "ai_fake_mcp_server.h"

#include "core/object/class_db.h"

void AIFakeMCPServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start"), &AIFakeMCPServer::start);
	ClassDB::bind_method(D_METHOD("stop"), &AIFakeMCPServer::stop);
	ClassDB::bind_method(D_METHOD("is_running"), &AIFakeMCPServer::is_running);
	ClassDB::bind_method(D_METHOD("clear"), &AIFakeMCPServer::clear);
	ClassDB::bind_method(D_METHOD("register_tool", "name", "descriptor", "result", "fail", "error"), &AIFakeMCPServer::register_tool_struct, DEFVAL(Variant()), DEFVAL(false), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("list_tools"), &AIFakeMCPServer::list_tools);
	ClassDB::bind_method(D_METHOD("call_tool", "name", "input"), &AIFakeMCPServer::call_tool, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("register_resource", "uri", "descriptor", "content", "fail", "error"), &AIFakeMCPServer::register_resource_struct, DEFVAL(Variant()), DEFVAL(false), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("list_resources"), &AIFakeMCPServer::list_resources);
	ClassDB::bind_method(D_METHOD("read_resource", "uri"), &AIFakeMCPServer::read_resource);
	ClassDB::bind_method(D_METHOD("register_prompt", "name", "descriptor", "messages", "fail", "error"), &AIFakeMCPServer::register_prompt_struct, DEFVAL(Array()), DEFVAL(false), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("list_prompts"), &AIFakeMCPServer::list_prompts);
	ClassDB::bind_method(D_METHOD("render_prompt", "name", "arguments"), &AIFakeMCPServer::render_prompt, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_calls"), &AIFakeMCPServer::get_calls);
}

void AIFakeMCPServer::start() {
	running = true;
}

void AIFakeMCPServer::stop() {
	running = false;
}

bool AIFakeMCPServer::is_running() const {
	return running;
}

void AIFakeMCPServer::clear() {
	tools.clear();
	resources.clear();
	prompts.clear();
	calls.clear();
	running = false;
}

void AIFakeMCPServer::register_tool_struct(const String &p_name, const Dictionary &p_descriptor, const Variant &p_result, bool p_fail, const String &p_error) {
	const String name = p_name.strip_edges();
	if (name.is_empty()) {
		return;
	}

	ToolStub stub;
	stub.descriptor = p_descriptor.duplicate(true);
	stub.descriptor["name"] = name;
	stub.result = p_result;
	stub.fail = p_fail;
	stub.error = p_error;
	tools[name] = stub;
}

Array AIFakeMCPServer::list_tools() const {
	Array result;
	for (const KeyValue<String, ToolStub> &kv : tools) {
		result.push_back(kv.value.descriptor.duplicate(true));
	}
	return result;
}

Dictionary AIFakeMCPServer::call_tool(const String &p_name, const Variant &p_input) {
	Dictionary call;
	call["name"] = p_name;
	call["input"] = p_input;
	calls.push_back(call);

	Dictionary result;
	if (!running) {
		result["success"] = false;
		result["error"] = "Fake MCP server is not running.";
		return result;
	}

	const String name = p_name.strip_edges();
	HashMap<String, ToolStub>::Iterator tool = tools.find(name);
	if (!tool) {
		result["success"] = false;
		result["error"] = "Fake MCP tool not found: " + name;
		return result;
	}

	if (tool->value.fail) {
		result["success"] = false;
		result["error"] = tool->value.error.is_empty() ? String("Fake MCP tool failed: " + name) : tool->value.error;
		return result;
	}

	result["success"] = true;
	result["result"] = tool->value.result;
	return result;
}

void AIFakeMCPServer::register_resource_struct(const String &p_uri, const Dictionary &p_descriptor, const Variant &p_content, bool p_fail, const String &p_error) {
	const String uri = p_uri.strip_edges();
	if (uri.is_empty()) {
		return;
	}

	ResourceStub stub;
	stub.descriptor = p_descriptor.duplicate(true);
	stub.descriptor["uri"] = uri;
	if (!stub.descriptor.has("mimeType") && !stub.descriptor.has("mime")) {
		stub.descriptor["mimeType"] = "text/plain";
	}
	stub.content = p_content;
	stub.fail = p_fail;
	stub.error = p_error;
	resources[uri] = stub;
}

Array AIFakeMCPServer::list_resources() const {
	Array result;
	for (const KeyValue<String, ResourceStub> &kv : resources) {
		result.push_back(kv.value.descriptor.duplicate(true));
	}
	return result;
}

Dictionary AIFakeMCPServer::read_resource(const String &p_uri) {
	Dictionary result;
	if (!running) {
		result["success"] = false;
		result["error"] = "Fake MCP server is not running.";
		return result;
	}

	const String uri = p_uri.strip_edges();
	HashMap<String, ResourceStub>::Iterator resource = resources.find(uri);
	if (!resource) {
		result["success"] = false;
		result["error"] = "Fake MCP resource not found: " + uri;
		return result;
	}

	if (resource->value.fail) {
		result["success"] = false;
		result["error"] = resource->value.error.is_empty() ? String("Fake MCP resource failed: " + uri) : resource->value.error;
		return result;
	}

	result["success"] = true;
	result["uri"] = uri;
	result["mimeType"] = resource->value.descriptor.get("mimeType", resource->value.descriptor.get("mime", "text/plain"));
	result["mime"] = result["mimeType"];
	result["content"] = resource->value.content;
	if (resource->value.content.get_type() == Variant::DICTIONARY) {
		Dictionary content = resource->value.content;
		if (content.has("text")) {
			result["text"] = content["text"];
		}
	} else if (resource->value.content.get_type() != Variant::NIL) {
		result["text"] = String(resource->value.content);
	}
	return result;
}

void AIFakeMCPServer::register_prompt_struct(const String &p_name, const Dictionary &p_descriptor, const Array &p_messages, bool p_fail, const String &p_error) {
	const String name = p_name.strip_edges();
	if (name.is_empty()) {
		return;
	}

	PromptStub stub;
	stub.descriptor = p_descriptor.duplicate(true);
	stub.descriptor["name"] = name;
	stub.messages = p_messages.duplicate(true);
	stub.fail = p_fail;
	stub.error = p_error;
	prompts[name] = stub;
}

Array AIFakeMCPServer::list_prompts() const {
	Array result;
	for (const KeyValue<String, PromptStub> &kv : prompts) {
		result.push_back(kv.value.descriptor.duplicate(true));
	}
	return result;
}

Dictionary AIFakeMCPServer::render_prompt(const String &p_name, const Dictionary &p_arguments) {
	(void)p_arguments;

	Dictionary result;
	if (!running) {
		result["success"] = false;
		result["error"] = "Fake MCP server is not running.";
		return result;
	}

	const String name = p_name.strip_edges();
	HashMap<String, PromptStub>::Iterator prompt = prompts.find(name);
	if (!prompt) {
		result["success"] = false;
		result["error"] = "Fake MCP prompt not found: " + name;
		return result;
	}

	if (prompt->value.fail) {
		result["success"] = false;
		result["error"] = prompt->value.error.is_empty() ? String("Fake MCP prompt failed: " + name) : prompt->value.error;
		return result;
	}

	result["success"] = true;
	result["name"] = name;
	result["messages"] = prompt->value.messages.duplicate(true);
	return result;
}

Array AIFakeMCPServer::get_calls() const {
	return calls.duplicate(true);
}
