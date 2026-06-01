/**************************************************************************/
/*  test_ai_agent_runtime.cpp                                             */
/**************************************************************************/

#include "tests/test_macros.h"

#include "core/object/callable_mp.h"

#include "editor/ai_component/agent/ai_agent_base.h"
#include "editor/ai_component/agent/ai_context_manager.h"
#include "editor/ai_component/agent/ai_agent_runtime.h"
#include "editor/ai_component/agent/ai_agent_runtime_runner.h"
#include "editor/ai_component/agent/ai_agent_session.h"
#include "editor/ai_component/agent/ai_session_base.h"
#include "editor/ai_component/agent/ai_main_agent.h"
#include "editor/ai_component/context/ai_best_practices_context_provider.h"
#include "editor/ai_component/prompts/agent_system_prompt.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/ai_component/providers/ai_openai_compatible_codec.h"
#include "editor/ai_component/providers/ai_openai_runtime_client.h"
#include "editor/ai_component/storage/ai_conversation_serializer.h"
#include "editor/ai_component/storage/ai_conversation_store.h"
#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/ai_tool_permission.h"
#include "editor/ai_component/tools/ai_tool_registry.h"

TEST_FORCE_LINK(test_ai_agent_runtime);

namespace TestAIAgentRuntime {

class ExposedAISessionBase : public AISessionBase {
	GDCLASS(ExposedAISessionBase, AISessionBase);

public:
	String get_project_scope_key_for_test() const {
		return _get_project_scope_key();
	}

	String make_unique_id_for_test(const String &p_prefix = String()) const {
		return _make_unique_id(p_prefix);
	}
};

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

class FailingRuntimeTool : public AITool {
	GDCLASS(FailingRuntimeTool, AITool);

public:
	int execute_count = 0;

	virtual String get_name() const override {
		return "test.fail";
	}

	virtual String get_description() const override {
		return "Always returns a test failure.";
	}

	virtual Dictionary get_parameters_schema() const override {
		Dictionary schema;
		schema["type"] = "object";
		return schema;
	}

	virtual AIToolResult execute(const Dictionary &p_arguments) override {
		(void)p_arguments;
		execute_count++;

		AIToolResult result;
		result.error = "simulated missing scene";
		return result;
	}
};

class ScriptedRuntimeClient : public AIAgentRuntimeClient {
	GDCLASS(ScriptedRuntimeClient, AIAgentRuntimeClient);

	Vector<AIAgentRuntimeResponse> responses;
	Vector<AIAgentRuntimeResponse> partial_responses;

public:
	int request_count = 0;
	Array last_messages;
	Array last_tool_schemas;

	void push_response(const AIAgentRuntimeResponse &p_response) {
		responses.push_back(p_response);
	}

	void push_partial_response(const AIAgentRuntimeResponse &p_response) {
		partial_responses.push_back(p_response);
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

	virtual AIAgentRuntimeResponse complete_streaming(const Array &p_messages, const Array &p_tool_schemas, const Callable &p_partial_response_callback) override {
		request_count++;
		last_messages = p_messages;
		last_tool_schemas = p_tool_schemas;

		for (int i = 0; i < partial_responses.size(); i++) {
			Dictionary partial;
			partial["content"] = partial_responses[i].content;
			partial["metadata"] = partial_responses[i].metadata;
			p_partial_response_callback.call(partial);
		}
		partial_responses.clear();

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
	Vector<String> stream_events;
	bool stream_supported = false;

	virtual bool request_chat_completion(const AIProviderConfig &p_config, const Array &p_messages, const Array &p_tool_schemas, String &r_response_text, String &r_error) override {
		request_count++;
		last_config = p_config;
		last_messages = p_messages;
		last_tool_schemas = p_tool_schemas;
		r_response_text = response_text;
		r_error = error;
		return error.is_empty();
	}

	virtual bool request_chat_completion_stream(const AIProviderConfig &p_config, const Array &p_messages, const Array &p_tool_schemas, const Callable &p_stream_event_callback, String &r_error) override {
		request_count++;
		last_config = p_config;
		last_messages = p_messages;
		last_tool_schemas = p_tool_schemas;
		if (!stream_supported) {
			r_error = "OpenAI streaming runtime transport is not implemented.";
			return false;
		}
		if (!error.is_empty()) {
			r_error = error;
			return false;
		}
		for (int i = 0; i < stream_events.size(); i++) {
			p_stream_event_callback.call(stream_events[i]);
		}
		r_error = String();
		return true;
	}
};

class RuntimeProgressRecorder : public RefCounted {
	GDCLASS(RuntimeProgressRecorder, RefCounted);

public:
	Array added_messages;
	Array added_indices;
	Array updated_messages;
	Array updated_indices;
	Array partial_responses;

	void record_added(int p_index, const Dictionary &p_message) {
		added_indices.push_back(p_index);
		added_messages.push_back(p_message);
	}

	void record_updated(int p_index, const Dictionary &p_message) {
		updated_indices.push_back(p_index);
		updated_messages.push_back(p_message);
	}

	void record_partial(const Dictionary &p_response) {
		partial_responses.push_back(p_response);
	}
};

static AIAgentProfile _make_test_profile(bool p_allow_echo) {
	(void)p_allow_echo;

	AIAgentProfile profile;
	profile.id = "test";
	profile.display_name = "Test";
	return profile;
}

static AIAgentProfile _make_test_profile_with_ask_tool(const String &p_tool_name) {
	(void)p_tool_name;

	AIAgentProfile profile;
	profile.id = "test";
	profile.display_name = "Test";
	return profile;
}

static AIAgentMessage _make_user_message(const String &p_content) {
	AIAgentMessage message;
	message.role = AI_AGENT_ROLE_USER;
	message.content = p_content;
	return message;
}

static AIAgentMessage _make_message(AIAgentRole p_role, const String &p_content) {
	AIAgentMessage message;
	message.role = p_role;
	message.content = p_content;
	return message;
}

static AIAgentMessage _make_assistant_tool_call_message(const String &p_id, const String &p_tool_name) {
	AIAgentMessage message;
	message.role = AI_AGENT_ROLE_ASSISTANT;

	Dictionary call;
	call["id"] = p_id;
	call["tool_name"] = p_tool_name;
	call["arguments"] = Dictionary();

	Array tool_calls;
	tool_calls.push_back(call);
	message.metadata["tool_calls"] = tool_calls;
	return message;
}

static AIAgentMessage _make_tool_result_message(const String &p_id, const String &p_content) {
	AIAgentMessage message;
	message.role = AI_AGENT_ROLE_TOOL;
	message.content = p_content;
	message.metadata["tool_call_id"] = p_id;
	message.metadata["tool_name"] = "project.read_file";
	return message;
}

TEST_CASE("[Editor][AI] Best practices context provider injects static guidance document") {
	Ref<AIBestPracticesContextProvider> provider;
	provider.instantiate();
	provider->set_max_chars(2048);

	Array context_documents = provider->collect_context();

	REQUIRE(context_documents.size() == 1);
	Dictionary document = context_documents[0];
	CHECK(String(document["title"]) == "Agent Best Practices");
	CHECK(String(document["source"]).contains("best_practices.md"));
	CHECK(String(document["content"]).contains("Godot"));
	CHECK(String(document["content"]).contains("confirm the game design"));
	CHECK(String(document["content"]).contains("separate scenes"));
}

TEST_CASE("[Editor][AI] System prompt prioritizes clarification planning and project architecture") {
	const String prompt = String(AIAgentPrompts::SYSTEM_PROMPT);

	CHECK(prompt.contains("Clarify before acting"));
	CHECK(prompt.contains("Ask mode"));
	CHECK(prompt.contains("Auto mode"));
	CHECK_FALSE(prompt.contains("Review mode"));
	CHECK_FALSE(prompt.contains("Write mode"));
	CHECK(prompt.contains("confirm design"));
	CHECK(prompt.contains("agent.manage_plan"));
	CHECK(prompt.contains("in_progress"));
	CHECK(prompt.contains("completed"));
	CHECK(prompt.contains("one scene"));
	CHECK(prompt.contains("one script"));
	CHECK_FALSE(prompt.contains("For scene.set_property, use property_path exactly"));
}

TEST_CASE("[Editor][AI] Session base exposes project scoped unique identifiers") {
	ExposedAISessionBase *session = memnew(ExposedAISessionBase);

	const String scope_key = session->get_project_scope_key_for_test();
	const String id = session->make_unique_id_for_test();
	const String prefixed_id = session->make_unique_id_for_test("agent_run");

	CHECK_FALSE(scope_key.is_empty());
	CHECK_FALSE(id.is_empty());
	CHECK(prefixed_id.begins_with("agent_run_"));
	CHECK(prefixed_id != id);

	memdelete(session);
}

TEST_CASE("[Editor][AI] Context manager trims history and tool output within budget") {
	Ref<AIContextManager> context_manager;
	context_manager.instantiate();
	context_manager->set_max_input_chars(900);
	context_manager->set_max_context_chars(160);
	context_manager->set_max_history_chars(420);
	context_manager->set_max_tool_result_chars(80);
	context_manager->set_min_recent_messages(2);

	Array context_documents;
	Dictionary project_tree;
	project_tree["title"] = "Project Tree";
	project_tree["source"] = "res://";
	project_tree["content"] = String("tree-entry\n").repeat(80);
	context_documents.push_back(project_tree);

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_message(AI_AGENT_ROLE_USER, "old user " + String("x").repeat(260)));
	messages.push_back(_make_message(AI_AGENT_ROLE_ASSISTANT, "old assistant " + String("y").repeat(260)));
	messages.push_back(_make_assistant_tool_call_message("call_context", "project.read_file"));
	messages.push_back(_make_tool_result_message("call_context", "tool result " + String("z").repeat(260)));
	messages.push_back(_make_message(AI_AGENT_ROLE_USER, "latest question"));

	AIContextBuildResult result = context_manager->build_messages("system prompt", messages, context_documents);

	CHECK((int)result.metadata.get("estimated_input_chars", 0) <= 900);
	CHECK((int)result.metadata.get("omitted_history_messages", 0) >= 2);
	CHECK((int)result.metadata.get("truncated_tool_results", 0) == 1);
	CHECK((int)result.metadata.get("truncated_context_documents", 0) == 1);

	bool saw_latest_user = false;
	bool saw_old_user = false;
	bool saw_tool_call = false;
	bool saw_tool_result = false;
	for (int i = 0; i < result.messages.size(); i++) {
		Dictionary message = result.messages[i];
		const String role = String(message.get("role", ""));
		const String content = String(message.get("content", ""));
		if (role == "user" && content.contains("latest question")) {
			saw_latest_user = true;
		}
		if (content.contains("old user")) {
			saw_old_user = true;
		}
		if (role == "assistant" && message.has("metadata") && Variant(message["metadata"]).get_type() == Variant::DICTIONARY) {
			Dictionary metadata = message["metadata"];
			if (metadata.has("tool_calls")) {
				saw_tool_call = true;
			}
		}
		if (role == "tool") {
			saw_tool_result = true;
			CHECK(content.length() <= 120);
			CHECK(content.contains("[truncated]"));
			REQUIRE(message.has("metadata"));
			Dictionary metadata = message["metadata"];
			CHECK(String(metadata.get("tool_call_id", "")) == "call_context");
			CHECK(bool(metadata.get("context_truncated", false)));
		}
	}

	CHECK(saw_latest_user);
	CHECK_FALSE(saw_old_user);
	CHECK(saw_tool_call);
	CHECK(saw_tool_result);
}

TEST_CASE("[Editor][AI] Agent base runs with bound tools prompt and model profile") {
	Ref<EchoRuntimeTool> echo_tool;
	echo_tool.instantiate();

	Ref<ScriptedRuntimeClient> client;
	client.instantiate();
	AIAgentRuntimeResponse final_response;
	final_response.content = "Ready.";
	client->push_response(final_response);

	Ref<AIAgentBase> agent;
	agent.instantiate();
	agent->set_runtime_client(client);
	agent->set_system_prompt("Custom agent prompt.");
	CHECK(agent->add_tool(echo_tool, AI_TOOL_PERMISSION_ALLOW));

	AIModelProfile model_profile;
	model_profile.provider_name = "Agent Test Provider";
	model_profile.model = "agent-test-model";
	model_profile.base_url = "https://agent.example.test/v1";
	model_profile.api_key = "agent-key";
	model_profile.max_provider_turns = 7;
	model_profile.max_tool_calls = 3;
	agent->set_model_profile(model_profile);

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_user_message("Use the agent object."));

	AIAgentRuntimeResult result = agent->run(messages);

	CHECK(result.success);
	CHECK(result.messages[result.messages.size() - 1].content == "Ready.");
	CHECK(agent->get_runtime()->get_max_provider_turns() == 7);
	CHECK(agent->get_runtime()->get_max_tool_calls() == 3);
	REQUIRE(client->last_tool_schemas.size() == 1);
	Dictionary schema = client->last_tool_schemas[0];
	Dictionary function_schema = schema["function"];
	CHECK(String(function_schema["name"]) == "test.echo");
	REQUIRE(client->last_messages.size() >= 1);
	Dictionary system_message = client->last_messages[0];
	CHECK(String(system_message.get("role", "")) == "system");
	CHECK(String(system_message.get("content", "")).contains("Custom agent prompt."));
}

TEST_CASE("[Editor][AI] Main agent owns the current editor tool set") {
	Ref<AIMainAgent> agent;
	agent.instantiate();

	CHECK(agent->get_profile().id == "ask");
	Ref<AIToolRegistry> registry = agent->get_tool_registry();
	REQUIRE(registry.is_valid());
	CHECK(registry->has_tool("project.read_file"));
	CHECK(registry->has_tool("script.write"));
	CHECK(registry->has_tool("shader.create"));
	CHECK(registry->has_tool("shader.edit"));
	CHECK(registry->has_tool("shader.delete"));
	CHECK(registry->has_tool("shader.apply_to_node"));
	CHECK(registry->get_tool_permission("project.read_file") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("script.write") == AI_TOOL_PERMISSION_DENY);
	CHECK(registry->get_available_tool_schemas().size() < registry->get_tool_schemas().size());

	agent->set_agent_profile_id("auto");
	CHECK(agent->get_profile().id == "auto");
	CHECK(registry->get_tool_permission("script.write") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("script.delete") == AI_TOOL_PERMISSION_ASK);

	agent->set_agent_profile_id("unknown");
	CHECK(agent->get_profile().id == "ask");
	CHECK(registry->get_tool_permission("script.write") == AI_TOOL_PERMISSION_DENY);
}

TEST_CASE("[Editor][AI] Agent runtime sends budgeted context to the provider client") {
	Ref<ScriptedRuntimeClient> client;
	client.instantiate();

	AIAgentRuntimeResponse final_response;
	final_response.content = "Ready.";
	client->push_response(final_response);

	Ref<AIToolRegistry> registry;
	registry.instantiate();

	Ref<AIContextManager> context_manager;
	context_manager.instantiate();
	context_manager->set_max_input_chars(700);
	context_manager->set_max_context_chars(128);
	context_manager->set_max_history_chars(260);
	context_manager->set_min_recent_messages(1);

	Ref<AIAgentRuntime> runtime;
	runtime.instantiate();
	runtime->set_client(client);
	runtime->set_tool_registry(registry);
	runtime->set_context_manager(context_manager);
	runtime->set_profile(_make_test_profile(false));

	Array context_documents;
	Dictionary project_tree;
	project_tree["title"] = "Project Tree";
	project_tree["source"] = "res://";
	project_tree["content"] = String("tree-entry\n").repeat(40);
	context_documents.push_back(project_tree);

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_message(AI_AGENT_ROLE_USER, "old request " + String("x").repeat(240)));
	messages.push_back(_make_message(AI_AGENT_ROLE_ASSISTANT, "old answer " + String("y").repeat(240)));
	messages.push_back(_make_message(AI_AGENT_ROLE_USER, "current request"));

	AIAgentRuntimeResult result = runtime->run(messages, context_documents);

	CHECK(result.success);
	CHECK(client->request_count == 1);
	REQUIRE(result.metadata.has("last_context"));
	Dictionary context_metadata = result.metadata["last_context"];
	CHECK((int)context_metadata.get("omitted_history_messages", 0) >= 2);

	bool saw_old_request = false;
	bool saw_current_request = false;
	for (int i = 0; i < client->last_messages.size(); i++) {
		Dictionary message = client->last_messages[i];
		const String content = String(message.get("content", ""));
		if (content.contains("old request")) {
			saw_old_request = true;
		}
		if (content.contains("current request")) {
			saw_current_request = true;
		}
	}

	CHECK_FALSE(saw_old_request);
	CHECK(saw_current_request);
}

TEST_CASE("[Editor][AI] Agent runtime executes allowed tool calls and continues the turn") {
	Ref<EchoRuntimeTool> echo_tool;
	echo_tool.instantiate();

	Ref<AIToolRegistry> registry;
	registry.instantiate();
	CHECK(registry->register_tool(echo_tool, AI_TOOL_PERMISSION_ALLOW));

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
	runtime->set_profile(_make_test_profile(false));

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

TEST_CASE("[Editor][AI] Agent runtime passes tool failures back to the provider") {
	Ref<FailingRuntimeTool> failing_tool;
	failing_tool.instantiate();

	Ref<AIToolRegistry> registry;
	registry.instantiate();
	CHECK(registry->register_tool(failing_tool, AI_TOOL_PERMISSION_ALLOW));

	Ref<ScriptedRuntimeClient> client;
	client.instantiate();

	AIAgentRuntimeResponse tool_response;
	AIToolCall call;
	call.id = "call_fail";
	call.tool_name = "test.fail";
	tool_response.tool_calls.push_back(call);
	client->push_response(tool_response);

	AIAgentRuntimeResponse final_response;
	final_response.content = "I saw the failure.";
	client->push_response(final_response);

	Ref<AIAgentRuntime> runtime;
	runtime.instantiate();
	runtime->set_client(client);
	runtime->set_tool_registry(registry);
	runtime->set_profile(_make_test_profile(false));

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_user_message("Try the failing tool."));

	AIAgentRuntimeResult result = runtime->run(messages);

	CHECK(result.success);
	CHECK(result.error.is_empty());
	CHECK(client->request_count == 2);
	CHECK(failing_tool->execute_count == 1);

	REQUIRE(result.tool_calls.size() == 1);
	CHECK(result.tool_calls[0].status == AI_TOOL_CALL_STATUS_FAILED);
	REQUIRE(result.messages.size() == 4);
	CHECK(result.messages[2].role == AI_AGENT_ROLE_TOOL);
	CHECK(result.messages[2].content.contains("Tool call failed for `test.fail`"));
	CHECK(result.messages[2].content.contains("simulated missing scene"));
	CHECK(String(result.messages[2].metadata["tool_call_id"]) == "call_fail");
	CHECK(String(result.messages[2].metadata["status"]) == "failed");

	bool saw_failure_tool_message = false;
	for (int i = 0; i < client->last_messages.size(); i++) {
		Dictionary message = client->last_messages[i];
		if (String(message.get("role", "")) != "tool") {
			continue;
		}

		const String content = String(message.get("content", ""));
		if (content.contains("simulated missing scene")) {
			saw_failure_tool_message = true;
			REQUIRE(message.has("metadata"));
			Dictionary metadata = message["metadata"];
			CHECK(String(metadata.get("tool_call_id", "")) == "call_fail");
			CHECK(String(metadata.get("status", "")) == "failed");
		}
	}

	CHECK(saw_failure_tool_message);
	CHECK(result.messages[3].role == AI_AGENT_ROLE_ASSISTANT);
	CHECK(result.messages[3].content == "I saw the failure.");
}

TEST_CASE("[Editor][AI] Agent runtime pauses when a tool requires approval") {
	Ref<EchoRuntimeTool> echo_tool;
	echo_tool.instantiate();

	Ref<AIToolRegistry> registry;
	registry.instantiate();
	CHECK(registry->register_tool(echo_tool, AI_TOOL_PERMISSION_ASK));

	Ref<ScriptedRuntimeClient> client;
	client.instantiate();

	AIAgentRuntimeResponse tool_response;
	AIToolCall call;
	call.id = "call_approval";
	call.tool_name = "test.echo";
	call.arguments["value"] = "delete request";
	tool_response.tool_calls.push_back(call);
	client->push_response(tool_response);

	Ref<AIAgentRuntime> runtime;
	runtime.instantiate();
	runtime->set_client(client);
	runtime->set_tool_registry(registry);
	runtime->set_profile(_make_test_profile(false));

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_user_message("Delete this script."));

	AIAgentRuntimeResult result = runtime->run(messages);

	CHECK(result.success);
	CHECK(result.error.is_empty());
	CHECK(echo_tool->execute_count == 0);
	REQUIRE(!result.pending_approval.is_empty());
	CHECK(String(result.pending_approval.get("id", "")) == "call_approval");
	CHECK(String(result.pending_approval.get("tool_name", "")) == "test.echo");
	CHECK(String(result.pending_approval.get("reason", "")).contains("approval"));
	REQUIRE(result.messages.size() == 2);
	CHECK(result.messages[1].role == AI_AGENT_ROLE_ASSISTANT);
	REQUIRE(result.messages[1].metadata.has("tool_calls"));
	Array tool_calls = result.messages[1].metadata["tool_calls"];
	REQUIRE(tool_calls.size() == 1);
	Dictionary pending_call = tool_calls[0];
	CHECK(String(pending_call.get("status", "")) == "pending");

	REQUIRE(client->last_tool_schemas.size() == 1);
	Dictionary schema = client->last_tool_schemas[0];
	Dictionary function_schema = schema["function"];
	CHECK(String(function_schema["name"]) == "test.echo");
}

TEST_CASE("[Editor][AI] Agent runtime exposes ask-gated tool schemas to the provider") {
	Ref<EchoRuntimeTool> echo_tool;
	echo_tool.instantiate();

	Ref<AIToolRegistry> registry;
	registry.instantiate();
	CHECK(registry->register_tool(echo_tool, AI_TOOL_PERMISSION_ASK));

	Ref<ScriptedRuntimeClient> client;
	client.instantiate();
	AIAgentRuntimeResponse final_response;
	final_response.content = "Ready.";
	client->push_response(final_response);

	Ref<AIAgentRuntime> runtime;
	runtime.instantiate();
	runtime->set_client(client);
	runtime->set_tool_registry(registry);
	runtime->set_profile(_make_test_profile(false));

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_user_message("May need approval."));

	AIAgentRuntimeResult result = runtime->run(messages);

	CHECK(result.success);
	REQUIRE(client->last_tool_schemas.size() == 1);
	Dictionary schema = client->last_tool_schemas[0];
	Dictionary function_schema = schema["function"];
	CHECK(String(function_schema["name"]) == "test.echo");
	CHECK(echo_tool->execute_count == 0);
}

TEST_CASE("[Editor][AI] Agent runtime reports tool progress before the final result") {
	Ref<EchoRuntimeTool> echo_tool;
	echo_tool.instantiate();

	Ref<AIToolRegistry> registry;
	registry.instantiate();
	CHECK(registry->register_tool(echo_tool, AI_TOOL_PERMISSION_ALLOW));

	Ref<ScriptedRuntimeClient> client;
	client.instantiate();

	AIAgentRuntimeResponse tool_response;
	AIToolCall call;
	call.id = "call_progress";
	call.tool_name = "test.echo";
	call.arguments["value"] = "progress context";
	tool_response.tool_calls.push_back(call);
	client->push_response(tool_response);

	AIAgentRuntimeResponse final_response;
	final_response.content = "Progress complete.";
	client->push_response(final_response);

	Ref<RuntimeProgressRecorder> recorder;
	recorder.instantiate();

	Ref<AIAgentRuntime> runtime;
	runtime.instantiate();
	runtime->set_client(client);
	runtime->set_tool_registry(registry);
	runtime->set_profile(_make_test_profile(false));
	runtime->set_progress_callbacks(callable_mp(recorder.ptr(), &RuntimeProgressRecorder::record_added), callable_mp(recorder.ptr(), &RuntimeProgressRecorder::record_updated));

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_user_message("Inspect progress."));

	AIAgentRuntimeResult result = runtime->run(messages);

	CHECK(result.success);
	REQUIRE(recorder->added_messages.size() == 3);
	REQUIRE(recorder->added_indices.size() == 3);
	CHECK((int)recorder->added_indices[0] == 1);
	CHECK((int)recorder->added_indices[1] == 2);
	CHECK((int)recorder->added_indices[2] == 3);
	Dictionary first_added = recorder->added_messages[0];
	CHECK(String(first_added["role"]) == "assistant");
	REQUIRE(first_added.has("metadata"));
	Dictionary first_metadata = first_added["metadata"];
	REQUIRE(first_metadata.has("tool_calls"));
	Array first_tool_calls = first_metadata["tool_calls"];
	REQUIRE(first_tool_calls.size() == 1);
	Dictionary first_call = first_tool_calls[0];
	CHECK(String(first_call["status"]) == "pending");

	REQUIRE(recorder->updated_messages.size() >= 2);
	Dictionary running_update = recorder->updated_messages[0];
	Dictionary running_metadata = running_update["metadata"];
	Array running_tool_calls = running_metadata["tool_calls"];
	Dictionary running_call = running_tool_calls[0];
	CHECK(String(running_call["status"]) == "running");
	CHECK((int)recorder->updated_indices[0] == 1);

	Dictionary final_update = recorder->updated_messages[recorder->updated_messages.size() - 1];
	Dictionary final_metadata = final_update["metadata"];
	Array final_tool_calls = final_metadata["tool_calls"];
	Dictionary final_call = final_tool_calls[0];
	CHECK(String(final_call["status"]) == "completed");

	Dictionary second_added = recorder->added_messages[1];
	CHECK(String(second_added["role"]) == "tool");
	CHECK(String(second_added["content"]) == "progress context");
	Dictionary third_added = recorder->added_messages[2];
	CHECK(String(third_added["role"]) == "assistant");
	CHECK(String(third_added["content"]) == "Progress complete.");
}

TEST_CASE("[Editor][AI] Agent runtime reports provider tool argument parse failures without executing the tool") {
	Ref<EchoRuntimeTool> echo_tool;
	echo_tool.instantiate();

	Ref<AIToolRegistry> registry;
	registry.instantiate();
	CHECK(registry->register_tool(echo_tool, AI_TOOL_PERMISSION_ALLOW));

	Ref<ScriptedRuntimeClient> client;
	client.instantiate();

	AIAgentRuntimeResponse tool_response;
	AIToolCall call;
	call.id = "call_invalid_args";
	call.tool_name = "test.echo";
	call.status = AI_TOOL_CALL_STATUS_FAILED;
	call.arguments["_provider_tool_arguments_parse_error"] = "Failed to parse provider tool arguments.";
	call.arguments["_provider_tool_arguments"] = "{\"value\":";
	tool_response.tool_calls.push_back(call);
	client->push_response(tool_response);

	AIAgentRuntimeResponse final_response;
	final_response.content = "I will retry with valid arguments.";
	client->push_response(final_response);

	Ref<AIAgentRuntime> runtime;
	runtime.instantiate();
	runtime->set_client(client);
	runtime->set_tool_registry(registry);
	runtime->set_profile(_make_test_profile(false));

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_user_message("Use the tool."));

	AIAgentRuntimeResult result = runtime->run(messages);

	CHECK(result.success);
	CHECK(echo_tool->execute_count == 0);
	REQUIRE(result.tool_calls.size() == 1);
	CHECK(result.tool_calls[0].status == AI_TOOL_CALL_STATUS_FAILED);
	REQUIRE(result.messages.size() == 4);
	CHECK(result.messages[2].role == AI_AGENT_ROLE_TOOL);
	CHECK(result.messages[2].content.contains("Failed to parse provider tool arguments"));
	CHECK(result.messages[3].content == "I will retry with valid arguments.");
}

TEST_CASE("[Editor][AI] Agent runtime streams assistant message updates before final result") {
	Ref<ScriptedRuntimeClient> client;
	client.instantiate();

	AIAgentRuntimeResponse first_partial;
	first_partial.content = "Hel";
	client->push_partial_response(first_partial);

	AIAgentRuntimeResponse second_partial;
	second_partial.content = "Hello";
	client->push_partial_response(second_partial);

	AIAgentRuntimeResponse final_response;
	final_response.content = "Hello world.";
	client->push_response(final_response);

	Ref<RuntimeProgressRecorder> recorder;
	recorder.instantiate();

	Ref<AIToolRegistry> registry;
	registry.instantiate();

	Ref<AIAgentRuntime> runtime;
	runtime.instantiate();
	runtime->set_client(client);
	runtime->set_tool_registry(registry);
	runtime->set_profile(_make_test_profile(false));
	runtime->set_progress_callbacks(callable_mp(recorder.ptr(), &RuntimeProgressRecorder::record_added), callable_mp(recorder.ptr(), &RuntimeProgressRecorder::record_updated));

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_user_message("Stream a reply."));

	AIAgentRuntimeResult result = runtime->run(messages);

	CHECK(result.success);
	REQUIRE(result.messages.size() == 2);
	CHECK(result.messages[1].content == "Hello world.");
	REQUIRE(recorder->added_messages.size() == 1);
	REQUIRE(recorder->added_indices.size() == 1);
	CHECK((int)recorder->added_indices[0] == 1);
	Dictionary added = recorder->added_messages[0];
	CHECK(String(added["role"]) == "assistant");
	CHECK(String(added["content"]) == "Hel");

	REQUIRE(recorder->updated_messages.size() >= 2);
	Dictionary first_update = recorder->updated_messages[0];
	CHECK(String(first_update["content"]) == "Hello");
	Dictionary final_update = recorder->updated_messages[recorder->updated_messages.size() - 1];
	CHECK(String(final_update["content"]) == "Hello world.");
}

TEST_CASE("[Editor][AI] Agent runtime aggregates provider token usage") {
	Ref<EchoRuntimeTool> echo_tool;
	echo_tool.instantiate();

	Ref<AIToolRegistry> registry;
	registry.instantiate();
	CHECK(registry->register_tool(echo_tool, AI_TOOL_PERMISSION_ALLOW));

	Ref<ScriptedRuntimeClient> client;
	client.instantiate();

	AIAgentRuntimeResponse tool_response;
	AIToolCall call;
	call.id = "call_usage";
	call.tool_name = "test.echo";
	call.arguments["value"] = "usage context";
	tool_response.tool_calls.push_back(call);
	Dictionary first_usage;
	first_usage["prompt_tokens"] = 10;
	first_usage["completion_tokens"] = 2;
	first_usage["total_tokens"] = 12;
	tool_response.metadata["usage"] = first_usage;
	client->push_response(tool_response);

	AIAgentRuntimeResponse final_response;
	final_response.content = "Usage recorded.";
	Dictionary second_usage;
	second_usage["prompt_tokens"] = 20;
	second_usage["completion_tokens"] = 5;
	second_usage["total_tokens"] = 25;
	final_response.metadata["usage"] = second_usage;
	client->push_response(final_response);

	Ref<AIAgentRuntime> runtime;
	runtime.instantiate();
	runtime->set_client(client);
	runtime->set_tool_registry(registry);
	runtime->set_profile(_make_test_profile(false));

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_user_message("Inspect usage."));

	AIAgentRuntimeResult result = runtime->run(messages);

	CHECK(result.success);
	REQUIRE(result.metadata.has("token_usage"));
	Dictionary usage = result.metadata["token_usage"];
	CHECK((int)usage.get("prompt_tokens", 0) == 30);
	CHECK((int)usage.get("completion_tokens", 0) == 7);
	CHECK((int)usage.get("total_tokens", 0) == 37);

	REQUIRE(result.messages.size() == 4);
	REQUIRE(result.messages[1].metadata.has("usage"));
	REQUIRE(result.messages[1].metadata.has("estimated_context_usage"));
	CHECK(result.messages[3].metadata.has("usage"));
	CHECK(result.messages[3].metadata.has("estimated_context_usage"));
}

TEST_CASE("[Editor][AI] Agent runtime denies disallowed tool calls without executing them") {
	Ref<EchoRuntimeTool> echo_tool;
	echo_tool.instantiate();

	Ref<AIToolRegistry> registry;
	registry.instantiate();
	CHECK(registry->register_tool(echo_tool, AI_TOOL_PERMISSION_DENY));

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
	runtime->set_profile(_make_test_profile(true));

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

	CHECK(runtime->get_max_provider_turns() == 255);
	CHECK(runtime->get_max_tool_calls() == 60);
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

TEST_CASE("[Editor][AI] Conversation store isolates conversations by project scope") {
	Ref<AIConversationStore> first_store;
	first_store.instantiate();
	first_store->set_project_scope("test_project_scope_a");

	Ref<AIConversationStore> second_store;
	second_store.instantiate();
	second_store->set_project_scope("test_project_scope_b");

	const String first_session_id = "test_project_scope_a_session";
	const String second_session_id = "test_project_scope_b_session";
	Vector<AIAgentMessage> first_messages;
	first_messages.push_back(_make_user_message("Project A message."));
	Vector<AIAgentMessage> second_messages;
	second_messages.push_back(_make_user_message("Project B message."));

	first_store->delete_conversation(first_session_id);
	first_store->delete_conversation(second_session_id);
	second_store->delete_conversation(first_session_id);
	second_store->delete_conversation(second_session_id);

	CHECK(first_store->save_conversation(first_session_id, "Project A", first_messages) == OK);
	CHECK(second_store->save_conversation(second_session_id, "Project B", second_messages) == OK);

	String title;
	Vector<AIAgentMessage> loaded_messages;
	CHECK(first_store->load_conversation(first_session_id, title, loaded_messages));
	CHECK_FALSE(first_store->load_conversation(second_session_id, title, loaded_messages));
	CHECK(second_store->load_conversation(second_session_id, title, loaded_messages));
	CHECK_FALSE(second_store->load_conversation(first_session_id, title, loaded_messages));

	CHECK(first_store->get_base_dir_for_test() != second_store->get_base_dir_for_test());

	first_store->delete_conversation(first_session_id);
	second_store->delete_conversation(second_session_id);
}

TEST_CASE("[Editor][AI] Agent session owns runtime dependencies with tool runtime enabled") {
	AIAgentSession *session = memnew(AIAgentSession);
	session->set_conversation_project_scope_for_test("test_project_scope_dependencies");

	CHECK(session->get_agent_profile_id() == "ask");
	REQUIRE(session->get_main_agent().is_valid());
	REQUIRE(session->get_agent_runtime().is_valid());
	REQUIRE(session->get_agent_runtime_runner().is_valid());
	REQUIRE(session->get_tool_registry().is_valid());
	CHECK(session->get_main_agent()->get_runtime() == session->get_agent_runtime());
	CHECK(session->get_main_agent()->get_tool_registry() == session->get_tool_registry());
	CHECK(session->get_agent_runtime_runner()->get_runtime() == session->get_agent_runtime());
	CHECK(session->is_tool_runtime_available());
	CHECK(session->get_tool_registry()->has_tool("agent.activate_skill"));
	CHECK(session->get_tool_registry()->has_tool("shader.create"));
	CHECK(session->get_tool_registry()->has_tool("shader.edit"));
	CHECK(session->get_tool_registry()->has_tool("shader.delete"));
	CHECK(session->get_tool_registry()->has_tool("shader.apply_to_node"));
	CHECK(session->get_tool_registry()->get_tool_permission("project.read_file") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(session->get_tool_registry()->get_tool_permission("script.write") == AI_TOOL_PERMISSION_DENY);

	session->set_agent_profile_id("auto");
	CHECK(session->get_agent_profile_id() == "auto");
	CHECK(session->get_agent_runtime()->get_profile().id == "auto");
	CHECK(session->get_agent_runtime()->get_profile().review_changes);
	CHECK(session->get_tool_registry()->get_tool_permission("script.write") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(session->get_tool_registry()->get_tool_permission("script.delete") == AI_TOOL_PERMISSION_ASK);

	session->set_agent_profile_id("unknown");
	CHECK(session->get_agent_profile_id() == "ask");
	CHECK(session->get_agent_runtime()->get_profile().id == "ask");
	CHECK_FALSE(session->get_agent_runtime()->get_profile().review_changes);
	CHECK(session->get_tool_registry()->get_tool_permission("script.write") == AI_TOOL_PERMISSION_DENY);

	memdelete(session);
}

TEST_CASE("[Editor][AI] Agent session applies provider advanced configuration to runtime") {
	AIAgentSession *session = memnew(AIAgentSession);
	session->set_conversation_project_scope_for_test("test_project_scope_provider_advanced_config");

	AIProviderConfig config;
	config.provider_name = "Advanced Test";
	config.model = "advanced-model";
	config.base_url = "https://advanced.example.test/v1";
	config.api_key = "advanced-key";
	config.timeout_seconds = 64;
	config.max_input_chars = 77777;
	config.max_context_chars = 11111;
	config.max_history_chars = 22222;
	config.max_tool_result_chars = 3333;
	config.min_recent_messages = 5;
	config.max_provider_turns = 13;
	config.max_tool_calls = 9;
	config.max_output_tokens = 2048;

	session->configure_provider(config);

	Ref<AIAgentRuntime> runtime = session->get_agent_runtime();
	REQUIRE(runtime.is_valid());
	CHECK(runtime->get_max_provider_turns() == 13);
	CHECK(runtime->get_max_tool_calls() == 9);

	Ref<AIContextManager> context_manager = runtime->get_context_manager();
	REQUIRE(context_manager.is_valid());
	CHECK(context_manager->get_max_input_chars() == 77777);
	CHECK(context_manager->get_max_context_chars() == 11111);
	CHECK(context_manager->get_max_history_chars() == 22222);
	CHECK(context_manager->get_max_tool_result_chars() == 3333);
	CHECK(context_manager->get_min_recent_messages() == 5);

	AIOpenAICompatibleRuntimeClient *runtime_client = Object::cast_to<AIOpenAICompatibleRuntimeClient>(*runtime->get_client());
	REQUIRE(runtime_client != nullptr);
	AIProviderConfig client_config = runtime_client->get_config();
	CHECK(client_config.model == "advanced-model");
	CHECK(client_config.timeout_seconds == 64);
	CHECK(client_config.max_output_tokens == 2048);

	session->delete_session(session->get_session_id());
	memdelete(session);
}

TEST_CASE("[Editor][AI] Agent session recalculates token usage from message metadata") {
	AIAgentSession *session = memnew(AIAgentSession);
	session->set_conversation_project_scope_for_test("test_project_scope_token_usage");

	AIAgentMessage user_message;
	user_message.role = AI_AGENT_ROLE_USER;
	user_message.content = "Hello";

	AIAgentMessage first_assistant;
	first_assistant.role = AI_AGENT_ROLE_ASSISTANT;
	first_assistant.content = "Hi";
	Dictionary first_usage;
	first_usage["prompt_tokens"] = 11;
	first_usage["completion_tokens"] = 3;
	first_usage["total_tokens"] = 14;
	first_assistant.metadata["usage"] = first_usage;

	AIAgentMessage second_assistant;
	second_assistant.role = AI_AGENT_ROLE_ASSISTANT;
	second_assistant.content = "Done";
	Dictionary second_usage;
	second_usage["prompt_tokens"] = 21;
	second_usage["completion_tokens"] = 4;
	second_usage["total_tokens"] = 25;
	second_assistant.metadata["usage"] = second_usage;

	Vector<AIAgentMessage> messages;
	messages.push_back(user_message);
	messages.push_back(first_assistant);
	messages.push_back(second_assistant);
	session->replace_messages_for_test(messages, -1);

	Dictionary usage = session->get_token_usage();
	CHECK((int)usage.get("prompt_tokens", 0) == 32);
	CHECK((int)usage.get("completion_tokens", 0) == 7);
	CHECK((int)usage.get("total_tokens", 0) == 39);
	CHECK((int)usage.get("message_count", 0) == 2);

	session->delete_session(session->get_session_id());
	memdelete(session);
}

TEST_CASE("[Editor][AI] Agent session deletes current conversation and starts clean session") {
	AIAgentSession *session = memnew(AIAgentSession);
	session->set_conversation_project_scope_for_test("test_project_scope_delete_current");
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

TEST_CASE("[Editor][AI] Agent session loads remaining conversation after deleting current one") {
	AIAgentSession *session = memnew(AIAgentSession);
	session->set_conversation_project_scope_for_test("test_project_scope_delete_current_load_remaining");

	Vector<AIAgentMessage> first_messages;
	first_messages.push_back(_make_user_message("First conversation."));
	session->replace_messages_for_test(first_messages, -1);
	CHECK(session->save_for_test() == OK);
	const String first_session_id = session->get_session_id();

	session->start_new_session();
	Vector<AIAgentMessage> second_messages;
	second_messages.push_back(_make_user_message("Second conversation."));
	session->replace_messages_for_test(second_messages, -1);
	CHECK(session->save_for_test() == OK);
	const String second_session_id = session->get_session_id();

	CHECK(session->delete_session(second_session_id));
	CHECK(session->get_session_id() == first_session_id);
	Array messages = session->get_messages_as_array();
	REQUIRE(messages.size() == 1);
	Dictionary message = messages[0];
	CHECK(String(message["content"]) == "First conversation.");

	session->delete_session(first_session_id);
	memdelete(session);
}

TEST_CASE("[Editor][AI] Agent session loads the most recent conversation for its project scope") {
	AIAgentSession *session = memnew(AIAgentSession);
	session->set_conversation_project_scope_for_test("test_project_scope_latest");
	const String saved_session_id = session->get_session_id();

	Vector<AIAgentMessage> messages;
	messages.push_back(_make_user_message("Restore this message."));
	session->replace_messages_for_test(messages, -1);
	CHECK(session->save_for_test() == OK);
	memdelete(session);

	AIAgentSession *restored_session = memnew(AIAgentSession);
	restored_session->set_conversation_project_scope_for_test("test_project_scope_latest");
	CHECK(restored_session->get_session_id() == saved_session_id);
	Array restored_messages = restored_session->get_messages_as_array();
	REQUIRE(restored_messages.size() == 1);
	Dictionary restored_message = restored_messages[0];
	CHECK(String(restored_message["content"]) == "Restore this message.");

	restored_session->delete_session(saved_session_id);
	memdelete(restored_session);
}

TEST_CASE("[Editor][AI] Agent session applies runtime results into message history") {
	AIAgentSession *session = memnew(AIAgentSession);
	session->set_conversation_project_scope_for_test("test_project_scope_apply_result");

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

	session->delete_session(session->get_session_id());
	memdelete(session);
}

TEST_CASE("[Editor][AI] Agent session maps runtime progress messages without duplicating final results") {
	AIAgentSession *session = memnew(AIAgentSession);
	session->set_conversation_project_scope_for_test("test_project_scope_runtime_progress_mapping");

	AIAgentMessage previous_user;
	previous_user.role = AI_AGENT_ROLE_USER;
	previous_user.content = "Previous task.";

	AIAgentMessage previous_assistant;
	previous_assistant.role = AI_AGENT_ROLE_ASSISTANT;
	previous_assistant.content = "Previous answer.";

	AIAgentMessage current_user;
	current_user.role = AI_AGENT_ROLE_USER;
	current_user.content = "Read a file.";

	AIAgentMessage assistant_placeholder;
	assistant_placeholder.role = AI_AGENT_ROLE_ASSISTANT;

	Vector<AIAgentMessage> original_messages;
	original_messages.push_back(previous_user);
	original_messages.push_back(previous_assistant);
	original_messages.push_back(current_user);
	original_messages.push_back(assistant_placeholder);
	session->replace_messages_for_test(original_messages, 3);

	AIToolCall call;
	call.id = "call_read";
	call.tool_name = "project.read_file";
	call.status = AI_TOOL_CALL_STATUS_PENDING;
	call.arguments["path"] = "res://player.gd";

	Array pending_calls;
	pending_calls.push_back(call.to_dict());
	AIAgentMessage assistant_tool_call;
	assistant_tool_call.role = AI_AGENT_ROLE_ASSISTANT;
	assistant_tool_call.metadata["tool_calls"] = pending_calls;

	session->add_runtime_message_for_test(3, assistant_tool_call);

	call.status = AI_TOOL_CALL_STATUS_RUNNING;
	Array running_calls;
	running_calls.push_back(call.to_dict());
	assistant_tool_call.metadata["tool_calls"] = running_calls;
	session->update_runtime_message_for_test(3, assistant_tool_call);

	call.status = AI_TOOL_CALL_STATUS_COMPLETED;
	Array completed_calls;
	completed_calls.push_back(call.to_dict());
	assistant_tool_call.metadata["tool_calls"] = completed_calls;
	session->update_runtime_message_for_test(3, assistant_tool_call);

	AIAgentMessage tool_message;
	tool_message.role = AI_AGENT_ROLE_TOOL;
	tool_message.content = "file contents";
	tool_message.metadata["tool_name"] = "project.read_file";
	tool_message.metadata["status"] = "completed";
	session->add_runtime_message_for_test(4, tool_message);

	AIAgentMessage final_assistant;
	final_assistant.role = AI_AGENT_ROLE_ASSISTANT;
	final_assistant.content = "I read the file.";

	AIAgentRuntimeResult runtime_result;
	runtime_result.success = true;
	runtime_result.messages.push_back(previous_user);
	runtime_result.messages.push_back(previous_assistant);
	runtime_result.messages.push_back(current_user);
	runtime_result.messages.push_back(assistant_tool_call);
	runtime_result.messages.push_back(tool_message);
	runtime_result.messages.push_back(final_assistant);
	session->apply_runtime_result_for_test(runtime_result);

	Array messages = session->get_messages_as_array();
	REQUIRE(messages.size() == 6);
	Dictionary previous_assistant_message = messages[1];
	CHECK(String(previous_assistant_message["content"]) == "Previous answer.");
	Dictionary tool_call_message = messages[3];
	CHECK(String(tool_call_message["role"]) == "assistant");
	REQUIRE(tool_call_message.has("metadata"));
	Dictionary tool_call_metadata = tool_call_message["metadata"];
	REQUIRE(tool_call_metadata.has("tool_calls"));
	Array tool_calls = tool_call_metadata["tool_calls"];
	REQUIRE(tool_calls.size() == 1);
	Dictionary mapped_call = tool_calls[0];
	CHECK(String(mapped_call["status"]) == "completed");
	Dictionary mapped_tool_message = messages[4];
	CHECK(String(mapped_tool_message["role"]) == "tool");
	CHECK(String(mapped_tool_message["content"]) == "file contents");
	Dictionary final_message = messages[5];
	CHECK(String(final_message["role"]) == "assistant");
	CHECK(String(final_message["content"]) == "I read the file.");

	session->delete_session(session->get_session_id());
	memdelete(session);
}

TEST_CASE("[Editor][AI] Agent session applies runtime failures as error messages") {
	AIAgentSession *session = memnew(AIAgentSession);
	session->set_conversation_project_scope_for_test("test_project_scope_apply_failure");

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

	session->delete_session(session->get_session_id());
	memdelete(session);
}

TEST_CASE("[Editor][AI] OpenAI-compatible codec builds non-streaming tool request bodies") {
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

	String body_text = String::utf8(reinterpret_cast<const char *>(AIOpenAICompatibleCodec::build_body(messages, "test-model", registry->get_tool_schemas(), false).ptr()));
	CHECK(body_text.contains("\"model\":\"test-model\""));
	CHECK(body_text.contains("\"stream\":false"));
	CHECK(body_text.contains("\"tools\""));
	CHECK(body_text.contains("\"test.echo\""));
	CHECK_FALSE(body_text.contains("\"max_tokens\""));

	String limited_body_text = String::utf8(reinterpret_cast<const char *>(AIOpenAICompatibleCodec::build_body(messages, "test-model", registry->get_tool_schemas(), false, 4096).ptr()));
	CHECK(limited_body_text.contains("\"max_tokens\":4096"));
}

TEST_CASE("[Editor][AI] OpenAI-compatible codec parses native tool call responses") {
	const String response_text = "{\"choices\":[{\"message\":{\"content\":null,\"tool_calls\":[{\"id\":\"call_read\",\"type\":\"function\",\"function\":{\"name\":\"project.read_file\",\"arguments\":\"{\\\"path\\\":\\\"res://player.gd\\\"}\"}}]},\"finish_reason\":\"tool_calls\"}]}";

	AIAgentRuntimeResponse response;
	String error;
	CHECK(AIOpenAICompatibleCodec::parse_chat_completion(response_text, response, error));
	CHECK(error.is_empty());
	CHECK(response.content.is_empty());

	REQUIRE(response.tool_calls.size() == 1);
	CHECK(response.tool_calls[0].id == "call_read");
	CHECK(response.tool_calls[0].tool_name == "project.read_file");
	CHECK(String(response.tool_calls[0].arguments["path"]) == "res://player.gd");
}

TEST_CASE("[Editor][AI] OpenAI-compatible codec preserves reasoning content from tool call responses") {
	const String response_text = "{\"choices\":[{\"message\":{\"content\":null,\"reasoning_content\":\"I should inspect the project first.\",\"tool_calls\":[{\"id\":\"call_read\",\"type\":\"function\",\"function\":{\"name\":\"project.read_file\",\"arguments\":\"{\\\"path\\\":\\\"res://player.gd\\\"}\"}}]},\"finish_reason\":\"tool_calls\"}]}";

	AIAgentRuntimeResponse response;
	String error;
	CHECK(AIOpenAICompatibleCodec::parse_chat_completion(response_text, response, error));
	CHECK(error.is_empty());

	CHECK(String(response.metadata["reasoning_content"]) == "I should inspect the project first.");
	REQUIRE(response.tool_calls.size() == 1);
	CHECK(response.tool_calls[0].id == "call_read");
}

TEST_CASE("[Editor][AI] OpenAI-compatible codec parses native final assistant responses") {
	const String response_text = "{\"choices\":[{\"message\":{\"content\":\"Done.\"},\"finish_reason\":\"stop\"}]}";

	AIAgentRuntimeResponse response;
	String error;
	CHECK(AIOpenAICompatibleCodec::parse_chat_completion(response_text, response, error));
	CHECK(error.is_empty());
	CHECK(response.content == "Done.");
	CHECK(response.tool_calls.is_empty());
}

TEST_CASE("[Editor][AI] OpenAI-compatible codec parses provider token usage") {
	const String response_text = "{\"choices\":[{\"message\":{\"content\":\"Done.\"},\"finish_reason\":\"stop\"}],\"usage\":{\"prompt_tokens\":120,\"completion_tokens\":30,\"total_tokens\":150}}";

	AIAgentRuntimeResponse response;
	String error;
	CHECK(AIOpenAICompatibleCodec::parse_chat_completion(response_text, response, error));
	CHECK(error.is_empty());
	REQUIRE(response.metadata.has("usage"));
	Dictionary usage = response.metadata["usage"];
	CHECK((int)usage.get("prompt_tokens", 0) == 120);
	CHECK((int)usage.get("completion_tokens", 0) == 30);
	CHECK((int)usage.get("total_tokens", 0) == 150);
	CHECK(String(usage.get("source", "")) == "provider");
}

TEST_CASE("[Editor][AI] OpenAI-compatible codec ignores null stream deltas") {
	const String event_text = "{\"choices\":[{\"delta\":{\"content\":null},\"finish_reason\":null}]}";

	String delta;
	String finish_reason;
	String error;
	CHECK(AIOpenAICompatibleCodec::extract_delta(event_text, delta, finish_reason, error));
	CHECK(error.is_empty());
	CHECK(delta.is_empty());
	CHECK_FALSE(delta == "<null>");
	CHECK(finish_reason.is_empty());
	CHECK_FALSE(finish_reason == "<null>");
}

TEST_CASE("[Editor][AI] OpenAI-compatible stream accumulator assembles content and tool calls") {
	AIOpenAICompatibleStreamAccumulator accumulator;
	AIOpenAIStreamParseResult result;

	CHECK(accumulator.apply_event("{\"choices\":[{\"delta\":{\"content\":\"Hel\"},\"finish_reason\":null}]}", result));
	CHECK(result.has_delta);
	CHECK(result.response.content == "Hel");

	CHECK(accumulator.apply_event("{\"choices\":[{\"delta\":{\"content\":\"lo\"},\"finish_reason\":null}]}", result));
	CHECK(result.response.content == "Hello");

	CHECK(accumulator.apply_event("{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_read\",\"type\":\"function\",\"function\":{\"name\":\"project_read_file\",\"arguments\":\"{\\\"path\\\":\"}}]},\"finish_reason\":null}]}", result));
	CHECK(accumulator.apply_event("{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"\\\"res://player.gd\\\"}\"}}]},\"finish_reason\":\"tool_calls\"}]}", result));
	CHECK(result.done);
	CHECK(result.response.content == "Hello");
	REQUIRE(result.response.tool_calls.size() == 1);
	CHECK(result.response.tool_calls[0].id == "call_read");
	CHECK(result.response.tool_calls[0].tool_name == "project_read_file");
	CHECK(String(result.response.tool_calls[0].arguments["path"]) == "res://player.gd");
}

TEST_CASE("[Editor][AI] OpenAI-compatible stream accumulator keeps invalid tool arguments as a failed tool call") {
	AIOpenAICompatibleStreamAccumulator accumulator;
	AIOpenAIStreamParseResult result;

	CHECK(accumulator.apply_event("{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_plan\",\"type\":\"function\",\"function\":{\"name\":\"ai_next_manage_project\",\"arguments\":\"{\\\"action\\\":\"}}]},\"finish_reason\":null}]}", result));
	CHECK(accumulator.apply_event("{\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}", result));
	CHECK(result.done);
	CHECK(result.error.is_empty());
	REQUIRE(result.response.tool_calls.size() == 1);
	CHECK(result.response.tool_calls[0].id == "call_plan");
	CHECK(result.response.tool_calls[0].tool_name == "ai_next_manage_project");
	CHECK(result.response.tool_calls[0].status == AI_TOOL_CALL_STATUS_FAILED);
	CHECK(String(result.response.tool_calls[0].arguments.get("_provider_tool_arguments_parse_error", String())).contains("Failed to parse provider tool arguments"));
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

TEST_CASE("[Editor][AI] OpenAI-compatible runtime client streams provider deltas") {
	Ref<FakeOpenAIRuntimeTransport> transport;
	transport.instantiate();
	transport->stream_supported = true;
	transport->stream_events.push_back("{\"choices\":[{\"delta\":{\"content\":\"Hel\"},\"finish_reason\":null}]}");
	transport->stream_events.push_back("{\"choices\":[{\"delta\":{\"content\":\"lo\"},\"finish_reason\":\"stop\"}],\"usage\":{\"prompt_tokens\":7,\"completion_tokens\":2,\"total_tokens\":9}}");
	transport->stream_events.push_back("[DONE]");

	AIProviderConfig config;
	config.provider_name = "Test";
	config.model = "test-model";
	config.base_url = "https://example.test/v1";
	config.api_key = "test-key";

	Ref<AIOpenAICompatibleRuntimeClient> client;
	client.instantiate();
	client->set_config(config);
	client->set_transport(transport);

	Ref<RuntimeProgressRecorder> recorder;
	recorder.instantiate();

	AIAgentRuntimeResponse response = client->complete_streaming(Array(), Array(), callable_mp(recorder.ptr(), &RuntimeProgressRecorder::record_partial));

	CHECK(response.error.is_empty());
	CHECK(response.content == "Hello");
	CHECK(transport->request_count == 1);
	REQUIRE(recorder->partial_responses.size() == 2);
	Dictionary first_partial = recorder->partial_responses[0];
	CHECK(String(first_partial["content"]) == "Hel");
	Dictionary second_partial = recorder->partial_responses[1];
	CHECK(String(second_partial["content"]) == "Hello");
	REQUIRE(response.metadata.has("usage"));
	Dictionary usage = response.metadata["usage"];
	CHECK((int)usage.get("total_tokens", 0) == 9);
}

TEST_CASE("[Editor][AI] OpenAI-compatible runtime client maps streamed tool calls") {
	Ref<FakeOpenAIRuntimeTransport> transport;
	transport.instantiate();
	transport->stream_supported = true;
	transport->stream_events.push_back("{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_read\",\"type\":\"function\",\"function\":{\"name\":\"project_read_file\",\"arguments\":\"{\\\"path\\\":\"}}]},\"finish_reason\":null}]}");
	transport->stream_events.push_back("{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"\\\"res://player.gd\\\"}\"}}]},\"finish_reason\":\"tool_calls\"}]}");
	transport->stream_events.push_back("[DONE]");

	AIProviderConfig config;
	config.provider_name = "Test";
	config.model = "test-model";
	config.base_url = "https://example.test/v1";
	config.api_key = "test-key";

	Ref<AIOpenAICompatibleRuntimeClient> client;
	client.instantiate();
	client->set_config(config);
	client->set_transport(transport);

	Dictionary function;
	function["name"] = "project.read_file";
	Dictionary parameters;
	parameters["type"] = "object";
	function["parameters"] = parameters;
	Dictionary schema;
	schema["type"] = "function";
	schema["function"] = function;
	Array tool_schemas;
	tool_schemas.push_back(schema);

	AIAgentRuntimeResponse response = client->complete_streaming(Array(), tool_schemas, Callable());

	CHECK(response.error.is_empty());
	REQUIRE(response.tool_calls.size() == 1);
	CHECK(response.tool_calls[0].id == "call_read");
	CHECK(response.tool_calls[0].tool_name == "project.read_file");
	CHECK(String(response.tool_calls[0].arguments["path"]) == "res://player.gd");
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

TEST_CASE("[Editor][AI] OpenAI-compatible runtime client passes assistant reasoning content back to provider") {
	Array messages;

	Dictionary assistant_message;
	assistant_message["role"] = "assistant";
	assistant_message["content"] = "";

	Dictionary metadata;
	metadata["reasoning_content"] = "I should inspect the project first.";
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

	Array chat_messages = AIOpenAICompatibleRuntimeClient::build_chat_messages_for_test(messages);

	REQUIRE(chat_messages.size() == 1);
	Dictionary converted_assistant = chat_messages[0];
	CHECK(String(converted_assistant["role"]) == "assistant");
	CHECK(String(converted_assistant["reasoning_content"]) == "I should inspect the project first.");
	REQUIRE(converted_assistant.has("tool_calls"));
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
	CHECK(registry->register_tool(echo_tool, AI_TOOL_PERMISSION_ALLOW));

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
	runtime->set_profile(_make_test_profile(false));
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
	CHECK(registry->register_tool(echo_tool, AI_TOOL_PERMISSION_ALLOW));

	Ref<ScriptedRuntimeClient> client;
	client.instantiate();

	AIAgentRuntimeResponse final_response;
	final_response.content = "Ready.";
	client->push_response(final_response);

	Ref<AIAgentRuntime> runtime;
	runtime.instantiate();
	runtime->set_client(client);
	runtime->set_tool_registry(registry);
	runtime->set_profile(_make_test_profile(false));

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
