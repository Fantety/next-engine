/**************************************************************************/
/*  test_ai_agent_runtime.cpp                                             */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/agent/ai_agent_runtime.h"
#include "editor/ai_component/agent/ai_agent_session.h"
#include "editor/ai_component/storage/ai_conversation_serializer.h"
#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/ai_tool_registry.h"

TEST_FORCE_LINK(test_ai_agent_runtime);

namespace TestAIAgentRuntime {

class EchoRuntimeTool : public AITool {
	GDCLASS(EchoRuntimeTool, AITool);

public:
	int execute_count = 0;

	virtual String get_name() const override {
		return "test.echo";
	}

	virtual String get_description() const override {
		return "Echoes a test value.";
	}

	virtual Dictionary get_parameters_schema() const override {
		Dictionary schema;
		schema["type"] = "object";

		Dictionary properties;
		Dictionary value_property;
		value_property["type"] = "string";
		properties["value"] = value_property;
		schema["properties"] = properties;

		Array required;
		required.push_back("value");
		schema["required"] = required;
		return schema;
	}

	virtual AIToolResult execute(const Dictionary &p_arguments) override {
		execute_count++;

		AIToolResult result;
		result.content = String(p_arguments.get("value", ""));
		return result;
	}
};

class ScriptedRuntimeClient : public AIAgentRuntimeClient {
	GDCLASS(ScriptedRuntimeClient, AIAgentRuntimeClient);

	Vector<AIAgentRuntimeResponse> responses;

public:
	int request_count = 0;
	Array last_messages;
	Array last_tool_schemas;

	void push_response(const AIAgentRuntimeResponse &p_response) {
		responses.push_back(p_response);
	}

	virtual AIAgentRuntimeResponse complete(const Array &p_messages, const Array &p_tool_schemas) override {
		request_count++;
		last_messages = p_messages;
		last_tool_schemas = p_tool_schemas;

		if (responses.is_empty()) {
			AIAgentRuntimeResponse response;
			response.error = "No scripted runtime response.";
			return response;
		}

		AIAgentRuntimeResponse response = responses[0];
		responses.remove_at(0);
		return response;
	}
};

static AIAgentProfile _make_test_profile(bool p_allow_echo) {
	AIAgentProfile profile;
	profile.id = "test";
	profile.display_name = "Test";
	if (p_allow_echo) {
		profile.allowed_tools.insert("test.echo");
	}
	return profile;
}

static AIAgentMessage _make_user_message(const String &p_content) {
	AIAgentMessage message;
	message.role = AI_AGENT_ROLE_USER;
	message.content = p_content;
	return message;
}

TEST_CASE("[Editor][AI] Agent runtime executes allowed tool calls and continues the turn") {
	Ref<EchoRuntimeTool> echo_tool;
	echo_tool.instantiate();

	Ref<AIToolRegistry> registry;
	registry.instantiate();
	CHECK(registry->register_tool(echo_tool));

	Ref<ScriptedRuntimeClient> client;
	client.instantiate();

	AIAgentRuntimeResponse tool_response;
	AIToolCall call;
	call.id = "call_1";
	call.tool_name = "test.echo";
	call.arguments["value"] = "project context";
	tool_response.tool_calls.push_back(call);
	client->push_response(tool_response);

	AIAgentRuntimeResponse final_response;
	final_response.content = "I read: project context";
	client->push_response(final_response);

	Ref<AIAgentRuntime> runtime;
	runtime.instantiate();
	runtime->set_client(client);
	runtime->set_tool_registry(registry);
	runtime->set_profile(_make_test_profile(true));

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_user_message("Inspect the project."));

	AIAgentRuntimeResult result = runtime->run(messages);

	CHECK(result.success);
	CHECK(result.error.is_empty());
	CHECK(client->request_count == 2);
	CHECK(echo_tool->execute_count == 1);

	REQUIRE(result.tool_calls.size() == 1);
	CHECK(result.tool_calls[0].status == AI_TOOL_CALL_STATUS_COMPLETED);
	CHECK(result.tool_calls[0].id == "call_1");

	REQUIRE(result.messages.size() == 4);
	CHECK(result.messages[1].role == AI_AGENT_ROLE_ASSISTANT);
	CHECK(result.messages[1].content.is_empty());
	if (result.messages[1].metadata.has("tool_calls")) {
		Array assistant_tool_calls = result.messages[1].metadata["tool_calls"];
		REQUIRE(assistant_tool_calls.size() == 1);
		Dictionary assistant_tool_call = assistant_tool_calls[0];
		CHECK(String(assistant_tool_call["id"]) == "call_1");
		CHECK(String(assistant_tool_call["tool_name"]) == "test.echo");
	} else {
		FAIL("Assistant tool-call metadata was not recorded.");
	}

	CHECK(result.messages[2].role == AI_AGENT_ROLE_TOOL);
	CHECK(result.messages[2].content == "project context");
	CHECK(String(result.messages[2].metadata["tool_call_id"]) == "call_1");
	CHECK(String(result.messages[2].metadata["tool_name"]) == "test.echo");
	CHECK(result.messages[3].role == AI_AGENT_ROLE_ASSISTANT);
	CHECK(result.messages[3].content == "I read: project context");

	REQUIRE(client->last_tool_schemas.size() == 1);
	Dictionary schema = client->last_tool_schemas[0];
	Dictionary function_schema = schema["function"];
	CHECK(String(function_schema["name"]) == "test.echo");
}

TEST_CASE("[Editor][AI] Agent runtime denies disallowed tool calls without executing them") {
	Ref<EchoRuntimeTool> echo_tool;
	echo_tool.instantiate();

	Ref<AIToolRegistry> registry;
	registry.instantiate();
	CHECK(registry->register_tool(echo_tool));

	Ref<ScriptedRuntimeClient> client;
	client.instantiate();

	AIAgentRuntimeResponse tool_response;
	AIToolCall call;
	call.id = "call_denied";
	call.tool_name = "test.echo";
	call.arguments["value"] = "secret";
	tool_response.tool_calls.push_back(call);
	client->push_response(tool_response);

	AIAgentRuntimeResponse final_response;
	final_response.content = "I cannot use that tool.";
	client->push_response(final_response);

	Ref<AIAgentRuntime> runtime;
	runtime.instantiate();
	runtime->set_client(client);
	runtime->set_tool_registry(registry);
	runtime->set_profile(_make_test_profile(false));

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_user_message("Use a denied tool."));

	AIAgentRuntimeResult result = runtime->run(messages);

	CHECK(result.success);
	CHECK(result.error.is_empty());
	CHECK(client->request_count == 2);
	CHECK(echo_tool->execute_count == 0);

	REQUIRE(result.tool_calls.size() == 1);
	CHECK(result.tool_calls[0].status == AI_TOOL_CALL_STATUS_DENIED);

	REQUIRE(result.messages.size() == 4);
	CHECK(result.messages[1].role == AI_AGENT_ROLE_ASSISTANT);
	CHECK(result.messages[1].metadata.has("tool_calls"));
	CHECK(result.messages[2].role == AI_AGENT_ROLE_TOOL);
	CHECK(result.messages[2].content.contains("denied"));
	CHECK(String(result.messages[2].metadata["status"]) == "denied");
	CHECK(result.messages[3].role == AI_AGENT_ROLE_ASSISTANT);
	CHECK(result.messages[3].content == "I cannot use that tool.");
	CHECK(client->last_tool_schemas.is_empty());
}

TEST_CASE("[Editor][AI] Agent runtime defaults to bounded provider and tool-call limits") {
	Ref<AIAgentRuntime> runtime;
	runtime.instantiate();

	CHECK(runtime->get_max_provider_turns() == 6);
	CHECK(runtime->get_max_tool_calls() == 20);
}

TEST_CASE("[Editor][AI] Conversation serializer preserves tool messages and metadata") {
	AIAgentMessage assistant_message;
	assistant_message.role = AI_AGENT_ROLE_ASSISTANT;
	assistant_message.metadata["tool_calls"] = Array();

	AIToolCall call;
	call.id = "call_roundtrip";
	call.tool_name = "test.echo";
	call.arguments["value"] = "context";
	Array tool_calls;
	tool_calls.push_back(call.to_dict());
	assistant_message.metadata["tool_calls"] = tool_calls;

	AIAgentMessage tool_message;
	tool_message.role = AI_AGENT_ROLE_TOOL;
	tool_message.content = "context";
	tool_message.metadata["tool_call_id"] = "call_roundtrip";
	tool_message.metadata["tool_name"] = "test.echo";
	tool_message.metadata["status"] = "completed";

	Vector<AIAgentMessage> messages;
	messages.push_back(assistant_message);
	messages.push_back(tool_message);

	Array serialized = AIConversationSerializer::messages_to_array(messages);
	Vector<AIAgentMessage> restored = AIConversationSerializer::messages_from_array(serialized);

	REQUIRE(restored.size() == 2);
	CHECK(restored[0].role == AI_AGENT_ROLE_ASSISTANT);
	CHECK(restored[0].metadata.has("tool_calls"));
	Array restored_tool_calls = restored[0].metadata["tool_calls"];
	REQUIRE(restored_tool_calls.size() == 1);
	Dictionary restored_call = restored_tool_calls[0];
	CHECK(String(restored_call["id"]) == "call_roundtrip");
	CHECK(String(restored_call["tool_name"]) == "test.echo");

	CHECK(restored[1].role == AI_AGENT_ROLE_TOOL);
	CHECK(restored[1].content == "context");
	CHECK(String(restored[1].metadata["tool_call_id"]) == "call_roundtrip");
	CHECK(String(restored[1].metadata["tool_name"]) == "test.echo");
	CHECK(String(restored[1].metadata["status"]) == "completed");
}

TEST_CASE("[Editor][AI] Agent session owns runtime dependencies without enabling tools by default") {
	AIAgentSession *session = memnew(AIAgentSession);

	CHECK(session->get_agent_profile_id() == "plan");
	REQUIRE(session->get_agent_runtime().is_valid());
	REQUIRE(session->get_tool_registry().is_valid());
	CHECK(session->is_tool_runtime_available() == false);

	session->set_agent_profile_id("build");
	CHECK(session->get_agent_profile_id() == "build");
	CHECK(session->get_agent_runtime()->get_profile().id == "build");

	session->set_agent_profile_id("unknown");
	CHECK(session->get_agent_profile_id() == "plan");
	CHECK(session->get_agent_runtime()->get_profile().id == "plan");

	memdelete(session);
}

TEST_CASE("[Editor][AI] Agent runtime fails closed when tool iteration limit is exceeded") {
	Ref<EchoRuntimeTool> echo_tool;
	echo_tool.instantiate();

	Ref<AIToolRegistry> registry;
	registry.instantiate();
	CHECK(registry->register_tool(echo_tool));

	Ref<ScriptedRuntimeClient> client;
	client.instantiate();

	for (int i = 0; i < 3; i++) {
		AIAgentRuntimeResponse response;
		AIToolCall call;
		call.id = "call_" + itos(i);
		call.tool_name = "test.echo";
		call.arguments["value"] = "loop";
		response.tool_calls.push_back(call);
		client->push_response(response);
	}

	Ref<AIAgentRuntime> runtime;
	runtime.instantiate();
	runtime->set_client(client);
	runtime->set_tool_registry(registry);
	runtime->set_profile(_make_test_profile(true));
	runtime->set_max_provider_turns(2);

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_user_message("Loop forever."));

	AIAgentRuntimeResult result = runtime->run(messages);

	CHECK_FALSE(result.success);
	CHECK(result.error.contains("turn limit"));
	CHECK(client->request_count == 2);
	CHECK(echo_tool->execute_count == 2);
}

} // namespace TestAIAgentRuntime
