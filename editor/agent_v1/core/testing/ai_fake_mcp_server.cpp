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

Array AIFakeMCPServer::get_calls() const {
	return calls.duplicate(true);
}
