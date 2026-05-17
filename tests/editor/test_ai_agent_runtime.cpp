/**************************************************************************/
/*  test_ai_agent_runtime.cpp                                             */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/agent/ai_agent_runtime.h"
#include "editor/ai_component/agent/ai_agent_runtime_runner.h"
#include "editor/ai_component/agent/ai_agent_session.h"
#include "editor/ai_component/providers/ai_openai_compatible_provider.h"
#include "editor/ai_component/providers/ai_openai_runtime_client.h"
#include "editor/ai_component/storage/ai_conversation_serializer.h"
#include "editor/ai_component/storage/ai_conversation_store.h"
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

class FakeOpenAIRuntimeTransport : public AIOpenAIRuntimeTransport {
	GDCLASS(FakeOpenAIRuntimeTransport, AIOpenAIRuntimeTransport);

public:
	AIProviderConfig last_config;
	Array last_messages;
	Array last_tool_schemas;
	String response_text;
	String error;
	int request_count = 0;

	virtual bool request_chat_completion(const AIProviderConfig &p_config, const Array &p_messages, const Array &p_tool_schemas, String &r_response_text, String &r_error) override {
		request_count++;
		last_config = p_config;
		last_messages = p_messages;
		last_tool_schemas = p_tool_schemas;
		r_response_text = response_text;
		r_error = error;
		return error.is_empty();
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

TEST_CASE("[Editor][AI] Conversation serializer ignores null message content") {
	Array serialized;
	Dictionary message;
	message["role"] = "assistant";
	message["content"] = Variant();
	serialized.push_back(message);

	Vector<AIAgentMessage> restored = AIConversationSerializer::messages_from_array(serialized);

	REQUIRE(restored.size() == 1);
	CHECK(restored[0].role == AI_AGENT_ROLE_ASSISTANT);
	CHECK(restored[0].content.is_empty());
	CHECK_FALSE(restored[0].content == "<null>");
}

TEST_CASE("[Editor][AI] Conversation store deletes saved conversations") {
	Ref<AIConversationStore> store;
	store.instantiate();

	const String session_id = "test_delete_conversation";
	Vector<AIAgentMessage> messages;
	messages.push_back(_make_user_message("Delete me."));

	CHECK(store->save_conversation(session_id, "Delete me", messages) == OK);

	String title;
	Vector<AIAgentMessage> loaded_messages;
	CHECK(store->load_conversation(session_id, title, loaded_messages));
	CHECK(store->delete_conversation(session_id));
	CHECK_FALSE(store->load_conversation(session_id, title, loaded_messages));
	CHECK_FALSE(store->delete_conversation(session_id));
}

TEST_CASE("[Editor][AI] Agent session owns runtime dependencies with tool runtime enabled") {
	AIAgentSession *session = memnew(AIAgentSession);

	CHECK(session->get_agent_profile_id() == "plan");
	REQUIRE(session->get_agent_runtime().is_valid());
	REQUIRE(session->get_agent_runtime_runner().is_valid());
	REQUIRE(session->get_tool_registry().is_valid());
	CHECK(session->get_agent_runtime_runner()->get_runtime() == session->get_agent_runtime());
	CHECK(session->is_tool_runtime_available());

	session->set_agent_profile_id("build");
	CHECK(session->get_agent_profile_id() == "build");
	CHECK(session->get_agent_runtime()->get_profile().id == "build");

	session->set_agent_profile_id("unknown");
	CHECK(session->get_agent_profile_id() == "plan");
	CHECK(session->get_agent_runtime()->get_profile().id == "plan");

	memdelete(session);
}

TEST_CASE("[Editor][AI] Agent session deletes current conversation and starts clean session") {
	AIAgentSession *session = memnew(AIAgentSession);
	const String original_session_id = session->get_session_id();

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_user_message("Stored message."));
	session->replace_messages_for_test(messages, -1);
	CHECK(session->save_for_test() == OK);

	CHECK(session->delete_session(original_session_id));
	CHECK(session->get_session_id() != original_session_id);
	CHECK(session->get_messages_as_array().is_empty());

	String title;
	Vector<AIAgentMessage> loaded_messages;
	CHECK_FALSE(session->get_conversation_store_for_test()->load_conversation(original_session_id, title, loaded_messages));

	memdelete(session);
}

TEST_CASE("[Editor][AI] Agent session applies runtime results into message history") {
	AIAgentSession *session = memnew(AIAgentSession);

	AIAgentMessage user_message;
	user_message.role = AI_AGENT_ROLE_USER;
	user_message.content = "Read a file.";

	AIAgentMessage assistant_placeholder;
	assistant_placeholder.role = AI_AGENT_ROLE_ASSISTANT;

	Vector<AIAgentMessage> original_messages;
	original_messages.push_back(user_message);
	original_messages.push_back(assistant_placeholder);

	AIAgentMessage assistant_tool_call;
	assistant_tool_call.role = AI_AGENT_ROLE_ASSISTANT;
	assistant_tool_call.metadata["tool_calls"] = Array();

	AIAgentMessage tool_message;
	tool_message.role = AI_AGENT_ROLE_TOOL;
	tool_message.content = "file contents";
	tool_message.metadata["tool_name"] = "project.read_file";
	tool_message.metadata["status"] = "completed";

	AIAgentMessage final_assistant;
	final_assistant.role = AI_AGENT_ROLE_ASSISTANT;
	final_assistant.content = "I read the file.";

	AIAgentRuntimeResult runtime_result;
	runtime_result.success = true;
	runtime_result.messages.push_back(user_message);
	runtime_result.messages.push_back(assistant_tool_call);
	runtime_result.messages.push_back(tool_message);
	runtime_result.messages.push_back(final_assistant);

	session->replace_messages_for_test(original_messages, 1);
	session->apply_runtime_result_for_test(runtime_result);

	Array messages = session->get_messages_as_array();
	REQUIRE(messages.size() == 4);
	Dictionary second_message = messages[1];
	Dictionary third_message = messages[2];
	Dictionary fourth_message = messages[3];
	CHECK(String(second_message["role"]) == "assistant");
	CHECK(String(third_message["role"]) == "tool");
	CHECK(String(third_message["content"]) == "file contents");
	CHECK(String(fourth_message["role"]) == "assistant");
	CHECK(String(fourth_message["content"]) == "I read the file.");
	CHECK(session->get_state() == AI_AGENT_STATE_IDLE);

	memdelete(session);
}

TEST_CASE("[Editor][AI] Agent session applies runtime failures as error messages") {
	AIAgentSession *session = memnew(AIAgentSession);

	AIAgentMessage user_message;
	user_message.role = AI_AGENT_ROLE_USER;
	user_message.content = "Read a file.";

	AIAgentMessage assistant_placeholder;
	assistant_placeholder.role = AI_AGENT_ROLE_ASSISTANT;

	Vector<AIAgentMessage> original_messages;
	original_messages.push_back(user_message);
	original_messages.push_back(assistant_placeholder);

	AIAgentRuntimeResult runtime_result;
	runtime_result.success = false;
	runtime_result.error = "tool runtime failed";

	session->replace_messages_for_test(original_messages, 1);
	session->apply_runtime_result_for_test(runtime_result);

	Array messages = session->get_messages_as_array();
	REQUIRE(messages.size() == 2);
	Dictionary error_message = messages[1];
	CHECK(String(error_message["role"]) == "error");
	CHECK(String(error_message["content"]) == "tool runtime failed");
	CHECK(session->get_state() == AI_AGENT_STATE_FAILED);

	memdelete(session);
}

TEST_CASE("[Editor][AI] OpenAI-compatible provider builds non-streaming tool request bodies") {
	Array messages;
	Dictionary user_message;
	user_message["role"] = "user";
	user_message["content"] = "Read a file.";
	messages.push_back(user_message);

	Ref<EchoRuntimeTool> echo_tool;
	echo_tool.instantiate();

	Ref<AIToolRegistry> registry;
	registry.instantiate();
	CHECK(registry->register_tool(echo_tool));

	String body_text = String::utf8(reinterpret_cast<const char *>(AIOpenAICompatibleProvider::build_body_for_test(messages, "test-model", registry->get_tool_schemas(), false).ptr()));
	CHECK(body_text.contains("\"model\":\"test-model\""));
	CHECK(body_text.contains("\"stream\":false"));
	CHECK(body_text.contains("\"tools\""));
	CHECK(body_text.contains("\"test.echo\""));
}

TEST_CASE("[Editor][AI] OpenAI-compatible provider parses native tool call responses") {
	const String response_text = "{\"choices\":[{\"message\":{\"content\":null,\"tool_calls\":[{\"id\":\"call_read\",\"type\":\"function\",\"function\":{\"name\":\"project.read_file\",\"arguments\":\"{\\\"path\\\":\\\"res://player.gd\\\"}\"}}]},\"finish_reason\":\"tool_calls\"}]}";

	AIAgentRuntimeResponse response;
	String error;
	CHECK(AIOpenAICompatibleProvider::parse_chat_completion_for_test(response_text, response, error));
	CHECK(error.is_empty());
	CHECK(response.content.is_empty());

	REQUIRE(response.tool_calls.size() == 1);
	CHECK(response.tool_calls[0].id == "call_read");
	CHECK(response.tool_calls[0].tool_name == "project.read_file");
	CHECK(String(response.tool_calls[0].arguments["path"]) == "res://player.gd");
}

TEST_CASE("[Editor][AI] OpenAI-compatible provider parses native final assistant responses") {
	const String response_text = "{\"choices\":[{\"message\":{\"content\":\"Done.\"},\"finish_reason\":\"stop\"}]}";

	AIAgentRuntimeResponse response;
	String error;
	CHECK(AIOpenAICompatibleProvider::parse_chat_completion_for_test(response_text, response, error));
	CHECK(error.is_empty());
	CHECK(response.content == "Done.");
	CHECK(response.tool_calls.is_empty());
}

TEST_CASE("[Editor][AI] OpenAI-compatible provider ignores null stream deltas") {
	const String event_text = "{\"choices\":[{\"delta\":{\"content\":null},\"finish_reason\":null}]}";

	String delta;
	String finish_reason;
	String error;
	CHECK(AIOpenAICompatibleProvider::extract_delta_for_test(event_text, delta, finish_reason, error));
	CHECK(error.is_empty());
	CHECK(delta.is_empty());
	CHECK_FALSE(delta == "<null>");
	CHECK(finish_reason.is_empty());
	CHECK_FALSE(finish_reason == "<null>");
}

TEST_CASE("[Editor][AI] OpenAI-compatible runtime client converts transport responses") {
	Ref<FakeOpenAIRuntimeTransport> transport;
	transport.instantiate();
	transport->response_text = "{\"choices\":[{\"message\":{\"content\":\"Ready.\"},\"finish_reason\":\"stop\"}]}";

	AIProviderConfig config;
	config.provider_name = "Test";
	config.model = "test-model";
	config.base_url = "https://example.test/v1";
	config.api_key = "test-key";

	Ref<AIOpenAICompatibleRuntimeClient> client;
	client.instantiate();
	client->set_config(config);
	client->set_transport(transport);

	Array messages;
	Dictionary user_message;
	user_message["role"] = "user";
	user_message["content"] = "Hello";
	messages.push_back(user_message);

	Array tool_schemas;
	Dictionary schema;
	schema["type"] = "function";
	tool_schemas.push_back(schema);

	AIAgentRuntimeResponse response = client->complete(messages, tool_schemas);

	CHECK(response.error.is_empty());
	CHECK(response.content == "Ready.");
	CHECK(response.tool_calls.is_empty());
	CHECK(transport->request_count == 1);
	CHECK(transport->last_config.model == "test-model");
	CHECK(transport->last_messages.size() == 1);
	CHECK(transport->last_tool_schemas.size() == 1);
}

TEST_CASE("[Editor][AI] OpenAI-compatible runtime client maps tool schema names for provider compatibility") {
	Dictionary function;
	function["name"] = "project.read_file";
	function["description"] = "Read a file.";
	Dictionary parameters;
	parameters["type"] = "object";
	function["parameters"] = parameters;

	Dictionary schema;
	schema["type"] = "function";
	schema["function"] = function;

	Array tool_schemas;
	tool_schemas.push_back(schema);

	Dictionary tool_name_map;
	Array provider_schemas = AIOpenAICompatibleRuntimeClient::build_provider_tool_schemas_for_test(tool_schemas, tool_name_map);

	REQUIRE(provider_schemas.size() == 1);
	Dictionary provider_schema = provider_schemas[0];
	Dictionary provider_function = provider_schema["function"];
	CHECK(String(provider_function["name"]) == "project_read_file");
	CHECK(String(tool_name_map["project_read_file"]) == "project.read_file");
}

TEST_CASE("[Editor][AI] OpenAI-compatible runtime client restores provider tool call names") {
	AIAgentRuntimeResponse response;
	AIToolCall call;
	call.id = "call_read";
	call.tool_name = "project_read_file";
	response.tool_calls.push_back(call);

	Dictionary tool_name_map;
	tool_name_map["project_read_file"] = "project.read_file";

	AIOpenAICompatibleRuntimeClient::apply_tool_name_map_for_test(response, tool_name_map);

	REQUIRE(response.tool_calls.size() == 1);
	CHECK(response.tool_calls[0].tool_name == "project.read_file");
}

TEST_CASE("[Editor][AI] OpenAI-compatible runtime client converts internal tool messages") {
	Array messages;

	Dictionary assistant_message;
	assistant_message["role"] = "assistant";
	assistant_message["content"] = "";

	Dictionary metadata;
	Array tool_calls;
	Dictionary call;
	call["id"] = "call_read";
	call["tool_name"] = "project.read_file";
	Dictionary arguments;
	arguments["path"] = "res://player.gd";
	call["arguments"] = arguments;
	tool_calls.push_back(call);
	metadata["tool_calls"] = tool_calls;
	assistant_message["metadata"] = metadata;
	messages.push_back(assistant_message);

	Dictionary tool_message;
	tool_message["role"] = "tool";
	tool_message["content"] = "extends Node";
	Dictionary tool_metadata;
	tool_metadata["tool_call_id"] = "call_read";
	tool_message["metadata"] = tool_metadata;
	messages.push_back(tool_message);

	Array chat_messages = AIOpenAICompatibleRuntimeClient::build_chat_messages_for_test(messages);

	REQUIRE(chat_messages.size() == 2);
	Dictionary converted_assistant = chat_messages[0];
	CHECK(String(converted_assistant["role"]) == "assistant");
	REQUIRE(converted_assistant.has("tool_calls"));
	Array converted_tool_calls = converted_assistant["tool_calls"];
	REQUIRE(converted_tool_calls.size() == 1);
	Dictionary converted_call = converted_tool_calls[0];
	CHECK(String(converted_call["id"]) == "call_read");
	CHECK(String(converted_call["type"]) == "function");
	Dictionary converted_function = converted_call["function"];
	CHECK(String(converted_function["name"]) == "project_read_file");
	CHECK(String(converted_function["arguments"]).contains("res://player.gd"));

	Dictionary converted_tool = chat_messages[1];
	CHECK(String(converted_tool["role"]) == "tool");
	CHECK(String(converted_tool["tool_call_id"]) == "call_read");
	CHECK(String(converted_tool["content"]) == "extends Node");
}

TEST_CASE("[Editor][AI] OpenAI-compatible runtime client ignores null message content") {
	Array messages;
	Dictionary assistant_message;
	assistant_message["role"] = "assistant";
	assistant_message["content"] = Variant();
	messages.push_back(assistant_message);

	Dictionary tool_message;
	tool_message["role"] = "tool";
	tool_message["content"] = Variant();
	Dictionary tool_metadata;
	tool_metadata["tool_call_id"] = "call_empty";
	tool_message["metadata"] = tool_metadata;
	messages.push_back(tool_message);

	Array chat_messages = AIOpenAICompatibleRuntimeClient::build_chat_messages_for_test(messages);

	REQUIRE(chat_messages.size() == 2);
	Dictionary converted_assistant = chat_messages[0];
	CHECK(String(converted_assistant["content"]).is_empty());
	CHECK_FALSE(String(converted_assistant["content"]) == "<null>");

	Dictionary converted_tool = chat_messages[1];
	CHECK(String(converted_tool["content"]).is_empty());
	CHECK_FALSE(String(converted_tool["content"]) == "<null>");
}

TEST_CASE("[Editor][AI] OpenAI-compatible runtime client defaults to HTTP transport") {
	Ref<AIOpenAICompatibleRuntimeClient> client;
	client.instantiate();

	CHECK(client->get_transport().is_valid());
	CHECK(Object::cast_to<AIOpenAIHTTPRuntimeTransport>(*client->get_transport()) != nullptr);
}

TEST_CASE("[Editor][AI] OpenAI-compatible runtime client reports transport failures") {
	Ref<FakeOpenAIRuntimeTransport> transport;
	transport.instantiate();
	transport->error = "network failed";

	Ref<AIOpenAICompatibleRuntimeClient> client;
	client.instantiate();
	client->set_transport(transport);

	AIAgentRuntimeResponse response = client->complete(Array(), Array());

	CHECK(response.content.is_empty());
	CHECK(response.tool_calls.is_empty());
	CHECK(response.error == "network failed");
	CHECK(transport->request_count == 1);
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

TEST_CASE("[Editor][AI] Agent runtime runner executes runtime on a background thread") {
	Ref<EchoRuntimeTool> echo_tool;
	echo_tool.instantiate();

	Ref<AIToolRegistry> registry;
	registry.instantiate();
	CHECK(registry->register_tool(echo_tool));

	Ref<ScriptedRuntimeClient> client;
	client.instantiate();

	AIAgentRuntimeResponse final_response;
	final_response.content = "Ready.";
	client->push_response(final_response);

	Ref<AIAgentRuntime> runtime;
	runtime.instantiate();
	runtime->set_client(client);
	runtime->set_tool_registry(registry);
	runtime->set_profile(_make_test_profile(true));

	Ref<AIAgentRuntimeRunner> runner;
	runner.instantiate();
	runner->set_runtime(runtime);

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_user_message("Run in background."));

	CHECK(runner->start(messages));
	CHECK_FALSE(runner->start(messages));
	runner->wait_to_finish();
	CHECK_FALSE(runner->is_running());

	AIAgentRuntimeResult result = runner->get_last_result();
	CHECK(result.success);
	REQUIRE(result.messages.size() == 2);
	CHECK(result.messages[1].content == "Ready.");
}

} // namespace TestAIAgentRuntime
