/**************************************************************************/
/*  test_agent_v1.cpp                                                     */
/**************************************************************************/

#include "tests/test_macros.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/callable_mp.h"
#include "editor/agent_v1/config/ai_config_service.h"
#include "editor/agent_v1/config/ai_local_settings_store.h"
#include "editor/agent_v1/core/runtime/ai_stream_sink.h"
#include "editor/agent_v1/core/testing/ai_fake_mcp_server.h"
#include "editor/agent_v1/domain/attachments/ai_attachment_blob_store.h"
#include "editor/agent_v1/domain/events/ai_event_store.h"
#include "editor/agent_v1/runtime/ai_fake_llm_runtime.h"
#include "editor/agent_v1/session/service/ai_session_service.h"
#include "tests/test_utils.h"

TEST_FORCE_LINK(test_agent_v1);

namespace TestAgentV1 {

static void write_text_file(const String &p_path, const String &p_text) {
	DirAccess::make_dir_recursive_absolute(p_path.get_base_dir());
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	REQUIRE(file.is_valid());
	REQUIRE(err == OK);
	file->store_string(p_text);
	file->flush();
}

class StreamCollector : public RefCounted {
	GDCLASS(StreamCollector, RefCounted);

public:
	Array events;

	bool handle_event(const Dictionary &p_event) {
		events.push_back(p_event);
		return false;
	}
};

TEST_CASE("[Editor][AgentV1] Event store writes versioned idempotent rows") {
	Ref<AIEventStore> store;
	store.instantiate();
	store->set_base_dir(TestUtils::get_temp_path("agent_v1/events"));

	Dictionary payload;
	payload["text"] = "hello";

	AIEventRow first;
	String error;
	CHECK(store->append_idempotent("session-a", AIDomainEventTypes::text_ended(), payload, false, "evt-key", first, error));
	CHECK(first.schema_version == AIEventRow::CURRENT_SCHEMA_VERSION);
	CHECK(first.idempotency_key == "evt-key");
	CHECK(first.seq == 1);

	AIEventRow retry;
	CHECK(store->append_idempotent("session-a", AIDomainEventTypes::text_ended(), payload, false, "evt-key", retry, error));
	CHECK(retry.id == first.id);
	CHECK(store->list("session-a").size() == 1);

	Dictionary conflicting = payload.duplicate(true);
	conflicting["text"] = "different";
	AIEventRow conflict_row;
	CHECK_FALSE(store->append_idempotent("session-a", AIDomainEventTypes::text_ended(), conflicting, false, "evt-key", conflict_row, error));
	CHECK(error.contains("idempotency conflict"));
}

TEST_CASE("[Editor][AgentV1] Attachment blob store separates metadata and bytes") {
	Ref<AIAttachmentBlobStore> store;
	store.instantiate();
	store->set_base_dir(TestUtils::get_temp_path("agent_v1/attachments"));

	Dictionary source;
	source["source"] = "unit-test";
	const PackedByteArray bytes = String("hello attachment").to_utf8_buffer();

	AIAttachmentBlobRecord record;
	AIError error;
	REQUIRE(store->put_bytes_struct(bytes, "text/plain", source, record, error));
	CHECK(!record.id.is_empty());
	CHECK(record.size == uint64_t(bytes.size()));
	CHECK(store->has_blob_struct(record.id));

	AIAttachmentBlobRecord loaded;
	REQUIRE(store->get_metadata_struct(record.hash, loaded, error));
	CHECK(loaded.hash == record.hash);
	CHECK(String(loaded.source_metadata.get("source", String())) == "unit-test");

	PackedByteArray loaded_bytes;
	REQUIRE(store->read_bytes_struct(record.id, loaded_bytes, error));
	CHECK(loaded_bytes == bytes);
}

TEST_CASE("[Editor][AgentV1] Config service discovers stable prioritized entries") {
	const String base = TestUtils::get_temp_path("agent_v1/config");

	Ref<AIConfigService> config;
	config.instantiate();
	config->set_global_config_path(base.path_join("global.jsonc"));
	config->set_project_config_path(base.path_join("project.jsonc"));
	config->set_opencode_config_path(base.path_join(".opencode/config.jsonc"));
	config->set_account_config_path(String());
	config->set_remote_config_path(String());
	config->set_managed_config_path(String());

	write_text_file(base.path_join("global.jsonc"), "{ \"default_model\": \"global-model\" }\n");
	write_text_file(base.path_join("project.jsonc"), "{ \"default_provider\": \"project-provider\", \"default_model\": \"project-model\" }\n");
	write_text_file(base.path_join(".opencode/config.jsonc"), "{\n  // JSONC comments and trailing commas are accepted.\n  \"default_model\": \"opencode-model\",\n}\n");

	const Dictionary effective = config->get_config();
	CHECK(bool(effective.get("success", true)));
	CHECK(String(effective.get("default_provider", String())) == "project-provider");
	CHECK(String(effective.get("default_model", String())) == "opencode-model");

	const Array entries = config->entries();
	CHECK(entries.size() >= 4);
	CHECK(String(Dictionary(entries[0]).get("source", String())) == "default");
	CHECK(String(Dictionary(entries[1]).get("source", String())) == "global");
	CHECK(String(Dictionary(entries[2]).get("source", String())) == "project");
	CHECK(String(Dictionary(entries[3]).get("source", String())) == "opencode");
}

TEST_CASE("[Editor][AgentV1] Local settings v3 stays separate from server config") {
	const String base = TestUtils::get_temp_path("agent_v1/local_settings");

	Ref<AILocalSettingsStore> local_settings;
	local_settings.instantiate();
	local_settings->set_settings_path(base.path_join("settings.v3.json"));

	Dictionary layout;
	layout["dock_width"] = 420;
	Dictionary patch;
	patch["theme"] = "dark";
	patch["layout"] = layout;

	Dictionary updated = local_settings->update_settings(patch);
	REQUIRE(bool(updated.get("success", false)));
	CHECK(int(updated.get("version", 0)) == 3);
	CHECK(String(updated.get("theme", String())) == "dark");

	Ref<AIConfigService> config;
	config.instantiate();
	config->set_global_config_path(base.path_join("global.jsonc"));
	config->set_project_config_path(String());
	config->set_opencode_config_path(String());
	config->set_account_config_path(String());
	config->set_remote_config_path(String());
	config->set_managed_config_path(String());

	Dictionary effective = config->get_config();
	CHECK(bool(effective.get("success", true)));
	CHECK_FALSE(effective.has("theme"));
	CHECK_FALSE(effective.has("layout"));
}

TEST_CASE("[Editor][AgentV1] Fake MCP server records deterministic tool calls") {
	Ref<AIFakeMCPServer> server;
	server.instantiate();

	Dictionary descriptor;
	descriptor["description"] = "Echo test value.";
	Dictionary expected;
	expected["content"] = "ok";

	server->register_tool_struct("test.echo", descriptor, expected);
	server->start();

	Dictionary input;
	input["value"] = "hello";
	Dictionary result = server->call_tool("test.echo", input);
	CHECK(bool(result.get("success", false)));
	CHECK(String(Dictionary(result.get("result", Dictionary())).get("content", String())) == "ok");
	CHECK(server->get_calls().size() == 1);
	CHECK(server->list_tools().size() == 1);
}

TEST_CASE("[Editor][AgentV1] Session prompt requires an existing session and resume false only admits") {
	const String base = TestUtils::get_temp_path("agent_v1/session");

	Ref<AISessionStore> sessions;
	sessions.instantiate();
	sessions->set_base_dir(base.path_join("sessions"));

	Ref<AIEventStore> events;
	events.instantiate();
	events->set_base_dir(base.path_join("events"));

	Ref<AISessionInputStore> inputs;
	inputs.instantiate();
	inputs->set_base_dir(base.path_join("inputs"));
	inputs->set_event_store(events);

	Ref<AISessionExecution> execution;
	execution.instantiate();

	Ref<AISessionService> service;
	service.instantiate();
	service->set_session_store(sessions);
	service->set_input_store(inputs);
	service->set_event_store(events);
	service->set_execution(execution);

	Dictionary prompt_without_session;
	prompt_without_session["directory"] = "res://";
	prompt_without_session["message_id"] = "msg-a";
	prompt_without_session["text"] = "hello";
	Dictionary rejected = service->prompt(prompt_without_session);
	CHECK_FALSE(bool(rejected.get("success", false)));

	Dictionary create_input;
	create_input["id"] = "session-a";
	create_input["directory"] = "res://";
	Dictionary created = service->create(create_input);
	REQUIRE(bool(created.get("success", false)));

	Dictionary prompt;
	prompt["session_id"] = "session-a";
	prompt["message_id"] = "msg-a";
	prompt["text"] = "hello";
	prompt["resume"] = false;
	Dictionary admitted = service->prompt(prompt);
	REQUIRE(bool(admitted.get("success", false)));
	CHECK_FALSE(bool(admitted.get("wake_scheduled", true)));
	CHECK(execution->get_state_struct("session-a").active == false);
	CHECK(events->list("session-a").size() == 1);
}

TEST_CASE("[Editor][AgentV1] Fake runtime streams text and provider-neutral tool calls") {
	Ref<AIFakeLLMRuntime> runtime;
	runtime.instantiate();
	runtime->set_response_text("fake text");

	Dictionary tool_input;
	tool_input["value"] = "42";
	runtime->set_tool_call("test.echo", tool_input);

	Ref<StreamCollector> collector;
	collector.instantiate();
	Ref<AICallableStreamSink> sink;
	sink.instantiate();
	sink->set_callback(callable_mp(collector.ptr(), &StreamCollector::handle_event));

	AIModelRequest request;
	request.request_id = "request-a";
	request.provider = "fake";
	request.model = "fake-model";
	request.messages.push_back(AIModelMessage::text_message("user", "hello", "msg-a"));

	AIError error;
	REQUIRE(runtime->stream_struct(request, sink, Ref<AICancelToken>(), error));
	CHECK(runtime->get_stream_call_count() == 1);

	bool saw_text = false;
	bool saw_tool = false;
	for (int i = 0; i < collector->events.size(); i++) {
		const Dictionary event = collector->events[i];
		if (String(event.get("type", String())) == "text-delta") {
			saw_text = true;
		}
		if (String(event.get("type", String())) == "tool-call") {
			saw_tool = true;
			CHECK(String(event.get("name", String())) == "test.echo");
			CHECK(String(Dictionary(event.get("input", Dictionary())).get("value", String())) == "42");
		}
	}
	CHECK(saw_text);
	CHECK(saw_tool);
}

} // namespace TestAgentV1
