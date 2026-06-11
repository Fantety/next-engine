/**************************************************************************/
/*  test_agent_v1.cpp                                                     */
/**************************************************************************/

#include "tests/test_macros.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/math/color.h"
#include "core/object/callable_mp.h"
#include "editor/agent_v1/config/ai_config_service.h"
#include "editor/agent_v1/config/ai_local_settings_store.h"
#include "editor/agent_v1/core/runtime/ai_stream_sink.h"
#include "editor/agent_v1/core/testing/ai_fake_mcp_server.h"
#include "editor/agent_v1/domain/attachments/ai_attachment_model_part_builder.h"
#include "editor/agent_v1/domain/attachments/ai_attachment_blob_store.h"
#include "editor/agent_v1/domain/attachments/ai_attachment_resolver.h"
#include "editor/agent_v1/domain/context/ai_context_epoch_service.h"
#include "editor/agent_v1/domain/context/ai_context_epoch_store.h"
#include "editor/agent_v1/domain/context/ai_context_source_registry.h"
#include "editor/agent_v1/domain/events/ai_event_store.h"
#include "editor/agent_v1/domain/projection/ai_session_history.h"
#include "editor/agent_v1/permission/ai_permission_service.h"
#include "editor/agent_v1/runtime/ai_fake_llm_runtime.h"
#include "editor/agent_v1/session/service/ai_session_service.h"
#include "editor/agent_v1/tools/ai_builtin_tools_v1.h"
#include "editor/agent_v1/tools/ai_tool_registry_v1.h"
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

static String read_text_file(const String &p_path) {
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		CHECK(file.is_valid());
		CHECK(err == OK);
		return String();
	}
	return file->get_as_text();
}

static void write_bytes_file(const String &p_path, const PackedByteArray &p_bytes) {
	DirAccess::make_dir_recursive_absolute(p_path.get_base_dir());
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	REQUIRE(file.is_valid());
	REQUIRE(err == OK);
	if (!p_bytes.is_empty()) {
		REQUIRE(file->store_buffer(p_bytes.ptr(), p_bytes.size()));
	}
	file->flush();
}

static PackedByteArray make_test_png_bytes() {
	Ref<Image> image = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
	REQUIRE(image.is_valid());
	image->fill(Color(1, 0, 0, 1));
	PackedByteArray bytes = image->save_png_to_buffer();
	REQUIRE(!bytes.is_empty());
	return bytes;
}

static AISystemContext make_context_fixture(const String &p_text, const String &p_directory, const String &p_model) {
	AISystemContextSource source;
	source.domain = "test.fixture";
	source.text = p_text;
	source.required = true;
	source.available = true;
	source.priority = 0;
	source.metadata["directory"] = p_directory;
	source.metadata["model"] = p_model;

	Vector<AISystemContextSource> sources;
	sources.push_back(source);
	return AISystemContext::combine(sources);
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

class LargeOutputTool : public AIV1Tool {
	GDCLASS(LargeOutputTool, AIV1Tool);

public:
	String payload;

	virtual bool execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) override {
		(void)p_arguments;
		(void)p_context;
		(void)r_error;

		Dictionary structured;
		structured["text"] = payload;
		structured["marker"] = "structured-full-output";
		r_result = AIV1ToolExecutionResult::ok(structured, payload, structured);
		return true;
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

TEST_CASE("[Editor][AgentV1] Local persistence defaults stay under net.nextengine") {
	Ref<AIEventStore> event_store;
	event_store.instantiate();
	CHECK(event_store->get_base_dir().begins_with("user://net.nextengine/"));

	Ref<AIAttachmentBlobStore> attachment_store;
	attachment_store.instantiate();
	CHECK(attachment_store->get_base_dir().begins_with("user://net.nextengine/"));

	Ref<AISessionInputStore> input_store;
	input_store.instantiate();
	CHECK(input_store->get_base_dir().begins_with("user://net.nextengine/"));

	Ref<AISessionStore> session_store;
	session_store.instantiate();
	CHECK(session_store->get_base_dir().begins_with("user://net.nextengine/"));

	Ref<AILocalSettingsStore> local_settings;
	local_settings.instantiate();
	CHECK(local_settings->get_settings_path().begins_with("user://net.nextengine/"));

	Ref<AIConfigService> config;
	config.instantiate();
	CHECK(config->get_global_config_path().begins_with("user://net.nextengine/"));
	CHECK(config->get_account_config_path().begins_with("user://net.nextengine/"));
	CHECK(config->get_managed_config_path().begins_with("user://net.nextengine/"));
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

TEST_CASE("[Editor][AgentV1] Prompt admission resolves data URL attachments into durable blobs") {
	const String base = TestUtils::get_temp_path("agent_v1/phase7_admission");

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());
	service->get_attachment_blob_store()->set_base_dir(base.path_join("attachments"));

	Dictionary create;
	create["id"] = "phase7-admission";
	create["directory"] = base.path_join("workspace");
	REQUIRE(bool(service->create(create).get("success", false)));

	Dictionary attachment;
	attachment["type"] = "data";
	attachment["id"] = "att-data";
	attachment["name"] = "note.txt";
	attachment["mime"] = "text/plain";
	attachment["data_url"] = "data:text/plain;base64,aGVsbG8=";
	Array attachments;
	attachments.push_back(attachment);

	Dictionary prompt;
	prompt["session_id"] = "phase7-admission";
	prompt["message_id"] = "phase7-message";
	prompt["text"] = "read attachment";
	prompt["attachments"] = attachments;
	prompt["resume"] = false;
	Dictionary admitted = service->prompt(prompt);
	REQUIRE(bool(admitted.get("success", false)));

	const Vector<AIEventRow> events = service->get_event_store()->list("phase7-admission");
	REQUIRE(events.size() == 1);
	CHECK(events[0].type == AIDomainEventTypes::prompt_admitted());
	const String admitted_json = Variant(events[0].data).stringify();
	CHECK_FALSE(admitted_json.contains("data:text/plain"));
	CHECK_FALSE(admitted_json.contains("aGVsbG8="));

	const Dictionary prompt_data = events[0].data.get("prompt", Dictionary());
	const Array files = prompt_data.get("files", Array());
	REQUIRE(files.size() == 1);
	const Dictionary file = files[0];
	CHECK(String(file.get("path", String())).begins_with("blob_"));
	CHECK(String(file.get("mime", String())) == "text/plain");
}

TEST_CASE("[Editor][AgentV1] Prompt admission rejects mismatched data URL attachment MIME") {
	const String base = TestUtils::get_temp_path("agent_v1/phase7_bad_data_url");

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());
	service->get_attachment_blob_store()->set_base_dir(base.path_join("attachments"));

	Dictionary create;
	create["id"] = "phase7-bad-data";
	create["directory"] = base.path_join("workspace");
	REQUIRE(bool(service->create(create).get("success", false)));

	Dictionary attachment;
	attachment["type"] = "data";
	attachment["id"] = "att-bad-image";
	attachment["name"] = "not-image.png";
	attachment["mime"] = "image/png";
	attachment["data_url"] = "data:image/png;base64,aGVsbG8=";
	Array attachments;
	attachments.push_back(attachment);

	Dictionary prompt;
	prompt["session_id"] = "phase7-bad-data";
	prompt["message_id"] = "phase7-bad-data-message";
	prompt["text"] = "bad attachment";
	prompt["attachments"] = attachments;
	prompt["resume"] = false;
	const Dictionary admitted = service->prompt(prompt);
	CHECK_FALSE(bool(admitted.get("success", false)));
	CHECK(service->get_event_store()->list("phase7-bad-data").is_empty());
}

TEST_CASE("[Editor][AgentV1] Blob attachment references are session-bound and size-limited") {
	Ref<AIAttachmentBlobStore> store;
	store.instantiate();
	store->set_base_dir(TestUtils::get_temp_path("agent_v1/phase7_blob_scope"));

	Ref<AIAttachmentResolver> resolver;
	resolver.instantiate();
	resolver->set_blob_store(store);
	resolver->set_max_attachment_bytes(1024);

	Dictionary data_attachment;
	data_attachment["type"] = "data";
	data_attachment["id"] = "att-session-a";
	data_attachment["name"] = "note.txt";
	data_attachment["mime"] = "text/plain";
	data_attachment["data_url"] = "data:text/plain;base64,aGVsbG8=";

	AILocationRef location;
	location.workspace_id = "workspace-a";
	AIFileAttachment resolved;
	AIError error;
	REQUIRE(resolver->resolve_struct("session-a", location, data_attachment, resolved, error));

	Dictionary blob_attachment;
	blob_attachment["type"] = "blob";
	blob_attachment["id"] = "att-reuse";
	blob_attachment["blob_id"] = resolved.path;

	AIFileAttachment same_session;
	REQUIRE(resolver->resolve_struct("session-a", location, blob_attachment, same_session, error));
	CHECK(same_session.path == resolved.path);

	AIFileAttachment other_session;
	CHECK_FALSE(resolver->resolve_struct("session-b", location, blob_attachment, other_session, error));
	CHECK(error.kind == AI_ERROR_PERMISSION);

	PackedByteArray large_bytes;
	large_bytes.resize(2048);
	Dictionary source;
	source["session_id"] = "session-a";
	source["workspace_id"] = "workspace-a";
	source["name"] = "large.bin";
	AIAttachmentBlobRecord large_record;
	REQUIRE(store->put_bytes_struct(large_bytes, "application/octet-stream", source, large_record, error));

	Dictionary large_blob;
	large_blob["type"] = "blob";
	large_blob["id"] = "att-large";
	large_blob["blob_id"] = large_record.id;
	AIFileAttachment large_file;
	CHECK_FALSE(resolver->resolve_struct("session-a", location, large_blob, large_file, error));
	CHECK(error.kind == AI_ERROR_VALIDATION);
}

TEST_CASE("[Editor][AgentV1] Model part builder checks image capability and builds provider neutral data parts") {
	Ref<AIAttachmentBlobStore> store;
	store.instantiate();
	store->set_base_dir(TestUtils::get_temp_path("agent_v1/phase7_builder_blobs"));

	AIAttachmentBlobRecord record;
	AIError error;
	Dictionary source;
	source["name"] = "pixel.png";
	REQUIRE(store->put_bytes_struct(make_test_png_bytes(), "image/png", source, record, error));

	AIFileAttachment attachment;
	attachment.id = "att-image";
	attachment.path = record.id;
	attachment.name = "pixel.png";
	attachment.mime = "image/png";
	attachment.size_bytes = int64_t(record.size);
	attachment.metadata["blob_id"] = record.id;
	attachment.metadata["blob_hash"] = record.hash;
	attachment.metadata["source_path"] = "C:/not/sent/pixel.png";

	Ref<AIModelPartBuilder> builder;
	builder.instantiate();
	builder->set_blob_store(store);

	AIModelPart unsupported_part;
	CHECK_FALSE(builder->build_attachment_part_struct(attachment, Dictionary(), unsupported_part, error));
	CHECK(error.kind == AI_ERROR_UNAVAILABLE);
	CHECK(String(error.details.get("modality", String())) == "image");

	Dictionary modalities;
	Array input_modalities;
	input_modalities.push_back("text");
	input_modalities.push_back("image");
	modalities["input"] = input_modalities;
	Dictionary capabilities;
	capabilities["modalities"] = modalities;
	Dictionary provider_config;
	provider_config["capabilities"] = capabilities;

	AIModelPart part;
	REQUIRE(builder->build_attachment_part_struct(attachment, provider_config, part, error));
	CHECK(part.type == AI_MODEL_PART_IMAGE);
	CHECK(part.mime == "image/png");
	CHECK(part.data.begins_with("data:image/png;base64,"));
	CHECK_FALSE(part.metadata.has("source_path"));
	CHECK(String(part.metadata.get("blob_id", String())).begins_with("blob_"));
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

TEST_CASE("[Editor][AgentV1] Session runner sends path attachments as blob-backed model parts") {
	const String base = TestUtils::get_temp_path("agent_v1/phase7_runner");
	const String workspace = base.path_join("workspace");
	const String image_path = workspace.path_join("pixel.png");
	write_bytes_file(image_path, make_test_png_bytes());

	Ref<AIFakeLLMRuntime> runtime;
	runtime.instantiate();
	runtime->set_response_text("image accepted");

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());
	service->get_attachment_blob_store()->set_base_dir(base.path_join("attachments"));
	service->get_runtime_registry()->register_runtime("fake", runtime);

	Array input_modalities;
	input_modalities.push_back("text");
	input_modalities.push_back("image");
	Dictionary modalities;
	modalities["input"] = input_modalities;
	Dictionary capabilities;
	capabilities["modalities"] = modalities;
	Dictionary fake_provider;
	fake_provider["type"] = "fake";
	fake_provider["model"] = "fake-model";
	fake_provider["api_key"] = "phase7-secret";
	fake_provider["capabilities"] = capabilities;
	Dictionary providers;
	providers["fake"] = fake_provider;
	Dictionary override_config;
	override_config["default_provider"] = "fake";
	override_config["default_model"] = "fake-model";
	override_config["providers"] = providers;
	service->get_config_service()->set_runtime_override(override_config);

	Dictionary create;
	create["id"] = "phase7-runner";
	create["directory"] = workspace;
	REQUIRE(bool(service->create(create).get("success", false)));

	Dictionary attachment;
	attachment["type"] = "path";
	attachment["id"] = "att-path";
	attachment["name"] = "pixel.png";
	attachment["path"] = "pixel.png";
	Array attachments;
	attachments.push_back(attachment);

	Dictionary prompt;
	prompt["session_id"] = "phase7-runner";
	prompt["message_id"] = "phase7-runner-message";
	prompt["text"] = "describe the image";
	prompt["attachments"] = attachments;
	prompt["resume"] = false;
	REQUIRE(bool(service->prompt(prompt).get("success", false)));

	Dictionary run = service->get_session_runner()->run("phase7-runner", false);
	REQUIRE(bool(run.get("success", false)));
	CHECK(runtime->get_stream_call_count() == 1);

	const Dictionary last_request = runtime->get_last_request();
	const String request_json = Variant(last_request).stringify();
	CHECK(request_json.contains("data:image/png;base64,"));
	CHECK_FALSE(request_json.contains(image_path));

	bool saw_image_part = false;
	const Array messages = last_request.get("messages", Array());
	for (int i = 0; i < messages.size(); i++) {
		const Dictionary message = messages[i];
		const Array parts = message.get("parts", Array());
		for (int j = 0; j < parts.size(); j++) {
			const Dictionary part = parts[j];
			if (String(part.get("type", String())) == "image") {
				saw_image_part = true;
				CHECK(String(part.get("data", String())).begins_with("data:image/png;base64,"));
			}
		}
	}
	CHECK(saw_image_part);

	const Vector<AIEventRow> events = service->get_event_store()->list("phase7-runner");
	bool saw_redacted_request = false;
	for (int i = 0; i < events.size(); i++) {
		const String event_json = Variant(events[i].data).stringify();
		CHECK_FALSE(event_json.contains(image_path));
		CHECK_FALSE(event_json.contains("phase7-secret"));
		if (events[i].type != AIDomainEventTypes::step_started()) {
			continue;
		}
		saw_redacted_request = saw_redacted_request || event_json.contains("[redacted attachment data]");
		CHECK(event_json.contains("[redacted]"));
		CHECK_FALSE(event_json.contains("data:image/png;base64,"));
	}
	CHECK(saw_redacted_request);
}

TEST_CASE("[Editor][AgentV1] Permission service records pending and rejected replies") {
	Ref<AIEventStore> event_store;
	event_store.instantiate();
	event_store->set_base_dir(TestUtils::get_temp_path("agent_v1/permission_events"));

	Ref<AIPermissionService> permission_service;
	permission_service.instantiate();
	permission_service->set_event_store(event_store);

	Dictionary source;
	source["assistant_message_id"] = "assistant-a";
	source["call_id"] = "call-a";
	source["tool"] = "file_write";

	Dictionary input;
	input["session_id"] = "session-permission";
	input["action"] = "file.write";
	input["resource"] = "res://generated.txt";
	input["source"] = source;

	AIPermissionDecision decision;
	AIError error;
	CHECK_FALSE(permission_service->assert_permission_struct(input, decision, error));
	CHECK(decision.pending);
	CHECK(decision.request_id.begins_with("perm_"));
	CHECK(event_store->list("session-permission").size() == 1);
	CHECK(event_store->list("session-permission")[0].type == AIDomainEventTypes::permission_asked());

	Dictionary reply;
	reply["request_id"] = decision.request_id;
	reply["reply"] = "reject";
	reply["reason"] = "not now";
	AIPermissionDecision reply_decision;
	CHECK_FALSE(permission_service->reply_struct(reply, reply_decision, error));
	CHECK(reply_decision.denied);
	CHECK(event_store->list("session-permission").size() == 2);
	CHECK(event_store->list("session-permission")[1].type == AIDomainEventTypes::permission_replied());

	AIPermissionDecision rejected_decision;
	CHECK_FALSE(permission_service->assert_permission_struct(input, rejected_decision, error));
	CHECK(rejected_decision.denied);
	CHECK(error.kind == AI_ERROR_PERMISSION);
}

TEST_CASE("[Editor][AgentV1] Permission rules use the last matching policy") {
	Ref<AIPermissionService> permission_service;
	permission_service.instantiate();

	Array rules;
	Dictionary deny_all_writes;
	deny_all_writes["action"] = "file.write";
	deny_all_writes["resource"] = "*";
	deny_all_writes["effect"] = "deny";
	deny_all_writes["reason"] = "default deny";
	rules.push_back(deny_all_writes);

	Dictionary allow_specific_write;
	allow_specific_write["action"] = "file.write";
	allow_specific_write["resource"] = "res://allowed.txt";
	allow_specific_write["effect"] = "allow";
	allow_specific_write["reason"] = "specific allow";
	rules.push_back(allow_specific_write);
	permission_service->set_rules(rules);

	Dictionary allowed_input;
	allowed_input["session_id"] = "session-permission-last";
	allowed_input["action"] = "file.write";
	allowed_input["resource"] = "res://allowed.txt";

	AIPermissionDecision allowed_decision;
	AIError error;
	CHECK(permission_service->assert_permission_struct(allowed_input, allowed_decision, error));
	CHECK(allowed_decision.allowed);
	CHECK(allowed_decision.effect == "allow");
	CHECK(allowed_decision.reason == "specific allow");

	Array deny_rules;
	Dictionary allow_all_writes;
	allow_all_writes["action"] = "file.write";
	allow_all_writes["resource"] = "*";
	allow_all_writes["effect"] = "allow";
	allow_all_writes["reason"] = "default allow";
	deny_rules.push_back(allow_all_writes);

	Dictionary deny_specific_write;
	deny_specific_write["action"] = "file.write";
	deny_specific_write["resource"] = "res://blocked.txt";
	deny_specific_write["effect"] = "deny";
	deny_specific_write["reason"] = "specific deny";
	deny_rules.push_back(deny_specific_write);
	permission_service->set_rules(deny_rules);

	Dictionary denied_input;
	denied_input["session_id"] = "session-permission-last";
	denied_input["action"] = "file.write";
	denied_input["resource"] = "res://blocked.txt";

	AIPermissionDecision denied_decision;
	CHECK_FALSE(permission_service->assert_permission_struct(denied_input, denied_decision, error));
	CHECK(denied_decision.denied);
	CHECK(denied_decision.effect == "deny");
	CHECK(denied_decision.reason == "specific deny");
	CHECK(error.kind == AI_ERROR_PERMISSION);

	Array wildcard_rules;
	Dictionary deny_file_namespace;
	deny_file_namespace["action"] = "file.*";
	deny_file_namespace["resource"] = "res://blocked/*";
	deny_file_namespace["effect"] = "deny";
	deny_file_namespace["reason"] = "wildcard deny";
	wildcard_rules.push_back(deny_file_namespace);
	permission_service->set_rules(wildcard_rules);

	Dictionary wildcard_input;
	wildcard_input["session_id"] = "session-permission-last";
	wildcard_input["action"] = "file.write";
	wildcard_input["resource"] = "res://blocked/generated.txt";

	AIPermissionDecision wildcard_decision;
	CHECK_FALSE(permission_service->assert_permission_struct(wildcard_input, wildcard_decision, error));
	CHECK(wildcard_decision.denied);
	CHECK(wildcard_decision.effect == "deny");
	CHECK(wildcard_decision.reason == "wildcard deny");
	CHECK(error.kind == AI_ERROR_PERMISSION);
}

TEST_CASE("[Editor][AgentV1] Tool registry settles read tools, pending writes, and stale calls") {
	const String root = TestUtils::get_temp_path("agent_v1/tools_root");
	const String read_path = root.path_join("readme.txt");
	const String write_path = root.path_join("generated.txt");
	write_text_file(read_path, "tool registry read");

	Ref<AIEventStore> event_store;
	event_store.instantiate();
	event_store->set_base_dir(TestUtils::get_temp_path("agent_v1/tool_events"));

	Ref<AISessionProjector> projector;
	projector.instantiate();

	Ref<AIPermissionService> permission_service;
	permission_service.instantiate();
	permission_service->set_event_store(event_store);

	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();
	registry->set_event_store(event_store);
	registry->set_projector(projector);
	registry->set_permission_service(permission_service);
	registry->set_root_dir(root);
	registry->register_builtin_tools();

	Ref<AIV1ToolMaterialization> materialization = registry->materialize_struct();
	CHECK(materialization->has_tool("file_read"));
	CHECK(materialization->has_tool("file_write"));
	CHECK(materialization->has_tool("shell_run"));

	Dictionary read_args;
	read_args["path"] = "readme.txt";
	Dictionary read_call;
	read_call["id"] = "call-read";
	read_call["name"] = "file_read";
	read_call["input"] = read_args;
	Dictionary read_settle_input;
	read_settle_input["session_id"] = "session-tools";
	read_settle_input["assistant_message_id"] = "assistant-tools";
	read_settle_input["call"] = read_call;

	AIV1ToolSettlement read_settlement;
	AIError error;
	REQUIRE(materialization->settle_struct(read_settle_input, read_settlement, error));
	CHECK(read_settlement.success);
	CHECK(String(Dictionary(read_settlement.structured).get("text", String())) == "tool registry read");

	Dictionary write_args;
	write_args["path"] = "generated.txt";
	write_args["content"] = "not written without approval";
	Dictionary write_call;
	write_call["id"] = "call-write";
	write_call["name"] = "file_write";
	write_call["input"] = write_args;
	Dictionary write_settle_input;
	write_settle_input["session_id"] = "session-tools";
	write_settle_input["assistant_message_id"] = "assistant-tools";
	write_settle_input["call"] = write_call;

	AIV1ToolSettlement write_settlement;
	REQUIRE(materialization->settle_struct(write_settle_input, write_settlement, error));
	CHECK_FALSE(write_settlement.success);
	CHECK(write_settlement.pending);
	CHECK_FALSE(FileAccess::exists(write_path));
	const Vector<AIEventRow> pending_events = event_store->list("session-tools");
	for (int i = 0; i < pending_events.size(); i++) {
		CHECK(pending_events[i].type != AIDomainEventTypes::tool_failed());
	}

	Array pending = permission_service->get_pending_requests();
	REQUIRE(pending.size() == 1);
	Dictionary reply;
	reply["request_id"] = Dictionary(pending[0]).get("request_id", String());
	reply["reply"] = "reject";
	(void)permission_service->reply(reply);

	AIV1ToolSettlement rejected_write_settlement;
	REQUIRE(materialization->settle_struct(write_settle_input, rejected_write_settlement, error));
	CHECK_FALSE(rejected_write_settlement.success);
	CHECK(rejected_write_settlement.error.kind == AI_ERROR_PERMISSION);

	Ref<AIV1ReadFileTool> replacement;
	replacement.instantiate();
	CHECK(registry->register_tool_struct("file_read", replacement, "test"));

	AIV1ToolSettlement stale_settlement;
	REQUIRE(materialization->settle_struct(read_settle_input, stale_settlement, error));
	CHECK_FALSE(stale_settlement.success);
	CHECK(stale_settlement.stale);

	const Vector<AIEventRow> events = event_store->list("session-tools");
	bool saw_success = false;
	bool saw_failed = false;
	for (int i = 0; i < events.size(); i++) {
		saw_success = saw_success || events[i].type == AIDomainEventTypes::tool_success();
		saw_failed = saw_failed || events[i].type == AIDomainEventTypes::tool_failed();
	}
	CHECK(saw_success);
	CHECK(saw_failed);
}

TEST_CASE("[Editor][AgentV1] Tool materialization captures root, filters denied tools, and restores scoped registrations") {
	const String root_a = TestUtils::get_temp_path("agent_v1/tools_root_a");
	const String root_b = TestUtils::get_temp_path("agent_v1/tools_root_b");
	write_text_file(root_a.path_join("readme.txt"), "root a");
	write_text_file(root_b.path_join("readme.txt"), "root b");

	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();
	registry->register_builtin_tools();

	Array rules;
	Dictionary deny_write;
	deny_write["action"] = "file.write";
	deny_write["resource"] = "*";
	deny_write["effect"] = "deny";
	rules.push_back(deny_write);

	Ref<AIV1ToolMaterialization> filtered = registry->materialize_struct(root_a, rules);
	CHECK(filtered->has_tool("file_read"));
	CHECK_FALSE(filtered->has_tool("file_write"));
	CHECK(filtered->has_tool("shell_run"));

	Ref<AIV1ToolMaterialization> materialization = registry->materialize_struct(root_a, Array());
	registry->set_root_dir(root_b);

	Dictionary read_args;
	read_args["path"] = "readme.txt";
	Dictionary read_call;
	read_call["id"] = "call-read-root";
	read_call["name"] = "file_read";
	read_call["input"] = read_args;
	Dictionary read_settle_input;
	read_settle_input["session_id"] = "session-root";
	read_settle_input["assistant_message_id"] = "assistant-root";
	read_settle_input["call"] = read_call;

	AIV1ToolSettlement read_settlement;
	AIError error;
	REQUIRE(materialization->settle_struct(read_settle_input, read_settlement, error));
	CHECK(read_settlement.success);
	CHECK(String(Dictionary(read_settlement.structured).get("text", String())) == "root a");

	Dictionary escaped_args;
	escaped_args["path"] = "res://project.godot";
	Dictionary escaped_call;
	escaped_call["id"] = "call-read-escaped";
	escaped_call["name"] = "file_read";
	escaped_call["input"] = escaped_args;
	Dictionary escaped_settle_input;
	escaped_settle_input["session_id"] = "session-root";
	escaped_settle_input["assistant_message_id"] = "assistant-root";
	escaped_settle_input["call"] = escaped_call;

	AIV1ToolSettlement escaped_settlement;
	REQUIRE(materialization->settle_struct(escaped_settle_input, escaped_settlement, error));
	CHECK_FALSE(escaped_settlement.success);
	CHECK(escaped_settlement.error.kind == AI_ERROR_PERMISSION);

	Ref<AIV1Tool> base_tool;
	base_tool.instantiate();
	Dictionary schema;
	schema["type"] = "object";
	base_tool->configure("Base scoped tool.", schema);
	CHECK(registry->register_tool_struct("scoped_tool", base_tool, "base"));
	const Dictionary base_identity = registry->get_tool_identity("scoped_tool");

	Ref<AIV1Tool> override_tool;
	override_tool.instantiate();
	override_tool->configure("Override scoped tool.", schema);
	Ref<AIScopedRegistration> scope = registry->register_tool_scope_struct("scoped_tool", override_tool, "override");
	REQUIRE(scope.is_valid());
	const Dictionary override_identity = registry->get_tool_identity("scoped_tool");
	CHECK(String(override_identity.get("id", String())) != String(base_identity.get("id", String())));

	scope->close();
	const Dictionary restored_identity = registry->get_tool_identity("scoped_tool");
	CHECK(String(restored_identity.get("id", String())) == String(base_identity.get("id", String())));
}

TEST_CASE("[Editor][AgentV1] Tool materialization applies last matching permission visibility") {
	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();
	registry->register_builtin_tools();

	Array allow_after_deny;
	Dictionary deny_write;
	deny_write["action"] = "file.write";
	deny_write["resource"] = "*";
	deny_write["effect"] = "deny";
	allow_after_deny.push_back(deny_write);

	Dictionary allow_write;
	allow_write["action"] = "file.write";
	allow_write["resource"] = "*";
	allow_write["effect"] = "allow";
	allow_after_deny.push_back(allow_write);

	Ref<AIV1ToolMaterialization> visible = registry->materialize_struct(String(), allow_after_deny);
	CHECK(visible->has_tool("file_write"));

	Array deny_after_allow;
	deny_after_allow.push_back(allow_write);
	deny_after_allow.push_back(deny_write);

	Ref<AIV1ToolMaterialization> hidden = registry->materialize_struct(String(), deny_after_allow);
	CHECK_FALSE(hidden->has_tool("file_write"));
	CHECK(hidden->has_tool("file_read"));
}

TEST_CASE("[Editor][AgentV1] Tool settlement bounds oversized model output and retains the full result") {
	Ref<AIEventStore> event_store;
	event_store.instantiate();
	event_store->set_base_dir(TestUtils::get_temp_path("agent_v1/tool_output_events"));

	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();
	registry->set_event_store(event_store);

	Ref<LargeOutputTool> tool;
	tool.instantiate();
	tool->payload = "phase7-output-head\n" + String("x").repeat(70000) + "\nphase7-output-tail";

	Dictionary schema;
	schema["type"] = "object";
	Dictionary metadata;
	metadata["action"] = "test.large_output";
	tool->configure("Returns an oversized output for settlement bounding tests.", schema, Callable(), metadata);
	CHECK(registry->register_tool_struct("large_output", tool, "test"));

	Ref<AIV1ToolMaterialization> materialization = registry->materialize_struct();
	REQUIRE(materialization->has_tool("large_output"));

	Dictionary call;
	call["id"] = "call-large-output";
	call["name"] = "large_output";
	call["input"] = Dictionary();

	Dictionary settle_input;
	settle_input["session_id"] = "session-large-output";
	settle_input["assistant_message_id"] = "assistant-large-output";
	settle_input["call"] = call;

	AIV1ToolSettlement settlement;
	AIError error;
	REQUIRE(materialization->settle_struct(settle_input, settlement, error));
	REQUIRE(settlement.success);

	const String model_content = String(settlement.content);
	CHECK(model_content.length() < tool->payload.length());
	CHECK(model_content.contains("phase7-output-head"));
	CHECK_FALSE(model_content.contains("phase7-output-tail"));

	Dictionary original_structured;
	original_structured["text"] = tool->payload;
	original_structured["marker"] = "structured-full-output";
	const String structured_json = Variant(settlement.structured).stringify();
	CHECK(structured_json.length() < Variant(original_structured).stringify().length());
	CHECK_FALSE(structured_json.contains("phase7-output-tail"));

	CHECK(bool(settlement.metadata.get("output_bounded", false)));
	CHECK(int64_t(settlement.metadata.get("output_original_bytes", 0)) > int64_t(model_content.to_utf8_buffer().size()));
	const String full_output_path = String(settlement.metadata.get("full_output_path", String()));
	REQUIRE(!full_output_path.is_empty());
	REQUIRE(FileAccess::exists(full_output_path));
	const String stored_output = read_text_file(full_output_path);
	CHECK(stored_output.contains("phase7-output-head"));
	CHECK(stored_output.contains("phase7-output-tail"));
	CHECK(stored_output.contains("structured-full-output"));

	const Vector<AIEventRow> events = event_store->list("session-large-output");
	REQUIRE(events.size() == 1);
	CHECK(events[0].type == AIDomainEventTypes::tool_success());
	const String event_json = Variant(events[0].data).stringify();
	CHECK(event_json.contains("output_bounded"));
	CHECK_FALSE(event_json.contains("phase7-output-tail"));
}

TEST_CASE("[Editor][AgentV1] Context epoch gates promotion and does not repeat unchanged system context") {
	const String base = TestUtils::get_temp_path("agent_v1/phase6_context_gate");

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());

	Dictionary create;
	create["id"] = "phase6-session";
	create["directory"] = base.path_join("workspace");
	Dictionary created = service->create(create);
	REQUIRE(bool(created.get("success", false)));

	Dictionary prompt;
	prompt["session_id"] = "phase6-session";
	prompt["message_id"] = "phase6-message";
	prompt["text"] = "hello phase 6";
	prompt["resume"] = false;
	Dictionary admitted = service->prompt(prompt);
	REQUIRE(bool(admitted.get("success", false)));

	service->get_context_source_registry()->set_blocked(true, "context offline");
	Dictionary blocked_run = service->get_session_runner()->run("phase6-session", false);
	CHECK_FALSE(bool(blocked_run.get("success", true)));

	Vector<AIEventRow> events = service->get_event_store()->list("phase6-session");
	for (int i = 0; i < events.size(); i++) {
		CHECK(events[i].type != AIDomainEventTypes::prompt_promoted());
		CHECK(events[i].type != AIDomainEventTypes::context_updated());
	}

	service->get_context_source_registry()->set_blocked(false);
	Dictionary first_run = service->get_session_runner()->run("phase6-session", false);
	REQUIRE(bool(first_run.get("success", false)));

	events = service->get_event_store()->list("phase6-session");
	int64_t context_seq = 0;
	int64_t promoted_seq = 0;
	int context_count = 0;
	for (int i = 0; i < events.size(); i++) {
		if (events[i].type == AIDomainEventTypes::context_updated()) {
			context_seq = events[i].seq;
			context_count++;
		} else if (events[i].type == AIDomainEventTypes::prompt_promoted()) {
			promoted_seq = events[i].seq;
		}
	}
	CHECK(context_count == 1);
	REQUIRE(context_seq > 0);
	REQUIRE(promoted_seq > 0);
	CHECK(context_seq < promoted_seq);

	Dictionary idle_run = service->get_session_runner()->run("phase6-session", false);
	CHECK(bool(idle_run.get("success", false)));
	events = service->get_event_store()->list("phase6-session");
	int idle_context_count = 0;
	for (int i = 0; i < events.size(); i++) {
		if (events[i].type == AIDomainEventTypes::context_updated()) {
			idle_context_count++;
		}
	}
	CHECK(idle_context_count == 1);

	Ref<AIContextEpochStore> fresh_epoch_store;
	fresh_epoch_store.instantiate();
	Ref<AISessionProjector> fresh_projector;
	fresh_projector.instantiate();
	service->set_context_epoch_store(fresh_epoch_store);
	service->set_projector(fresh_projector);

	Dictionary replay_idle_run = service->get_session_runner()->run("phase6-session", false);
	CHECK(bool(replay_idle_run.get("success", false)));

	events = service->get_event_store()->list("phase6-session");
	int replay_context_count = 0;
	for (int i = 0; i < events.size(); i++) {
		if (events[i].type == AIDomainEventTypes::context_updated()) {
			replay_context_count++;
		}
	}
	CHECK(replay_context_count == 1);

	AIContextEpoch hydrated_epoch;
	REQUIRE(fresh_epoch_store->get_epoch_struct("phase6-session", hydrated_epoch));
	CHECK(hydrated_epoch.baseline_seq == context_seq);
}

TEST_CASE("[Editor][AgentV1] Context source changes reconcile through context updated events") {
	const String base = TestUtils::get_temp_path("agent_v1/phase6_context_change");

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());

	Dictionary create;
	create["id"] = "phase6-change";
	create["directory"] = base.path_join("workspace");
	REQUIRE(bool(service->create(create).get("success", false)));

	Dictionary prompt;
	prompt["session_id"] = "phase6-change";
	prompt["message_id"] = "phase6-change-message";
	prompt["text"] = "first prompt";
	prompt["resume"] = false;
	REQUIRE(bool(service->prompt(prompt).get("success", false)));
	REQUIRE(bool(service->get_session_runner()->run("phase6-change", false).get("success", false)));

	Dictionary source;
	source["domain"] = "test.manual";
	source["text"] = "Manual context update for phase 6.";
	source["required"] = true;
	source["priority"] = 50;
	service->get_context_source_registry()->add_source(source);

	Dictionary reconcile = service->get_session_runner()->run("phase6-change", false);
	CHECK(bool(reconcile.get("success", false)));

	const Vector<AIEventRow> events = service->get_event_store()->list("phase6-change");
	int context_count = 0;
	bool saw_manual_context = false;
	for (int i = 0; i < events.size(); i++) {
		if (events[i].type == AIDomainEventTypes::context_updated()) {
			context_count++;
			saw_manual_context = saw_manual_context || String(events[i].data.get("text", String())).contains("Manual context update");
		}
	}
	CHECK(context_count == 2);
	CHECK(saw_manual_context);
}

TEST_CASE("[Editor][AgentV1] Context projector normalizes baseline seq from event row") {
	Ref<AISessionProjector> projector;
	projector.instantiate();

	AIContextEpoch epoch;
	epoch.session_id = "phase6-seq";
	epoch.baseline = "baseline";
	epoch.snapshot["snapshot_hash"] = "old-hash";
	epoch.agent_id = "main";
	epoch.baseline_seq = 2;
	epoch.revision = 1;

	Dictionary data;
	data["message_id"] = "system-seq";
	data["text"] = "baseline";
	data["epoch"] = epoch.to_dictionary();

	AIEventRow row;
	row.aggregate_id = "phase6-seq";
	row.seq = 7;
	row.type = AIDomainEventTypes::context_updated();
	row.data = data;

	REQUIRE(projector->project(row));

	AIContextEpoch projected_epoch;
	REQUIRE(projector->get_context_epoch_struct("phase6-seq", projected_epoch));
	CHECK(projected_epoch.baseline_seq == 7);
}

TEST_CASE("[Editor][AgentV1] Context current fence rejects changed source snapshot") {
	Ref<AIEventStore> event_store;
	event_store.instantiate();
	event_store->set_base_dir(String());

	Ref<AISessionProjector> projector;
	projector.instantiate();

	Ref<AIContextEpochService> epoch_service;
	epoch_service.instantiate();
	epoch_service->set_event_store(event_store);
	epoch_service->set_projector(projector);

	AILocationRef location;
	location.directory = "res://phase6";

	const AISystemContext initial_context = make_context_fixture("same privileged baseline", "res://phase6", "fake-model");
	AIContextEpoch epoch;
	bool initialized = false;
	AIError error;
	REQUIRE(epoch_service->initialize_struct("phase6-fence", location, "main", initial_context, epoch, initialized, error));
	REQUIRE(initialized);

	AIContextEpoch current_epoch;
	CHECK(epoch_service->current_struct("phase6-fence", "main", epoch.revision, initial_context, current_epoch, error));

	const AISystemContext changed_context = make_context_fixture("same privileged baseline", "res://phase6", "other-model");
	CHECK_FALSE(epoch_service->current_struct("phase6-fence", "main", epoch.revision, changed_context, current_epoch, error));
	CHECK(error.kind == AI_ERROR_CONFLICT);
	CHECK(bool(error.details.get("rebuild_request", false)));
}

TEST_CASE("[Editor][AgentV1] History selector trims without splitting tool content") {
	Vector<AISessionMessage> messages;

	AIPrompt prompt;
	prompt.text = "Large earlier prompt " + String("x").repeat(200);
	messages.push_back(AISessionMessage::user_message("phase6-history", 1, "user-history", prompt, 0));

	AISessionMessage assistant = AISessionMessage::assistant_shell("phase6-history", 2, "assistant-tool");
	Dictionary input;
	input["path"] = "res://file.txt";
	Dictionary output;
	output["text"] = "tool output that should stay attached";
	assistant.content.push_back(AIAssistantContent::tool_content("call-history", "file_read", AIToolState::success(input, output)));
	messages.push_back(assistant);

	const Vector<AISessionMessage> selected = AISessionHistory::entries_for_runner(messages, 0, 1);
	REQUIRE(selected.size() == 1);
	REQUIRE(selected[0].content.size() == 1);
	CHECK(selected[0].content[0].type == "tool");
	CHECK(selected[0].content[0].tool_state.status == AI_TOOL_STATUS_SUCCESS);
}

} // namespace TestAgentV1
