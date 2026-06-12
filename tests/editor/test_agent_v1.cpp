/**************************************************************************/
/*  test_agent_v1.cpp                                                     */
/**************************************************************************/

#include "tests/test_macros.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/math/color.h"
#include "core/object/callable_mp.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "editor/agent_v1/config/ai_config_service.h"
#include "editor/agent_v1/config/ai_local_settings_store.h"
#include "editor/agent_v1/agents/ai_agent_service_v1.h"
#include "editor/agent_v1/core/runtime/ai_stream_sink.h"
#include "editor/agent_v1/core/runtime/ai_stream_event.h"
#include "editor/agent_v1/core/testing/ai_fake_mcp_server.h"
#include "editor/agent_v1/domain/attachments/ai_attachment_model_part_builder.h"
#include "editor/agent_v1/domain/attachments/ai_attachment_blob_store.h"
#include "editor/agent_v1/domain/attachments/ai_attachment_resolver.h"
#include "editor/agent_v1/domain/context/ai_context_epoch_service.h"
#include "editor/agent_v1/domain/context/ai_context_epoch_store.h"
#include "editor/agent_v1/domain/context/ai_context_source_registry.h"
#include "editor/agent_v1/domain/compaction/ai_compaction_service.h"
#include "editor/agent_v1/domain/events/ai_event_store.h"
#include "editor/agent_v1/domain/projection/ai_session_history.h"
#include "editor/agent_v1/mcp/ai_mcp_service_v1.h"
#include "editor/agent_v1/permission/ai_permission_service.h"
#include "editor/agent_v1/runtime/ai_fake_llm_runtime.h"
#include "editor/agent_v1/session/recovery/ai_startup_recovery.h"
#include "editor/agent_v1/session/service/ai_session_service.h"
#include "editor/agent_v1/skills/ai_skill_service_v1.h"
#include "editor/agent_v1/tools/ai_builtin_tools_v1.h"
#include "editor/agent_v1/tools/ai_tool_registry_v1.h"
#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_adapter.h"
#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_bridge.h"
#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_config_adapter.h"
#include "editor/agent_ui/component/ai_reference_resolver.h"
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

static int count_occurrences(const String &p_text, const String &p_pattern) {
	if (p_pattern.is_empty()) {
		return 0;
	}

	int count = 0;
	int from = 0;
	while (true) {
		const int found = p_text.find(p_pattern, from);
		if (found < 0) {
			return count;
		}
		count++;
		from = found + p_pattern.length();
	}
}

static void flush_deferred_calls() {
	if (MessageQueue::get_singleton()) {
		MessageQueue::get_singleton()->flush();
	}
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

static Dictionary make_mcp_server_config(const String &p_id, const String &p_name, const String &p_permission_default = "ask") {
	Dictionary config;
	config["id"] = p_id;
	config["name"] = p_name;
	config["enabled"] = true;
	config["transport"] = "fake";
	config["trust"] = "workspace";
	config["tool_visibility"] = "model";
	config["permission_default"] = p_permission_default;
	return config;
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

class AgentV1UIAdapterSignalCollector : public RefCounted {
	GDCLASS(AgentV1UIAdapterSignalCollector, RefCounted);

public:
	Array permission_requests;
	Array message_snapshots;
	Array run_states;

	void on_permission_requested(const Dictionary &p_request) {
		permission_requests.push_back(p_request.duplicate(true));
	}

	void on_messages_changed(const String &p_session_id, const Array &p_messages) {
		Dictionary snapshot;
		snapshot["session_id"] = p_session_id;
		snapshot["messages"] = p_messages.duplicate(true);
		message_snapshots.push_back(snapshot);
	}

	void on_run_state_changed(const Dictionary &p_state) {
		run_states.push_back(p_state.duplicate(true));
	}
};

class AgentV1UIConfigSignalCollector : public RefCounted {
	GDCLASS(AgentV1UIConfigSignalCollector, RefCounted);

public:
	Array model_snapshots;

	void on_models_changed(const Array &p_models) {
		model_snapshots.push_back(p_models.duplicate(true));
	}
};

class BlockingDrainRunner : public AISessionDrainRunner {
	GDCLASS(BlockingDrainRunner, AISessionDrainRunner);

public:
	int drain_count = 0;
	bool saw_cancel = false;

	virtual bool drain_struct(const String &p_session_id, const Ref<AICancelToken> &p_cancel_token, int64_t p_wake_seq, Vector<AISessionInputRecord> &r_promoted, AIError &r_error) override {
		(void)p_session_id;
		(void)p_wake_seq;
		(void)r_promoted;

		drain_count++;
		for (int i = 0; i < 1000; i++) {
			if (p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested()) {
				saw_cancel = true;
				r_error = AIError::make(AI_ERROR_INTERRUPTED, p_cancel_token->get_cancel_message("Blocking drain interrupted."));
				return false;
			}
			OS::get_singleton()->delay_usec(1000);
		}

		r_error = AIError::make(AI_ERROR_INTERNAL, "Blocking drain did not receive cancellation.");
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

class DeferredToolExecutionProbeTool : public AIV1Tool {
	GDCLASS(DeferredToolExecutionProbeTool, AIV1Tool);

public:
	Ref<AIEventStore> event_store;
	int execute_count = 0;
	bool saw_text_ended_before_execute = false;
	bool saw_step_ended_before_execute = false;

	virtual bool execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) override {
		(void)p_arguments;
		execute_count++;
		saw_text_ended_before_execute = false;
		saw_step_ended_before_execute = false;

		if (event_store.is_valid()) {
			const Vector<AIEventRow> events = event_store->list(p_context.session_id);
			for (int i = 0; i < events.size(); i++) {
				if (String(events[i].data.get("assistant_message_id", String())) != p_context.assistant_message_id) {
					continue;
				}
				saw_text_ended_before_execute = saw_text_ended_before_execute || events[i].type == AIDomainEventTypes::text_ended();
				saw_step_ended_before_execute = saw_step_ended_before_execute || events[i].type == AIDomainEventTypes::step_ended();
			}
		}

		Dictionary structured;
		structured["ok"] = true;
		structured["execute_count"] = execute_count;
		r_result = AIV1ToolExecutionResult::ok(structured, "deferred tool completed", structured);
		r_error = AIError::none();
		return true;
	}
};

class CoercedIntegerProbeTool : public AIV1Tool {
	GDCLASS(CoercedIntegerProbeTool, AIV1Tool);

public:
	bool saw_integer_argument = false;
	int max_depth = 0;

	virtual bool execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) override {
		(void)p_context;
		saw_integer_argument = p_arguments.get("max_depth", Variant()).get_type() == Variant::INT;
		max_depth = int(p_arguments.get("max_depth", 0));

		Dictionary structured;
		structured["max_depth"] = max_depth;
		structured["saw_integer_argument"] = saw_integer_argument;
		r_result = AIV1ToolExecutionResult::ok(structured, "ok", structured);
		r_error = AIError::none();
		return true;
	}
};

class DeferredToolProbeRuntime : public AILLMRuntime {
	GDCLASS(DeferredToolProbeRuntime, AILLMRuntime);

	bool _push_event(const Ref<AIStreamSink> &p_sink, const AIStreamEvent &p_event, AIError &r_error) const {
		bool stop_requested = false;
		String sink_error;
		if (!p_sink->push_event(p_event, stop_requested, sink_error)) {
			r_error = AIError::make(AI_ERROR_INTERNAL, sink_error);
			return false;
		}
		if (stop_requested) {
			r_error = AIError::make(AI_ERROR_CANCELLED, "Stream sink stopped deferred probe runtime.");
			return false;
		}
		return true;
	}

public:
	Ref<DeferredToolExecutionProbeTool> tool;
	int64_t stream_call_count = 0;
	int execute_count_after_tool_push = -1;

	virtual String get_runtime_type() const override {
		return "deferred-tool-probe";
	}

	virtual bool stream_struct(const AIModelRequest &p_request, const Ref<AIStreamSink> &p_sink, const Ref<AICancelToken> &p_cancel_token, AIError &r_error) override {
		stream_call_count++;
		if (p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested()) {
			r_error = AIError::make(AI_ERROR_INTERRUPTED, p_cancel_token->get_cancel_message("Deferred probe runtime interrupted."));
			return false;
		}
		if (p_sink.is_null()) {
			r_error = AIError::make(AI_ERROR_VALIDATION, "Deferred probe runtime requires a stream sink.");
			return false;
		}

		AIStreamEvent start = AIStreamEvent::step_start();
		start.id = p_request.request_id;
		if (!_push_event(p_sink, start, r_error)) {
			return false;
		}

		if (stream_call_count == 1) {
			if (!_push_event(p_sink, AIStreamEvent::text_delta("deferred-probe-text", "Need a tool."), r_error)) {
				return false;
			}
			AIStreamEvent tool_call = AIStreamEvent::tool_call("deferred-probe-call", "deferred_probe", Dictionary());
			if (!_push_event(p_sink, tool_call, r_error)) {
				return false;
			}
			execute_count_after_tool_push = tool.is_valid() ? tool->execute_count : -1;
		} else {
			if (!_push_event(p_sink, AIStreamEvent::text_delta("deferred-probe-final", "Tool result consumed."), r_error)) {
				return false;
			}
		}

		AIStreamEvent end;
		end.type = AI_STREAM_EVENT_STEP_END;
		end.id = p_request.request_id;
		if (!_push_event(p_sink, end, r_error)) {
			return false;
		}

		r_error = AIError::none();
		return true;
	}
};

class Phase10ParentRuntime : public AILLMRuntime {
	GDCLASS(Phase10ParentRuntime, AILLMRuntime);

public:
	Dictionary task_input;
	int64_t stream_call_count = 0;
	Dictionary last_request;

	virtual String get_runtime_type() const override {
		return "phase10-parent";
	}

	virtual bool stream_struct(const AIModelRequest &p_request, const Ref<AIStreamSink> &p_sink, const Ref<AICancelToken> &p_cancel_token, AIError &r_error) override {
		stream_call_count++;
		last_request = p_request.to_dictionary();
		if (p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested()) {
			r_error = AIError::make(AI_ERROR_INTERRUPTED, p_cancel_token->get_cancel_message("Parent runtime interrupted."));
			return false;
		}
		if (p_sink.is_null()) {
			r_error = AIError::make(AI_ERROR_VALIDATION, "Parent runtime requires a stream sink.");
			return false;
		}

		bool stop_requested = false;
		String sink_error;
		AIStreamEvent start = AIStreamEvent::step_start();
		start.id = p_request.request_id;
		if (!p_sink->push_event(start, stop_requested, sink_error)) {
			r_error = AIError::make(AI_ERROR_INTERNAL, sink_error);
			return false;
		}

		if (stream_call_count == 1) {
			AIStreamEvent text = AIStreamEvent::text_delta("phase10-parent-text", "Delegating task.");
			if (!p_sink->push_event(text, stop_requested, sink_error)) {
				r_error = AIError::make(AI_ERROR_INTERNAL, sink_error);
				return false;
			}
			AIStreamEvent tool_call = AIStreamEvent::tool_call("phase10-task-call", "task", task_input);
			if (!p_sink->push_event(tool_call, stop_requested, sink_error)) {
				r_error = AIError::make(AI_ERROR_INTERNAL, sink_error);
				return false;
			}
		} else {
			AIStreamEvent text = AIStreamEvent::text_delta("phase10-parent-final", "Parent consumed child result.");
			if (!p_sink->push_event(text, stop_requested, sink_error)) {
				r_error = AIError::make(AI_ERROR_INTERNAL, sink_error);
				return false;
			}
		}

		AIStreamEvent end;
		end.type = AI_STREAM_EVENT_STEP_END;
		end.id = p_request.request_id;
		if (!p_sink->push_event(end, stop_requested, sink_error)) {
			r_error = AIError::make(AI_ERROR_INTERNAL, sink_error);
			return false;
		}

		r_error = AIError::none();
		return !stop_requested;
	}
};

class Phase10CancelTokenRuntime : public AILLMRuntime {
	GDCLASS(Phase10CancelTokenRuntime, AILLMRuntime);

public:
	Ref<AICancelToken> expected_token;
	bool saw_expected_token = false;
	int64_t stream_call_count = 0;

	virtual String get_runtime_type() const override {
		return "phase10-cancel-token";
	}

	virtual bool stream_struct(const AIModelRequest &p_request, const Ref<AIStreamSink> &p_sink, const Ref<AICancelToken> &p_cancel_token, AIError &r_error) override {
		(void)p_request;
		stream_call_count++;
		saw_expected_token = expected_token.is_valid() && p_cancel_token.is_valid() && expected_token.ptr() == p_cancel_token.ptr();
		if (p_sink.is_null()) {
			r_error = AIError::make(AI_ERROR_VALIDATION, "Cancel token runtime requires a stream sink.");
			return false;
		}

		bool stop_requested = false;
		String sink_error;
		AIStreamEvent start = AIStreamEvent::step_start();
		start.id = p_request.request_id;
		if (!p_sink->push_event(start, stop_requested, sink_error)) {
			r_error = AIError::make(AI_ERROR_INTERNAL, sink_error);
			return false;
		}
		AIStreamEvent text = AIStreamEvent::text_delta("phase10-cancel-child-text", "child saw parent token");
		if (!p_sink->push_event(text, stop_requested, sink_error)) {
			r_error = AIError::make(AI_ERROR_INTERNAL, sink_error);
			return false;
		}
		AIStreamEvent end;
		end.type = AI_STREAM_EVENT_STEP_END;
		end.id = p_request.request_id;
		if (!p_sink->push_event(end, stop_requested, sink_error)) {
			r_error = AIError::make(AI_ERROR_INTERNAL, sink_error);
			return false;
		}
		r_error = AIError::none();
		return !stop_requested;
	}
};

static int phase10_child_session_count(const Ref<AISessionStore> &p_store, const String &p_parent_session_id) {
	int count = 0;
	const Array sessions = p_store->list_sessions();
	for (int i = 0; i < sessions.size(); i++) {
		if (sessions[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary session = sessions[i];
		const Dictionary metadata = session.get("metadata", Dictionary());
		if (String(metadata.get("parent_session_id", String())) == p_parent_session_id) {
			count++;
		}
	}
	return count;
}

static bool phase10_request_has_tool(const Dictionary &p_request, const String &p_tool_name) {
	const Array tools = p_request.get("tools", Array());
	for (int i = 0; i < tools.size(); i++) {
		if (tools[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		if (String(Dictionary(tools[i]).get("name", String())) == p_tool_name) {
			return true;
		}
	}
	return false;
}

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

TEST_CASE("[Editor][AgentV1] Session service scopes durable stores by project") {
	const String base = TestUtils::get_temp_path("agent_v1/project_scoped_storage");

	Ref<AISessionService> project_a;
	project_a.instantiate();
	project_a->set_project_scope("project-a", "res://", base);

	Ref<AISessionService> project_b;
	project_b.instantiate();
	project_b->set_project_scope("project-b", "res://", base);

	CHECK(project_a->get_project_scope_id() == "project-a");
	CHECK(project_b->get_project_scope_id() == "project-b");
	CHECK(project_a->get_session_store()->get_base_dir().contains("project-a"));
	CHECK(project_a->get_input_store()->get_base_dir().contains("project-a"));
	CHECK(project_a->get_event_store()->get_base_dir().contains("project-a"));
	CHECK(project_a->get_attachment_blob_store()->get_base_dir().contains("project-a"));
	CHECK(project_b->get_session_store()->get_base_dir().contains("project-b"));
	CHECK(project_a->get_session_store()->get_base_dir() != project_b->get_session_store()->get_base_dir());
	CHECK(project_a->get_event_store()->get_base_dir() != project_b->get_event_store()->get_base_dir());
	CHECK(project_a->get_attachment_blob_store()->get_base_dir() != project_b->get_attachment_blob_store()->get_base_dir());
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

TEST_CASE("[Editor][AgentUI] Inline file references resolve to attachments") {
	const Array attachments = AIReferenceResolver::resolve_attachments("Use @res://icon.png, @\"res://docs/my note.md\" and again @res://icon.png.");
	REQUIRE(attachments.size() == 2);

	REQUIRE(Variant(attachments[0]).get_type() == Variant::DICTIONARY);
	const Dictionary image = attachments[0];
	CHECK(String(image.get("type", String())) == "image");
	CHECK(String(image.get("source", String())) == "file");
	CHECK(String(image.get("path", String())) == "res://icon.png");
	CHECK(String(image.get("mime_type", String())) == "image/png");
	CHECK(String(image.get("detail", String())) == "auto");
	CHECK(bool(image.get("inline_reference", false)));

	REQUIRE(Variant(attachments[1]).get_type() == Variant::DICTIONARY);
	const Dictionary text = attachments[1];
	CHECK(String(text.get("type", String())) == "text");
	CHECK(String(text.get("source", String())) == "file");
	CHECK(String(text.get("path", String())) == "res://docs/my note.md");
	CHECK(String(text.get("mime_type", String())) == "text/markdown");
	CHECK(bool(text.get("inline_reference", false)));

	CHECK(AIReferenceResolver::make_reference_token_for_path("res://docs/my note.md") == "@\"res://docs/my note.md\"");
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

TEST_CASE("[Editor][AgentV1] MCP service discovers namespaced tools without overriding builtins") {
	Ref<AIFakeMCPServer> server;
	server.instantiate();

	Dictionary schema;
	schema["type"] = "object";
	Dictionary properties;
	Dictionary path_property;
	path_property["type"] = "string";
	properties["path"] = path_property;
	schema["properties"] = properties;
	Array required;
	required.push_back("path");
	schema["required"] = required;

	Dictionary descriptor;
	descriptor["description"] = "Read a file through MCP.";
	descriptor["inputSchema"] = schema;
	Dictionary result;
	result["content"] = "mcp read";
	server->register_tool_struct("read.file", descriptor, result);
	server->start();

	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();
	registry->register_builtin_tools();

	Ref<AIV1MCPService> service;
	service.instantiate();
	service->set_tool_registry(registry);
	REQUIRE(service->register_fake_server_for_test(make_mcp_server_config("filesystem server", "Filesystem", "allow"), server));

	AIError error;
	REQUIRE(service->refresh_struct(error));

	const String tool_name = AIV1MCPService::make_tool_name("filesystem server", "read.file");
	CHECK(tool_name == "mcp__filesystem_server__read_file");
	CHECK(registry->has_tool("file_read"));
	CHECK(registry->has_tool(tool_name));

	Ref<AIV1ToolMaterialization> materialization = registry->materialize_struct();
	CHECK(materialization->has_tool("file_read"));
	CHECK(materialization->has_tool(tool_name));

	const Dictionary identity = registry->get_tool_identity(tool_name);
	const Dictionary metadata = identity.get("metadata", Dictionary());
	CHECK(String(metadata.get("source", String())) == "mcp");
	CHECK(String(metadata.get("mcp_server_id", String())) == "filesystem server");
	CHECK(String(metadata.get("mcp_tool_name", String())) == "read.file");

	Array statuses = service->get_statuses();
	REQUIRE(statuses.size() == 1);
	CHECK(String(Dictionary(statuses[0]).get("state", String())) == "ready");

	Array snapshots = service->get_discovery_snapshots();
	REQUIRE(snapshots.size() == 1);
	CHECK(String(Dictionary(snapshots[0]).get("server_id", String())) == "filesystem server");
	CHECK(Array(Dictionary(snapshots[0]).get("tools", Array())).size() == 1);
}

TEST_CASE("[Editor][AgentV1] MCP service imports server definitions from app config") {
	Dictionary server_config = make_mcp_server_config("configured", "Configured", "ask");
	server_config["enabled"] = false;

	Dictionary servers;
	servers["configured"] = server_config;
	Dictionary mcp;
	mcp["servers"] = servers;
	Dictionary app_config;
	app_config["mcp"] = mcp;

	Ref<AIV1MCPService> service;
	service.instantiate();

	AIError error;
	REQUIRE(service->import_config_struct(app_config, error));
	Array statuses = service->get_statuses();
	REQUIRE(statuses.size() == 1);
	CHECK(String(Dictionary(statuses[0]).get("server_id", String())) == "configured");
	CHECK(String(Dictionary(statuses[0]).get("state", String())) == "disabled");
}

TEST_CASE("[Editor][AgentV1] MCP tool calls use unified permission and preserve original tool metadata") {
	Ref<AIFakeMCPServer> server;
	server.instantiate();

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

	Dictionary descriptor;
	descriptor["description"] = "Echo a value.";
	descriptor["inputSchema"] = schema;

	Dictionary text_content;
	text_content["type"] = "text";
	text_content["text"] = "approved result";
	Array content;
	content.push_back(text_content);
	Dictionary result;
	result["content"] = content;
	server->register_tool_struct("ask.echo", descriptor, result);
	server->start();

	Ref<AIEventStore> event_store;
	event_store.instantiate();
	event_store->set_base_dir(TestUtils::get_temp_path("agent_v1/mcp_permission_events"));

	Ref<AIPermissionService> permission_service;
	permission_service.instantiate();
	permission_service->set_event_store(event_store);

	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();
	registry->set_event_store(event_store);
	registry->set_permission_service(permission_service);

	Ref<AIV1MCPService> service;
	service.instantiate();
	service->set_tool_registry(registry);
	REQUIRE(service->register_fake_server_for_test(make_mcp_server_config("filesystem", "Filesystem", "ask"), server));

	AIError error;
	REQUIRE(service->refresh_struct(error));

	const String tool_name = AIV1MCPService::make_tool_name("filesystem", "ask.echo");
	Ref<AIV1ToolMaterialization> materialization = registry->materialize_struct();
	REQUIRE(materialization->has_tool(tool_name));

	Dictionary args;
	args["value"] = "hello";
	Dictionary call;
	call["id"] = "call-mcp-echo";
	call["name"] = tool_name;
	call["input"] = args;
	Dictionary settle_input;
	settle_input["session_id"] = "session-mcp-permission";
	settle_input["assistant_message_id"] = "assistant-mcp-permission";
	settle_input["call"] = call;

	AIV1ToolSettlement pending_settlement;
	REQUIRE(materialization->settle_struct(settle_input, pending_settlement, error));
	CHECK_FALSE(pending_settlement.success);
	CHECK(pending_settlement.pending);
	CHECK(server->get_calls().is_empty());

	Array pending = permission_service->get_pending_requests();
	REQUIRE(pending.size() == 1);
	Dictionary pending_request = pending[0];
	CHECK(String(pending_request.get("action", String())) == "mcp.filesystem.ask.echo");
	const String pending_resource = String(pending_request.get("resource", String()));
	CHECK(pending_resource.begins_with("filesystem/ask.echo?args_hash="));
	CHECK(pending_resource.contains("value"));
	CHECK(pending_resource.contains("hello"));

	Dictionary reply;
	reply["request_id"] = pending_request.get("request_id", String());
	reply["reply"] = "once";
	(void)permission_service->reply(reply);

	AIV1ToolSettlement settlement;
	REQUIRE(materialization->settle_struct(settle_input, settlement, error));
	REQUIRE(settlement.success);
	CHECK(String(settlement.content) == "approved result");
	CHECK(server->get_calls().size() == 1);

	Dictionary recorded_call = server->get_calls()[0];
	CHECK(String(recorded_call.get("name", String())) == "ask.echo");
	CHECK(String(Dictionary(recorded_call.get("input", Dictionary())).get("value", String())) == "hello");

	CHECK(String(settlement.metadata.get("tool_origin", String())) == "mcp");
	CHECK(String(settlement.metadata.get("mcp_server_id", String())) == "filesystem");
	CHECK(String(settlement.metadata.get("mcp_tool_name", String())) == "ask.echo");
	CHECK(String(settlement.metadata.get("mcp_agent_tool_name", String())) == tool_name);
}

TEST_CASE("[Editor][AgentV1] MCP server disconnect returns readable failure and builtins still settle") {
	const String root = TestUtils::get_temp_path("agent_v1/mcp_disconnect_root");
	write_text_file(root.path_join("readme.txt"), "builtin still works");

	Ref<AIFakeMCPServer> server;
	server.instantiate();

	Dictionary schema;
	schema["type"] = "object";
	Dictionary descriptor;
	descriptor["description"] = "Disconnect test.";
	descriptor["inputSchema"] = schema;
	Dictionary result;
	result["content"] = "should not appear";
	server->register_tool_struct("unstable.echo", descriptor, result);
	server->start();

	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();
	registry->set_root_dir(root);
	registry->register_builtin_tools();

	Ref<AIV1MCPService> service;
	service.instantiate();
	service->set_tool_registry(registry);
	REQUIRE(service->register_fake_server_for_test(make_mcp_server_config("unstable", "Unstable", "allow"), server));

	AIError error;
	REQUIRE(service->refresh_struct(error));
	const String tool_name = AIV1MCPService::make_tool_name("unstable", "unstable.echo");

	Ref<AIV1ToolMaterialization> materialization = registry->materialize_struct();
	REQUIRE(materialization->has_tool(tool_name));
	REQUIRE(materialization->has_tool("file_read"));

	server->stop();

	Dictionary mcp_call;
	mcp_call["id"] = "call-mcp-disconnect";
	mcp_call["name"] = tool_name;
	mcp_call["input"] = Dictionary();
	Dictionary mcp_settle_input;
	mcp_settle_input["session_id"] = "session-mcp-disconnect";
	mcp_settle_input["assistant_message_id"] = "assistant-mcp-disconnect";
	mcp_settle_input["call"] = mcp_call;

	AIV1ToolSettlement failed_settlement;
	REQUIRE(materialization->settle_struct(mcp_settle_input, failed_settlement, error));
	CHECK_FALSE(failed_settlement.success);
	CHECK(failed_settlement.error.kind == AI_ERROR_UNAVAILABLE);
	CHECK(failed_settlement.error.message.contains("not running"));

	Dictionary read_args;
	read_args["path"] = "readme.txt";
	Dictionary read_call;
	read_call["id"] = "call-read-after-mcp";
	read_call["name"] = "file_read";
	read_call["input"] = read_args;
	Dictionary read_settle_input;
	read_settle_input["session_id"] = "session-mcp-disconnect";
	read_settle_input["assistant_message_id"] = "assistant-mcp-disconnect";
	read_settle_input["call"] = read_call;

	AIV1ToolSettlement read_settlement;
	REQUIRE(materialization->settle_struct(read_settle_input, read_settlement, error));
	CHECK(read_settlement.success);
	CHECK(String(Dictionary(read_settlement.structured).get("text", String())) == "builtin still works");
}

TEST_CASE("[Editor][AgentV1] MCP resources enter context only after explicit selection") {
	Ref<AIFakeMCPServer> server;
	server.instantiate();

	Dictionary descriptor;
	descriptor["name"] = "Phase 8 Doc";
	descriptor["description"] = "A selectable MCP resource.";
	descriptor["mimeType"] = "text/plain";
	server->register_resource_struct("memory://phase8/doc", descriptor, "selected mcp resource text");
	server->start();

	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();

	Ref<AIV1MCPService> service;
	service.instantiate();
	service->set_tool_registry(registry);
	REQUIRE(service->register_fake_server_for_test(make_mcp_server_config("knowledge", "Knowledge", "ask"), server));

	AIError error;
	REQUIRE(service->refresh_struct(error));

	Array resources = service->list_resources("knowledge");
	REQUIRE(resources.size() == 1);
	CHECK(String(Dictionary(resources[0]).get("uri", String())) == "memory://phase8/doc");

	Dictionary read = service->read_resource("knowledge", "memory://phase8/doc");
	REQUIRE(bool(read.get("success", false)));
	CHECK(String(read.get("text", String())) == "selected mcp resource text");

	Dictionary source_dict = service->make_resource_context_source("knowledge", "memory://phase8/doc", false, 300);
	REQUIRE(bool(source_dict.get("success", false)));
	Dictionary source = source_dict.get("source", Dictionary());
	CHECK(String(source.get("domain", String())) == "mcp.resource/knowledge");
	CHECK(String(source.get("text", String())) == "selected mcp resource text");
	CHECK(String(Dictionary(source.get("metadata", Dictionary())).get("mcp_uri", String())) == "memory://phase8/doc");

	Ref<AIContextSourceRegistry> context_registry;
	context_registry.instantiate();
	context_registry->add_source(source);

	AILocationRef location;
	location.directory = TestUtils::get_temp_path("agent_v1/mcp_context");
	AISystemContext context;
	REQUIRE(context_registry->load_struct("main", location, "fake", "fake-model", context, error));
	CHECK(context.baseline.contains("selected mcp resource text"));
}

TEST_CASE("[Editor][AgentV1] MCP prompts list and render through the service adapter") {
	Ref<AIFakeMCPServer> server;
	server.instantiate();

	Dictionary prompt_descriptor;
	prompt_descriptor["description"] = "Summarize a selectable resource.";
	Array arguments;
	Dictionary uri_argument;
	uri_argument["name"] = "uri";
	uri_argument["required"] = true;
	arguments.push_back(uri_argument);
	prompt_descriptor["arguments"] = arguments;

	Array messages;
	Dictionary message;
	message["role"] = "user";
	message["content"] = "Summarize memory://phase8/doc";
	messages.push_back(message);
	server->register_prompt_struct("summarize_resource", prompt_descriptor, messages);
	server->start();

	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();

	Ref<AIV1MCPService> service;
	service.instantiate();
	service->set_tool_registry(registry);
	REQUIRE(service->register_fake_server_for_test(make_mcp_server_config("knowledge_prompts", "Knowledge Prompts", "allow"), server));

	AIError error;
	REQUIRE(service->refresh_struct(error));

	Array prompts = service->list_prompts("knowledge_prompts");
	REQUIRE(prompts.size() == 1);
	Dictionary listed = prompts[0];
	CHECK(String(listed.get("server_id", String())) == "knowledge_prompts");
	CHECK(String(listed.get("name", String())) == "summarize_resource");
	CHECK(Array(listed.get("arguments", Array())).size() == 1);

	Dictionary rendered = service->render_prompt("knowledge_prompts", "summarize_resource", Dictionary());
	REQUIRE(bool(rendered.get("success", false)));
	CHECK(String(rendered.get("server_id", String())) == "knowledge_prompts");
	Array rendered_messages = rendered.get("messages", Array());
	REQUIRE(rendered_messages.size() == 1);
	CHECK(String(Dictionary(rendered_messages[0]).get("content", String())) == "Summarize memory://phase8/doc");
}

TEST_CASE("[Editor][AgentV1] MCP configured server startup goes through permission before transport") {
	Ref<AIEventStore> event_store;
	event_store.instantiate();
	event_store->set_base_dir(TestUtils::get_temp_path("agent_v1/mcp_start_permission_events"));

	Ref<AIPermissionService> permission_service;
	permission_service.instantiate();
	permission_service->set_event_store(event_store);

	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();
	registry->set_permission_service(permission_service);

	Dictionary server_config;
	server_config["id"] = "configured";
	server_config["name"] = "Configured";
	server_config["enabled"] = true;
	server_config["transport"] = "stdio";
	server_config["command"] = "definitely_missing_mcp_command";
	server_config["startup_permission_default"] = "ask";

	Ref<AIV1MCPService> service;
	service.instantiate();
	service->set_tool_registry(registry);

	AIError error;
	REQUIRE(service->register_server_struct(server_config, error));
	CHECK_FALSE(service->refresh_struct(error));
	CHECK(error.kind == AI_ERROR_PERMISSION);
	CHECK(error.message.contains("pending"));

	Array pending = permission_service->get_pending_requests();
	REQUIRE(pending.size() == 1);
	Dictionary pending_request = pending[0];
	CHECK(String(pending_request.get("action", String())) == "mcp.server.start");
	CHECK(String(pending_request.get("resource", String())).contains("configured/stdio"));
	CHECK(String(pending_request.get("resource", String())).contains("definitely_missing_mcp_command"));

	Array statuses = service->get_statuses();
	REQUIRE(statuses.size() == 1);
	CHECK(String(Dictionary(statuses[0]).get("state", String())) == "permission_pending");
	CHECK(String(Dictionary(statuses[0]).get("last_error", String())).contains("pending"));
}

TEST_CASE("[Editor][AgentV1] MCP configured resource and prompt reads go through startup permission before transport") {
	Ref<AIEventStore> event_store;
	event_store.instantiate();
	event_store->set_base_dir(TestUtils::get_temp_path("agent_v1/mcp_read_permission_events"));

	Ref<AIPermissionService> permission_service;
	permission_service.instantiate();
	permission_service->set_event_store(event_store);

	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();
	registry->set_permission_service(permission_service);

	Dictionary server_config;
	server_config["id"] = "configured_read";
	server_config["name"] = "Configured Read";
	server_config["enabled"] = true;
	server_config["transport"] = "stdio";
	server_config["command"] = "definitely_missing_mcp_command";
	server_config["startup_permission_default"] = "ask";

	Ref<AIV1MCPService> service;
	service.instantiate();
	service->set_tool_registry(registry);

	AIError error;
	REQUIRE(service->register_server_struct(server_config, error));

	Dictionary resource_read = service->read_resource("configured_read", "file:///workspace/README.md");
	CHECK_FALSE(bool(resource_read.get("success", false)));
	CHECK(String(Dictionary(resource_read.get("error", Dictionary())).get("kind", String())) == "permission");
	CHECK(String(Dictionary(resource_read.get("error", Dictionary())).get("message", String())).contains("pending"));

	Array pending = permission_service->get_pending_requests();
	REQUIRE(pending.size() == 1);
	if (pending.size() == 1) {
		Dictionary pending_request = pending[0];
		CHECK(String(pending_request.get("action", String())) == "mcp.server.start");
		CHECK(String(pending_request.get("resource", String())).contains("configured_read/stdio"));
		CHECK(String(pending_request.get("resource", String())).contains("definitely_missing_mcp_command"));
	}

	Dictionary rendered = service->render_prompt("configured_read", "summarize", Dictionary());
	CHECK_FALSE(bool(rendered.get("success", false)));
	CHECK(String(Dictionary(rendered.get("error", Dictionary())).get("kind", String())) == "permission");
	CHECK(String(Dictionary(rendered.get("error", Dictionary())).get("message", String())).contains("pending"));
	CHECK(permission_service->get_pending_requests().size() == 1);
}

TEST_CASE("[Editor][AgentV1] MCP service does not retain itself through registered tool adapters") {
	Ref<AIFakeMCPServer> server;
	server.instantiate();
	Dictionary descriptor;
	descriptor["description"] = "Lifecycle test.";
	server->register_tool_struct("lifecycle.echo", descriptor, "ok");
	server->start();

	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();

	Ref<AIV1MCPService> service;
	service.instantiate();
	service->set_tool_registry(registry);
	REQUIRE(service->register_fake_server_for_test(make_mcp_server_config("lifecycle", "Lifecycle", "allow"), server));

	AIError error;
	REQUIRE(service->refresh_struct(error));
	CHECK(registry->has_tool(AIV1MCPService::make_tool_name("lifecycle", "lifecycle.echo")));
	CHECK(service->get_reference_count() == 1);
}

TEST_CASE("[Editor][AgentV1] MCP config import is atomic on invalid server definitions") {
	Ref<AIFakeMCPServer> server;
	server.instantiate();
	Dictionary descriptor;
	descriptor["description"] = "Atomic import test.";
	server->register_tool_struct("stable.echo", descriptor, "ok");
	server->start();

	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();

	Ref<AIV1MCPService> service;
	service.instantiate();
	service->set_tool_registry(registry);
	REQUIRE(service->register_fake_server_for_test(make_mcp_server_config("stable", "Stable", "allow"), server));

	AIError error;
	REQUIRE(service->refresh_struct(error));
	CHECK(registry->has_tool(AIV1MCPService::make_tool_name("stable", "stable.echo")));

	Dictionary invalid_server;
	invalid_server["id"] = "9-invalid";
	invalid_server["name"] = "Invalid";
	invalid_server["enabled"] = true;
	invalid_server["transport"] = "stdio";
	invalid_server["command"] = "missing";
	Array servers;
	servers.push_back(invalid_server);
	Dictionary mcp;
	mcp["servers"] = servers;
	Dictionary app_config;
	app_config["mcp"] = mcp;

	CHECK_FALSE(service->import_config_struct(app_config, error));
	CHECK(error.kind == AI_ERROR_VALIDATION);

	Array statuses = service->get_statuses();
	REQUIRE(statuses.size() == 1);
	CHECK(String(Dictionary(statuses[0]).get("server_id", String())) == "stable");
	CHECK(String(Dictionary(statuses[0]).get("state", String())) == "ready");
	CHECK(registry->has_tool(AIV1MCPService::make_tool_name("stable", "stable.echo")));
}

TEST_CASE("[Editor][AgentV1] MCP failed discovery rolls back partially registered tools") {
	Ref<AIFakeMCPServer> server;
	server.instantiate();
	Dictionary descriptor;
	descriptor["description"] = "First tool.";
	server->register_tool_struct("first.echo", descriptor, "ok");

	String long_tool_name;
	for (int i = 0; i < 160; i++) {
		long_tool_name += "x";
	}
	Dictionary long_descriptor;
	long_descriptor["description"] = "Too long for provider tool name.";
	server->register_tool_struct(long_tool_name, long_descriptor, "too long");
	server->start();

	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();

	Ref<AIV1MCPService> service;
	service.instantiate();
	service->set_tool_registry(registry);
	REQUIRE(service->register_fake_server_for_test(make_mcp_server_config("rollback", "Rollback", "allow"), server));

	AIError error;
	CHECK_FALSE(service->refresh_struct(error));
	CHECK(error.kind == AI_ERROR_VALIDATION);
	CHECK_FALSE(registry->has_tool(AIV1MCPService::make_tool_name("rollback", "first.echo")));
}

TEST_CASE("[Editor][AgentV1] Skill explicit selection enters context without loading unselected skills") {
	const String root = TestUtils::get_temp_path("agent_v1/skills_explicit");
	const String tdd_dir = root.path_join("tdd");
	const String unused_dir = root.path_join("unused");

	write_text_file(tdd_dir.path_join("skill.json"), "{\n\"id\":\"tdd\",\"name\":\"TDD\",\"description\":\"Use when changing behavior.\",\"entry\":\"SKILL.md\",\"triggers\":[{\"type\":\"keyword\",\"value\":\"test\"}]\n}\n");
	write_text_file(tdd_dir.path_join("SKILL.md"), "---\nname: TDD\ndescription: Use when changing behavior.\n---\n\nWrite failing tests before implementation.");
	write_text_file(unused_dir.path_join("skill.json"), "{\n\"id\":\"unused\",\"name\":\"Unused\",\"description\":\"Should not load.\",\"entry\":\"SKILL.md\"\n}\n");
	write_text_file(unused_dir.path_join("SKILL.md"), "UNSELECTED SECRET GUIDANCE");

	Ref<AIV1SkillService> service;
	service.instantiate();
	Dictionary skills_config;
	Array sources;
	sources.push_back(root);
	skills_config["sources"] = sources;
	skills_config["auto_select"] = false;
	Dictionary app_config;
	app_config["skills"] = skills_config;

	AIError error;
	REQUIRE(service->import_config_struct(app_config, error));
	REQUIRE(service->refresh_struct(error));

	Array manifests = service->list_manifests();
	REQUIRE(manifests.size() == 2);
	for (int i = 0; i < manifests.size(); i++) {
		CHECK_FALSE(String(Dictionary(manifests[i]).get("guidance", String())).contains("Write failing tests"));
		CHECK_FALSE(String(Dictionary(manifests[i]).get("content", String())).contains("UNSELECTED SECRET GUIDANCE"));
	}

	Array explicit_names;
	explicit_names.push_back("TDD");
	Array selected = service->select("please refactor this", explicit_names);
	REQUIRE(selected.size() == 1);
	CHECK(String(Dictionary(selected[0]).get("skill_id", String())) == "tdd");
	CHECK(bool(Dictionary(selected[0]).get("explicit", false)));

	Dictionary source_result = service->make_context_source("tdd", true, 150);
	REQUIRE(bool(source_result.get("success", false)));
	Dictionary source = source_result.get("source", Dictionary());
	CHECK(String(source.get("domain", String())) == "skill/tdd");
	CHECK(String(source.get("text", String())).contains("## Skill: TDD"));
	CHECK(String(source.get("text", String())).contains("Write failing tests before implementation."));

	Ref<AIContextSourceRegistry> context_registry;
	context_registry.instantiate();
	context_registry->add_source(source);

	AILocationRef location;
	location.directory = root;
	AISystemContext context;
	REQUIRE(context_registry->load_struct("main", location, "fake", "fake-model", context, error));
	CHECK(context.baseline.contains("Write failing tests before implementation."));
	CHECK_FALSE(context.baseline.contains("UNSELECTED SECRET GUIDANCE"));
}

TEST_CASE("[Editor][AgentV1] Skill selector is conservative and observes disabled and max count config") {
	const String root = TestUtils::get_temp_path("agent_v1/skills_selector");
	write_text_file(root.path_join("pytest").path_join("skill.json"), "{\n\"id\":\"pytest\",\"name\":\"Pytest\",\"description\":\"Use for pytest.\",\"entry\":\"SKILL.md\",\"triggers\":[{\"type\":\"keyword\",\"value\":\"pytest\"}]\n}\n");
	write_text_file(root.path_join("pytest").path_join("SKILL.md"), "Use pytest fixtures.");
	write_text_file(root.path_join("docs").path_join("skill.json"), "{\n\"id\":\"docs\",\"name\":\"Docs\",\"description\":\"Use for docs.\",\"entry\":\"SKILL.md\",\"triggers\":[{\"type\":\"keyword\",\"value\":\"documentation\"}]\n}\n");
	write_text_file(root.path_join("docs").path_join("SKILL.md"), "Write documentation.");

	Ref<AIV1SkillService> service;
	service.instantiate();
	Dictionary skills_config;
	Array sources;
	sources.push_back(root);
	skills_config["sources"] = sources;
	skills_config["auto_select"] = true;
	skills_config["max_skills_per_turn"] = 1;
	Array disabled;
	disabled.push_back("docs");
	skills_config["disabled_skill_ids"] = disabled;
	Dictionary app_config;
	app_config["skills"] = skills_config;

	AIError error;
	REQUIRE(service->import_config_struct(app_config, error));
	REQUIRE(service->refresh_struct(error));

	Array no_match = service->select("ordinary request", Array());
	CHECK(no_match.is_empty());

	Array selected = service->select("please fix the pytest suite and documentation", Array());
	REQUIRE(selected.size() == 1);
	CHECK(String(Dictionary(selected[0]).get("skill_id", String())) == "pytest");
	CHECK_FALSE(bool(Dictionary(selected[0]).get("explicit", true)));
}

TEST_CASE("[Editor][AgentV1] Session runner injects selected Skill into Context Epoch") {
	const String base = TestUtils::get_temp_path("agent_v1/skills_runner_epoch");
	const String workspace = base.path_join("workspace");
	const String skills_root = base.path_join("skills");
	write_text_file(skills_root.path_join("pytest").path_join("skill.json"), "{\n\"id\":\"pytest\",\"name\":\"Pytest\",\"description\":\"Python test workflow.\",\"entry\":\"SKILL.md\",\"triggers\":[{\"type\":\"keyword\",\"value\":\"pytest\"}]\n}\n");
	write_text_file(skills_root.path_join("pytest").path_join("SKILL.md"), "---\nname: Pytest\ndescription: Python test workflow.\n---\n\nUse pytest fixtures from runner integration.");
	write_text_file(skills_root.path_join("unused").path_join("skill.json"), "{\n\"id\":\"unused\",\"name\":\"Unused\",\"description\":\"Should not be selected.\",\"entry\":\"SKILL.md\",\"triggers\":[{\"type\":\"keyword\",\"value\":\"never-match-runner\"}]\n}\n");
	write_text_file(skills_root.path_join("unused").path_join("SKILL.md"), "UNSELECTED RUNNER SECRET");

	Ref<AIFakeLLMRuntime> runtime;
	runtime.instantiate();
	runtime->set_response_text("skill context accepted");

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());
	service->get_runtime_registry()->register_runtime("fake", runtime);

	Dictionary fake_provider;
	fake_provider["type"] = "fake";
	fake_provider["model"] = "fake-model";
	Dictionary providers;
	providers["fake"] = fake_provider;

	Array skill_sources;
	skill_sources.push_back(skills_root);
	Dictionary skills_config;
	skills_config["sources"] = skill_sources;
	skills_config["auto_select"] = true;

	Dictionary override_config;
	override_config["default_provider"] = "fake";
	override_config["default_model"] = "fake-model";
	override_config["providers"] = providers;
	override_config["skills"] = skills_config;
	service->get_config_service()->set_runtime_override(override_config);

	Dictionary create;
	create["id"] = "phase9-skill-runner";
	create["directory"] = workspace;
	REQUIRE(bool(service->create(create).get("success", false)));

	Dictionary prompt;
	prompt["session_id"] = "phase9-skill-runner";
	prompt["message_id"] = "phase9-skill-runner-message";
	prompt["text"] = "please run the pytest suite";
	prompt["resume"] = false;
	REQUIRE(bool(service->prompt(prompt).get("success", false)));

	Dictionary run = service->get_session_runner()->run("phase9-skill-runner", false);
	REQUIRE(bool(run.get("success", false)));
	CHECK(runtime->get_stream_call_count() == 1);

	const Dictionary request = runtime->get_last_request();
	const String request_json = Variant(request).stringify();
	CHECK(request_json.contains("Use pytest fixtures from runner integration."));
	CHECK_FALSE(request_json.contains("UNSELECTED RUNNER SECRET"));

	String system_text;
	const Array system_parts = request.get("system", Array());
	for (int i = 0; i < system_parts.size(); i++) {
		if (system_parts[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary part = system_parts[i];
		if (String(part.get("type", String())) == "text") {
			system_text += String(part.get("text", String()));
		}
	}
	CHECK_FALSE(system_text.contains(skills_root));

	const Dictionary metadata = request.get("metadata", Dictionary());
	const Array selected_skills = metadata.get("selected_skills", Array());
	CHECK(selected_skills.size() == 1);
	if (selected_skills.size() == 1) {
		CHECK(String(Dictionary(selected_skills[0]).get("skill_id", String())) == "pytest");
	}

	const Dictionary context_epoch = metadata.get("context_epoch", Dictionary());
	CHECK(String(context_epoch.get("baseline", String())).contains("Use pytest fixtures from runner integration."));
}

TEST_CASE("[Editor][AgentV1] Skill resources are read on demand with source metadata") {
	const String root = TestUtils::get_temp_path("agent_v1/skills_resources");
	const String skill_dir = root.path_join("resourceful");
	write_text_file(skill_dir.path_join("skill.json"), "{\n\"id\":\"resourceful\",\"name\":\"Resourceful\",\"description\":\"Has resources.\",\"entry\":\"SKILL.md\",\"resources\":[{\"path\":\"references/guide.md\",\"kind\":\"reference\"},{\"path\":\"templates/component.txt\",\"kind\":\"template\"}]\n}\n");
	write_text_file(skill_dir.path_join("SKILL.md"), "Load resources only when needed.");
	write_text_file(skill_dir.path_join("references").path_join("guide.md"), "Reference material loaded on demand.");
	write_text_file(skill_dir.path_join("templates").path_join("component.txt"), "Template body.");

	Ref<AIV1SkillService> service;
	service.instantiate();
	Dictionary skills_config;
	Array sources;
	sources.push_back(root);
	skills_config["sources"] = sources;
	Dictionary app_config;
	app_config["skills"] = skills_config;

	AIError error;
	REQUIRE(service->import_config_struct(app_config, error));
	REQUIRE(service->refresh_struct(error));

	Dictionary read = service->read_resource("resourceful", "references/guide.md", "reference");
	REQUIRE(bool(read.get("success", false)));
	CHECK(String(read.get("text", String())) == "Reference material loaded on demand.");
	CHECK(String(read.get("skill_id", String())) == "resourceful");
	CHECK(String(read.get("path", String())) == "references/guide.md");
	CHECK(String(read.get("kind", String())) == "reference");
	CHECK_FALSE(String(read.get("content_hash", String())).is_empty());

	Dictionary escaped = service->read_resource("resourceful", "../outside.md", "reference");
	CHECK_FALSE(bool(escaped.get("success", false)));
	CHECK(String(Dictionary(escaped.get("error", Dictionary())).get("kind", String())) == "permission");
}

TEST_CASE("[Editor][AgentV1] Skill script tools register through ToolRegistry and require permission") {
	const String root = TestUtils::get_temp_path("agent_v1/skills_script_tool");
	const String skill_dir = root.path_join("script_skill");
	write_text_file(skill_dir.path_join("skill.json"), "{\n\"id\":\"script_skill\",\"name\":\"Script Skill\",\"description\":\"Has a script tool.\",\"entry\":\"SKILL.md\",\"tools\":[{\"name\":\"echo\",\"description\":\"Echo from skill.\",\"command\":[\"cmd\",\"/C\",\"echo skill tool\"],\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}]\n}\n");
	write_text_file(skill_dir.path_join("SKILL.md"), "Script tool guidance.");

	Ref<AIEventStore> event_store;
	event_store.instantiate();
	event_store->set_base_dir(TestUtils::get_temp_path("agent_v1/skill_script_permission_events"));

	Ref<AIPermissionService> permission_service;
	permission_service.instantiate();
	permission_service->set_event_store(event_store);

	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();
	registry->set_permission_service(permission_service);

	Ref<AIV1SkillService> service;
	service.instantiate();
	service->set_tool_registry(registry);

	Dictionary skills_config;
	Array sources;
	sources.push_back(root);
	skills_config["sources"] = sources;
	skills_config["script_tools_enabled"] = true;
	skills_config["script_permission_default"] = "ask";
	Dictionary app_config;
	app_config["skills"] = skills_config;

	AIError error;
	REQUIRE(service->import_config_struct(app_config, error));
	REQUIRE(service->refresh_struct(error));

	const String tool_name = AIV1SkillService::make_tool_name("script_skill", "echo");
	CHECK(registry->has_tool(tool_name));

	Ref<AIV1ToolMaterialization> materialization = registry->materialize_struct();
	REQUIRE(materialization->has_tool(tool_name));

	Dictionary call;
	call["id"] = "call-skill-script";
	call["name"] = tool_name;
	call["input"] = Dictionary();
	Dictionary settle_input;
	settle_input["session_id"] = "session-skill-script";
	settle_input["assistant_message_id"] = "assistant-skill-script";
	settle_input["call"] = call;

	AIV1ToolSettlement pending_settlement;
	REQUIRE(materialization->settle_struct(settle_input, pending_settlement, error));
	CHECK_FALSE(pending_settlement.success);
	CHECK(pending_settlement.pending);

	Array pending = permission_service->get_pending_requests();
	REQUIRE(pending.size() == 1);
	Dictionary pending_request = pending[0];
	CHECK(String(pending_request.get("action", String())) == "skill.script.run");
	CHECK(String(pending_request.get("resource", String())).contains("script_skill/echo"));

	Dictionary reply;
	reply["request_id"] = pending_request.get("request_id", String());
	reply["reply"] = "once";
	(void)permission_service->reply(reply);

	AIV1ToolSettlement settlement;
	REQUIRE(materialization->settle_struct(settle_input, settlement, error));
	REQUIRE(settlement.success);
	CHECK(String(settlement.content).contains("skill tool"));
	CHECK(String(settlement.metadata.get("tool_origin", String())) == "skill");
	CHECK(String(settlement.metadata.get("skill_id", String())) == "script_skill");
}

TEST_CASE("[Editor][AgentV1] Agent service resolves configured and session-bound agents") {
	Ref<AIConfigService> config;
	config.instantiate();

	Dictionary main_agent;
	main_agent["name"] = "Main";
	main_agent["provider"] = "fake-parent";
	main_agent["model"] = "parent-model";
	Array main_system;
	main_system.push_back("Main system prompt.");
	main_agent["system"] = main_system;

	Dictionary reviewer_agent;
	reviewer_agent["name"] = "Reviewer";
	reviewer_agent["description"] = "Reviews delegated work.";
	reviewer_agent["provider"] = "fake-child";
	reviewer_agent["model"] = "child-model";
	reviewer_agent["system_prompt"] = "Reviewer system prompt.";

	Dictionary agents;
	agents["main"] = main_agent;
	agents["reviewer"] = reviewer_agent;

	Dictionary override_config;
	override_config["default_agent"] = "main";
	override_config["agents"] = agents;
	config->set_runtime_override(override_config);

	Ref<AISessionStore> sessions;
	sessions.instantiate();
	sessions->set_base_dir(String());
	Dictionary session_input;
	session_input["id"] = "phase10-agent-session";
	session_input["agent_id"] = "reviewer";
	bool created = false;
	String store_error;
	AISessionRow session;
	REQUIRE(sessions->create_or_reuse(session_input, session, created, store_error));

	Ref<AIAgentService> service;
	service.instantiate();
	service->set_config_service(config);
	service->set_session_store(sessions);

	Array listed = service->list();
	CHECK(listed.size() == 2);

	Dictionary resolved = service->resolve("reviewer");
	REQUIRE(bool(resolved.get("success", false)));
	CHECK(String(resolved.get("id", String())) == "reviewer");
	CHECK(String(resolved.get("provider", String())) == "fake-child");
	CHECK(String(resolved.get("model", String())) == "child-model");
	CHECK(String(Array(resolved.get("system", Array()))[0]) == "Reviewer system prompt.");

	Dictionary session_resolved = service->resolve_for_session("phase10-agent-session");
	REQUIRE(bool(session_resolved.get("success", false)));
	CHECK(String(session_resolved.get("id", String())) == "reviewer");
}

TEST_CASE("[Editor][AgentV1] Task tool is materialized and asks agent.spawn before creating a child session") {
	const String workspace = TestUtils::get_temp_path("agent_v1/phase10_task_permission");

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());

	Dictionary main_agent;
	main_agent["provider"] = "fake";
	main_agent["model"] = "fake-model";
	Dictionary subagents;
	Array allowed;
	allowed.push_back("reviewer");
	subagents["allowed_agent_ids"] = allowed;
	subagents["max_depth"] = 2;
	main_agent["subagents"] = subagents;

	Dictionary reviewer_agent;
	reviewer_agent["provider"] = "fake";
	reviewer_agent["model"] = "fake-model";

	Dictionary agents;
	agents["main"] = main_agent;
	agents["reviewer"] = reviewer_agent;
	Dictionary override_config;
	override_config["agents"] = agents;
	service->get_config_service()->set_runtime_override(override_config);

	Dictionary create;
	create["id"] = "phase10-task-parent";
	create["agent_id"] = "main";
	create["directory"] = workspace;
	REQUIRE(bool(service->create(create).get("success", false)));

	Ref<AIV1ToolMaterialization> materialization = service->get_tool_registry()->materialize_struct(workspace, Array());
	REQUIRE(materialization->has_tool("task"));

	Dictionary task_input;
	task_input["agent_id"] = "reviewer";
	task_input["description"] = "Review this change.";
	task_input["prompt"] = "Find correctness risks.";

	Dictionary call;
	call["id"] = "phase10-task-call";
	call["name"] = "task";
	call["input"] = task_input;

	Dictionary settle_input;
	settle_input["session_id"] = "phase10-task-parent";
	settle_input["agent"] = "main";
	settle_input["assistant_message_id"] = "phase10-parent-assistant";
	settle_input["call"] = call;

	AIError error;
	AIV1ToolSettlement settlement;
	REQUIRE(materialization->settle_struct(settle_input, settlement, error));
	CHECK_FALSE(settlement.success);
	CHECK(settlement.pending);

	Array pending = service->get_permission_service()->get_pending_requests();
	REQUIRE(pending.size() == 1);
	Dictionary request = pending[0];
	CHECK(String(request.get("action", String())) == "agent.spawn");
	CHECK(String(request.get("resource", String())) == "reviewer");
	CHECK(service->get_session_store()->list_sessions().size() == 1);
}

TEST_CASE("[Editor][AgentV1] Task tool creates child session through the normal runner and returns result to parent") {
	const String base = TestUtils::get_temp_path("agent_v1/phase10_subagent_runner");
	const String workspace = base.path_join("workspace");

	Ref<Phase10ParentRuntime> parent_runtime;
	parent_runtime.instantiate();
	parent_runtime->task_input["agent_id"] = "reviewer";
	parent_runtime->task_input["description"] = "Review delegated code.";
	parent_runtime->task_input["prompt"] = "Return the important finding.";
	parent_runtime->task_input["expected_output"] = "A concise review result.";

	Ref<AIFakeLLMRuntime> child_runtime;
	child_runtime.instantiate();
	child_runtime->set_response_text("child completed result");

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());
	service->get_runtime_registry()->register_runtime("fake-parent", parent_runtime);
	service->get_runtime_registry()->register_runtime("fake-child", child_runtime);

	Dictionary parent_provider;
	parent_provider["type"] = "fake";
	parent_provider["model"] = "parent-model";
	Dictionary child_provider;
	child_provider["type"] = "fake";
	child_provider["model"] = "child-model";
	Dictionary providers;
	providers["fake-parent"] = parent_provider;
	providers["fake-child"] = child_provider;

	Dictionary main_agent;
	main_agent["provider"] = "fake-parent";
	main_agent["model"] = "parent-model";
	Dictionary subagents;
	Array allowed;
	allowed.push_back("reviewer");
	subagents["allowed_agent_ids"] = allowed;
	subagents["max_depth"] = 2;
	subagents["max_concurrent"] = 1;
	main_agent["subagents"] = subagents;

	Dictionary reviewer_agent;
	reviewer_agent["name"] = "Reviewer";
	reviewer_agent["provider"] = "fake-child";
	reviewer_agent["model"] = "child-model";
	reviewer_agent["system_prompt"] = "You are the reviewer subagent.";

	Dictionary agents;
	agents["main"] = main_agent;
	agents["reviewer"] = reviewer_agent;

	Dictionary allow_spawn;
	allow_spawn["action"] = "agent.spawn";
	allow_spawn["resource"] = "reviewer";
	allow_spawn["effect"] = "allow";
	Array rules;
	rules.push_back(allow_spawn);
	Dictionary permissions;
	permissions["rules"] = rules;

	Dictionary override_config;
	override_config["default_provider"] = "fake-parent";
	override_config["default_model"] = "parent-model";
	override_config["providers"] = providers;
	override_config["agents"] = agents;
	override_config["permissions"] = permissions;
	service->get_config_service()->set_runtime_override(override_config);

	Dictionary create;
	create["id"] = "phase10-parent";
	create["agent_id"] = "main";
	create["directory"] = workspace;
	REQUIRE(bool(service->create(create).get("success", false)));

	Dictionary prompt;
	prompt["session_id"] = "phase10-parent";
	prompt["message_id"] = "phase10-parent-prompt";
	prompt["text"] = "Please delegate this review.";
	prompt["resume"] = false;
	REQUIRE(bool(service->prompt(prompt).get("success", false)));

	Dictionary run = service->get_session_runner()->run("phase10-parent", false);
	REQUIRE(bool(run.get("success", false)));
	CHECK(parent_runtime->stream_call_count == 2);
	CHECK(child_runtime->get_stream_call_count() == 1);

	Array sessions = service->get_session_store()->list_sessions();
	String child_session_id;
	for (int i = 0; i < sessions.size(); i++) {
		const Dictionary item = sessions[i];
		const Dictionary metadata = item.get("metadata", Dictionary());
		if (String(metadata.get("parent_session_id", String())) == "phase10-parent") {
			child_session_id = item.get("id", String());
			CHECK(String(item.get("agent_id", String())) == "reviewer");
			CHECK(String(metadata.get("parent_tool_call_id", String())) == "phase10-task-call");
		}
	}
	REQUIRE(!child_session_id.is_empty());

	const Dictionary child_request = child_runtime->get_last_request();
	CHECK(String(child_request.get("provider", String())) == "fake-child");
	CHECK(String(child_request.get("model", String())) == "child-model");
	const String child_request_json = Variant(child_request).stringify();
	CHECK(child_request_json.contains("[Parent Task]"));
	CHECK(child_request_json.contains("Review delegated code."));
	CHECK(child_request_json.contains("Return the important finding."));

	const Vector<AIEventRow> parent_events = service->get_event_store()->list("phase10-parent");
	bool saw_task_success = false;
	for (int i = 0; i < parent_events.size(); i++) {
		if (parent_events[i].type != AIDomainEventTypes::tool_success()) {
			continue;
		}
		const Dictionary data = parent_events[i].data;
		if (String(data.get("tool", String())) != "task") {
			continue;
		}
		saw_task_success = true;
		const Dictionary structured = data.get("structured", Dictionary());
		CHECK(String(structured.get("child_session_id", String())) == child_session_id);
		CHECK(String(structured.get("status", String())) == "completed");
		CHECK(String(structured.get("result", String())).contains("child completed result"));
	}
	CHECK(saw_task_success);

	const Dictionary parent_second_request = parent_runtime->last_request;
	CHECK(Variant(parent_second_request).stringify().contains("child completed result"));
}

TEST_CASE("[Editor][AgentV1] Session runner uses non-main default agent consistently") {
	const String workspace = TestUtils::get_temp_path("agent_v1/phase10_default_agent");

	Ref<AIFakeLLMRuntime> runtime;
	runtime.instantiate();
	runtime->set_response_text("coder default agent response");

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());
	service->get_runtime_registry()->register_runtime("fake-coder", runtime);

	Dictionary provider;
	provider["type"] = "fake";
	provider["model"] = "coder-model";
	Dictionary providers;
	providers["fake-coder"] = provider;

	Dictionary coder_agent;
	coder_agent["provider"] = "fake-coder";
	coder_agent["model"] = "coder-model";
	coder_agent["system_prompt"] = "You are the configured default coder.";
	Dictionary agents;
	agents["coder"] = coder_agent;

	Dictionary override_config;
	override_config["default_agent"] = "coder";
	override_config["default_provider"] = "fake-coder";
	override_config["default_model"] = "coder-model";
	override_config["providers"] = providers;
	override_config["agents"] = agents;
	service->get_config_service()->set_runtime_override(override_config);

	Dictionary create;
	create["id"] = "phase10-default-agent-session";
	create["directory"] = workspace;
	REQUIRE(bool(service->create(create).get("success", false)));

	Dictionary prompt;
	prompt["session_id"] = "phase10-default-agent-session";
	prompt["message_id"] = "phase10-default-agent-prompt";
	prompt["text"] = "Use the configured default agent.";
	prompt["resume"] = false;
	REQUIRE(bool(service->prompt(prompt).get("success", false)));

	Dictionary run = service->get_session_runner()->run("phase10-default-agent-session", false);
	REQUIRE(bool(run.get("success", false)));
	CHECK(runtime->get_stream_call_count() == 1);
	const Dictionary request = runtime->get_last_request();
	CHECK(String(request.get("provider", String())) == "fake-coder");
	CHECK(String(request.get("model", String())) == "coder-model");
}

TEST_CASE("[Editor][AgentV1] Session runner applies provider request options") {
	const String workspace = TestUtils::get_temp_path("agent_v1/provider_request_options");

	Ref<AIFakeLLMRuntime> runtime;
	runtime.instantiate();
	runtime->set_response_text("provider options applied");

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());
	service->get_runtime_registry()->register_runtime("fake-provider-options", runtime);

	Dictionary provider;
	provider["type"] = "fake";
	provider["model"] = "options-model";
	provider["max_output_tokens"] = 2048;
	provider["timeout_msec"] = 45000;
	Dictionary providers;
	providers["fake-provider-options"] = provider;

	Dictionary main_agent;
	main_agent["provider"] = "fake-provider-options";
	main_agent["model"] = "options-model";
	Dictionary agents;
	agents["main"] = main_agent;

	Dictionary override_config;
	override_config["default_provider"] = "fake-provider-options";
	override_config["default_model"] = "options-model";
	override_config["providers"] = providers;
	override_config["agents"] = agents;
	service->get_config_service()->set_runtime_override(override_config);

	Dictionary create;
	create["id"] = "provider-request-options";
	create["directory"] = workspace;
	REQUIRE(bool(service->create(create).get("success", false)));

	Dictionary prompt;
	prompt["session_id"] = "provider-request-options";
	prompt["message_id"] = "provider-request-options-prompt";
	prompt["text"] = "Use provider options.";
	prompt["resume"] = false;
	REQUIRE(bool(service->prompt(prompt).get("success", false)));

	Dictionary run = service->get_session_runner()->run("provider-request-options", false);
	REQUIRE(bool(run.get("success", false)));

	const Dictionary request = runtime->get_last_request();
	CHECK(int(request.get("max_output_tokens", 0)) == 2048);
	const Dictionary provider_options = request.get("provider_options", Dictionary());
	CHECK(int(provider_options.get("timeout_msec", 0)) == 45000);
}

TEST_CASE("[Editor][AgentV1] Task tool is idempotent for a parent tool call") {
	const String workspace = TestUtils::get_temp_path("agent_v1/phase10_task_idempotency");

	Ref<AIFakeLLMRuntime> child_runtime;
	child_runtime.instantiate();
	child_runtime->set_response_text("idempotent child result");

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());
	service->get_runtime_registry()->register_runtime("fake-child", child_runtime);

	Dictionary child_provider;
	child_provider["type"] = "fake";
	child_provider["model"] = "child-model";
	Dictionary providers;
	providers["fake-child"] = child_provider;

	Dictionary main_permissions;
	main_permissions["spawn_default_effect"] = "allow";
	Dictionary main_agent;
	main_agent["provider"] = "fake-child";
	main_agent["model"] = "child-model";
	main_agent["permissions"] = main_permissions;
	Dictionary subagents;
	Array allowed;
	allowed.push_back("reviewer");
	subagents["allowed_agent_ids"] = allowed;
	subagents["max_depth"] = 2;
	main_agent["subagents"] = subagents;

	Dictionary reviewer_agent;
	reviewer_agent["provider"] = "fake-child";
	reviewer_agent["model"] = "child-model";

	Dictionary agents;
	agents["main"] = main_agent;
	agents["reviewer"] = reviewer_agent;
	Dictionary override_config;
	override_config["providers"] = providers;
	override_config["agents"] = agents;
	service->get_config_service()->set_runtime_override(override_config);

	Dictionary create;
	create["id"] = "phase10-idempotent-parent";
	create["agent_id"] = "main";
	create["directory"] = workspace;
	REQUIRE(bool(service->create(create).get("success", false)));

	Ref<AIV1ToolMaterialization> materialization = service->get_tool_registry()->materialize_struct(workspace, Array());
	REQUIRE(materialization->has_tool("task"));

	Dictionary task_input;
	task_input["agent_id"] = "reviewer";
	task_input["description"] = "Run once.";
	task_input["prompt"] = "Return one result.";

	Dictionary call;
	call["id"] = "phase10-idempotent-task-call";
	call["name"] = "task";
	call["input"] = task_input;

	Dictionary settle_input;
	settle_input["session_id"] = "phase10-idempotent-parent";
	settle_input["agent"] = "main";
	settle_input["assistant_message_id"] = "phase10-idempotent-assistant";
	settle_input["call"] = call;

	AIError error;
	AIV1ToolSettlement first;
	REQUIRE(materialization->settle_struct(settle_input, first, error));
	REQUIRE(first.success);
	CHECK(phase10_child_session_count(service->get_session_store(), "phase10-idempotent-parent") == 1);
	const String first_child = String(Dictionary(first.structured).get("child_session_id", String()));
	REQUIRE(!first_child.is_empty());

	AIV1ToolSettlement retry;
	REQUIRE(materialization->settle_struct(settle_input, retry, error));
	REQUIRE(retry.success);
	CHECK(phase10_child_session_count(service->get_session_store(), "phase10-idempotent-parent") == 1);
	CHECK(String(Dictionary(retry.structured).get("child_session_id", String())) == first_child);
}

TEST_CASE("[Editor][AgentV1] Agent permission default deny narrows inherited global tool rules") {
	const String workspace = TestUtils::get_temp_path("agent_v1/phase10_agent_permission_narrowing");

	Ref<AIFakeLLMRuntime> runtime;
	runtime.instantiate();
	runtime->set_response_text("restricted agent response");

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());
	service->get_runtime_registry()->register_runtime("fake-restricted", runtime);

	Dictionary provider;
	provider["type"] = "fake";
	provider["model"] = "restricted-model";
	Dictionary providers;
	providers["fake-restricted"] = provider;

	Dictionary restricted_permissions;
	restricted_permissions["default_effect"] = "deny";
	Dictionary restricted_agent;
	restricted_agent["provider"] = "fake-restricted";
	restricted_agent["model"] = "restricted-model";
	restricted_agent["permissions"] = restricted_permissions;
	Dictionary agents;
	agents["restricted"] = restricted_agent;

	Dictionary allow_read;
	allow_read["action"] = "file.read";
	allow_read["resource"] = "*";
	allow_read["effect"] = "allow";
	Array rules;
	rules.push_back(allow_read);
	Dictionary permissions;
	permissions["rules"] = rules;

	Dictionary override_config;
	override_config["default_agent"] = "restricted";
	override_config["providers"] = providers;
	override_config["agents"] = agents;
	override_config["permissions"] = permissions;
	service->get_config_service()->set_runtime_override(override_config);

	Dictionary create;
	create["id"] = "phase10-restricted-agent";
	create["agent_id"] = "restricted";
	create["directory"] = workspace;
	REQUIRE(bool(service->create(create).get("success", false)));

	Dictionary prompt;
	prompt["session_id"] = "phase10-restricted-agent";
	prompt["message_id"] = "phase10-restricted-prompt";
	prompt["text"] = "Show restricted tools.";
	prompt["resume"] = false;
	REQUIRE(bool(service->prompt(prompt).get("success", false)));

	Dictionary run = service->get_session_runner()->run("phase10-restricted-agent", false);
	REQUIRE(bool(run.get("success", false)));
	CHECK_FALSE(phase10_request_has_tool(runtime->get_last_request(), "file_read"));
	CHECK_FALSE(phase10_request_has_tool(runtime->get_last_request(), "file_write"));
}

TEST_CASE("[Editor][AgentV1] Agent service rejects spawning above max concurrent active subagents") {
	const String workspace = TestUtils::get_temp_path("agent_v1/phase10_max_concurrent");

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());

	Dictionary main_agent;
	main_agent["provider"] = "fake";
	main_agent["model"] = "fake-model";
	Dictionary subagents;
	Array allowed;
	allowed.push_back("reviewer");
	subagents["allowed_agent_ids"] = allowed;
	subagents["max_depth"] = 2;
	subagents["max_concurrent"] = 1;
	main_agent["subagents"] = subagents;

	Dictionary reviewer_agent;
	reviewer_agent["provider"] = "fake";
	reviewer_agent["model"] = "fake-model";

	Dictionary agents;
	agents["main"] = main_agent;
	agents["reviewer"] = reviewer_agent;
	Dictionary override_config;
	override_config["agents"] = agents;
	service->get_config_service()->set_runtime_override(override_config);

	Dictionary create_parent;
	create_parent["id"] = "phase10-concurrent-parent";
	create_parent["agent_id"] = "main";
	create_parent["directory"] = workspace;
	REQUIRE(bool(service->create(create_parent).get("success", false)));

	Dictionary child_metadata;
	child_metadata["parent_session_id"] = "phase10-concurrent-parent";
	child_metadata["parent_tool_call_id"] = "phase10-running-call";
	child_metadata["subagent_status"] = "running";
	Dictionary create_child;
	create_child["id"] = "phase10-running-child";
	create_child["agent_id"] = "reviewer";
	create_child["directory"] = workspace;
	create_child["metadata"] = child_metadata;
	REQUIRE(bool(service->create(create_child).get("success", false)));

	AIError error;
	CHECK_FALSE(service->get_agent_service()->assert_can_spawn_struct("phase10-concurrent-parent", "reviewer", error));
	CHECK(error.kind == AI_ERROR_PERMISSION);
}

TEST_CASE("[Editor][AgentV1] Task tool rejects non-positive timeout limits") {
	const String workspace = TestUtils::get_temp_path("agent_v1/phase10_task_timeout");

	Ref<AIFakeLLMRuntime> child_runtime;
	child_runtime.instantiate();
	child_runtime->set_response_text("timeout child result");

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());
	service->get_runtime_registry()->register_runtime("fake-child", child_runtime);

	Dictionary provider;
	provider["type"] = "fake";
	provider["model"] = "child-model";
	Dictionary providers;
	providers["fake-child"] = provider;

	Dictionary main_permissions;
	main_permissions["spawn_default_effect"] = "allow";
	Dictionary main_agent;
	main_agent["provider"] = "fake-child";
	main_agent["model"] = "child-model";
	main_agent["permissions"] = main_permissions;
	Dictionary subagents;
	Array allowed;
	allowed.push_back("reviewer");
	subagents["allowed_agent_ids"] = allowed;
	subagents["max_depth"] = 2;
	subagents["timeout_ms"] = 300000;
	main_agent["subagents"] = subagents;

	Dictionary reviewer_agent;
	reviewer_agent["provider"] = "fake-child";
	reviewer_agent["model"] = "child-model";

	Dictionary agents;
	agents["main"] = main_agent;
	agents["reviewer"] = reviewer_agent;
	Dictionary override_config;
	override_config["providers"] = providers;
	override_config["agents"] = agents;
	service->get_config_service()->set_runtime_override(override_config);

	Dictionary create;
	create["id"] = "phase10-timeout-parent";
	create["agent_id"] = "main";
	create["directory"] = workspace;
	REQUIRE(bool(service->create(create).get("success", false)));

	Ref<AIV1ToolMaterialization> materialization = service->get_tool_registry()->materialize_struct(workspace, Array());
	REQUIRE(materialization->has_tool("task"));

	Dictionary task_input;
	task_input["agent_id"] = "reviewer";
	task_input["description"] = "Timeout should reject.";
	task_input["prompt"] = "This should not run.";
	task_input["timeout_ms"] = 0;

	Dictionary call;
	call["id"] = "phase10-timeout-task-call";
	call["name"] = "task";
	call["input"] = task_input;

	Dictionary settle_input;
	settle_input["session_id"] = "phase10-timeout-parent";
	settle_input["agent"] = "main";
	settle_input["assistant_message_id"] = "phase10-timeout-assistant";
	settle_input["call"] = call;

	AIError error;
	AIV1ToolSettlement settlement;
	REQUIRE(materialization->settle_struct(settle_input, settlement, error));
	CHECK_FALSE(settlement.success);
	CHECK(settlement.error.kind == AI_ERROR_VALIDATION);
	CHECK(phase10_child_session_count(service->get_session_store(), "phase10-timeout-parent") == 0);
}

TEST_CASE("[Editor][AgentV1] Subagent task receives the parent runner cancel token") {
	const String workspace = TestUtils::get_temp_path("agent_v1/phase10_task_cancel_token");

	Ref<Phase10ParentRuntime> parent_runtime;
	parent_runtime.instantiate();
	parent_runtime->task_input["agent_id"] = "reviewer";
	parent_runtime->task_input["description"] = "Observe cancel token.";
	parent_runtime->task_input["prompt"] = "Return after checking token.";

	Ref<Phase10CancelTokenRuntime> child_runtime;
	child_runtime.instantiate();
	Ref<AICancelToken> parent_token;
	parent_token.instantiate();
	child_runtime->expected_token = parent_token;

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());
	service->get_runtime_registry()->register_runtime("fake-parent", parent_runtime);
	service->get_runtime_registry()->register_runtime("fake-child", child_runtime);

	Dictionary parent_provider;
	parent_provider["type"] = "fake";
	parent_provider["model"] = "parent-model";
	Dictionary child_provider;
	child_provider["type"] = "fake";
	child_provider["model"] = "child-model";
	Dictionary providers;
	providers["fake-parent"] = parent_provider;
	providers["fake-child"] = child_provider;

	Dictionary main_agent;
	main_agent["provider"] = "fake-parent";
	main_agent["model"] = "parent-model";
	Dictionary subagents;
	Array allowed;
	allowed.push_back("reviewer");
	subagents["allowed_agent_ids"] = allowed;
	subagents["max_depth"] = 2;
	main_agent["subagents"] = subagents;

	Dictionary reviewer_agent;
	reviewer_agent["provider"] = "fake-child";
	reviewer_agent["model"] = "child-model";

	Dictionary allow_spawn;
	allow_spawn["action"] = "agent.spawn";
	allow_spawn["resource"] = "reviewer";
	allow_spawn["effect"] = "allow";
	Array rules;
	rules.push_back(allow_spawn);
	Dictionary permissions;
	permissions["rules"] = rules;

	Dictionary override_config;
	override_config["providers"] = providers;
	override_config["permissions"] = permissions;
	Dictionary agents;
	agents["main"] = main_agent;
	agents["reviewer"] = reviewer_agent;
	override_config["agents"] = agents;
	service->get_config_service()->set_runtime_override(override_config);

	Dictionary create;
	create["id"] = "phase10-cancel-parent";
	create["agent_id"] = "main";
	create["directory"] = workspace;
	REQUIRE(bool(service->create(create).get("success", false)));

	Dictionary prompt;
	prompt["session_id"] = "phase10-cancel-parent";
	prompt["message_id"] = "phase10-cancel-prompt";
	prompt["text"] = "Delegate and preserve cancel token.";
	prompt["resume"] = false;
	REQUIRE(bool(service->prompt(prompt).get("success", false)));

	Vector<AISessionInputRecord> promoted;
	AIError error;
	REQUIRE(service->get_session_runner()->drain_struct("phase10-cancel-parent", parent_token, 0, promoted, error));
	CHECK(child_runtime->stream_call_count == 1);
	CHECK(child_runtime->saw_expected_token);
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

TEST_CASE("[Editor][AgentV1] Session runner records assistant turn before settling local tools") {
	const String workspace = TestUtils::get_temp_path("agent_v1/deferred_tool_settlement");

	Ref<DeferredToolExecutionProbeTool> tool;
	tool.instantiate();
	tool->configure("Records when it is executed relative to provider streaming.", Dictionary());

	Ref<DeferredToolProbeRuntime> runtime;
	runtime.instantiate();
	runtime->tool = tool;

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());
	service->get_runtime_registry()->register_runtime("deferred-provider", runtime);
	tool->event_store = service->get_event_store();
	REQUIRE(service->get_tool_registry()->register_tool_struct("deferred_probe", tool, "test"));

	Dictionary provider;
	provider["type"] = "fake";
	provider["model"] = "deferred-model";
	Dictionary providers;
	providers["deferred-provider"] = provider;

	Dictionary main_agent;
	main_agent["provider"] = "deferred-provider";
	main_agent["model"] = "deferred-model";
	Dictionary agents;
	agents["main"] = main_agent;

	Dictionary override_config;
	override_config["default_provider"] = "deferred-provider";
	override_config["default_model"] = "deferred-model";
	override_config["providers"] = providers;
	override_config["agents"] = agents;
	service->get_config_service()->set_runtime_override(override_config);

	Dictionary create;
	create["id"] = "deferred-tool-session";
	create["directory"] = workspace;
	REQUIRE(bool(service->create(create).get("success", false)));

	Dictionary prompt;
	prompt["session_id"] = "deferred-tool-session";
	prompt["message_id"] = "deferred-tool-prompt";
	prompt["text"] = "Use a local tool.";
	prompt["resume"] = false;
	REQUIRE(bool(service->prompt(prompt).get("success", false)));

	Dictionary run = service->get_session_runner()->run("deferred-tool-session", false);
	REQUIRE(bool(run.get("success", false)));
	CHECK(runtime->execute_count_after_tool_push == 0);
	CHECK(tool->execute_count == 1);
	CHECK(tool->saw_text_ended_before_execute);
	CHECK(tool->saw_step_ended_before_execute);
	CHECK(runtime->stream_call_count == 2);

	const Vector<AIEventRow> events = service->get_event_store()->list("deferred-tool-session");
	int tool_called_index = -1;
	int text_ended_index = -1;
	int step_ended_index = -1;
	int tool_success_index = -1;
	for (int i = 0; i < events.size(); i++) {
		if (events[i].type == AIDomainEventTypes::tool_called()) {
			tool_called_index = i;
		} else if (events[i].type == AIDomainEventTypes::text_ended() && text_ended_index < 0) {
			text_ended_index = i;
		} else if (events[i].type == AIDomainEventTypes::step_ended() && step_ended_index < 0) {
			step_ended_index = i;
		} else if (events[i].type == AIDomainEventTypes::tool_success()) {
			tool_success_index = i;
		}
	}
	REQUIRE(tool_called_index >= 0);
	REQUIRE(text_ended_index >= 0);
	REQUIRE(step_ended_index >= 0);
	REQUIRE(tool_success_index >= 0);
	CHECK(tool_called_index < text_ended_index);
	CHECK(text_ended_index < tool_success_index);
	CHECK(step_ended_index < tool_success_index);
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

	const Dictionary request_metadata = last_request.get("metadata", Dictionary());
	const Array config_sources = request_metadata.get("config_sources", Array());
	const String config_sources_json = Variant(config_sources).stringify();
	CHECK_FALSE(config_sources_json.contains("phase7-secret"));
	bool saw_runtime_config_source = false;
	for (int i = 0; i < config_sources.size(); i++) {
		if (config_sources[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary source = config_sources[i];
		saw_runtime_config_source = saw_runtime_config_source || String(source.get("source", String())) == "runtime";
		CHECK_FALSE(source.has("data"));
	}
	CHECK(saw_runtime_config_source);

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
	Dictionary read_metadata = read_settlement.metadata;
	Dictionary read_identity = read_metadata.get("registration_identity", Dictionary());
	CHECK(String(read_identity.get("name", String())) == "file_read");
	CHECK(String(Dictionary(read_identity.get("metadata", Dictionary())).get("source", String())) == "builtin");
	Dictionary read_permission = read_metadata.get("permission_decision", Dictionary());
	CHECK(String(read_permission.get("status", String())) == "allowed");
	CHECK(String(read_permission.get("effect", String())) == "allow");
	CHECK(String(read_permission.get("action", String())) == "file.read");
	CHECK(String(read_permission.get("resource", String())).ends_with("readme.txt"));

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
	Dictionary rejected_metadata = rejected_write_settlement.metadata;
	Dictionary rejected_identity = rejected_metadata.get("registration_identity", Dictionary());
	CHECK(String(rejected_identity.get("name", String())) == "file_write");
	CHECK(String(Dictionary(rejected_identity.get("metadata", Dictionary())).get("source", String())) == "builtin");
	Dictionary rejected_permission = rejected_metadata.get("permission_decision", Dictionary());
	CHECK(String(rejected_permission.get("status", String())) == "rejected");
	CHECK(String(rejected_permission.get("reply", String())) == "reject");
	CHECK(String(rejected_permission.get("action", String())) == "file.write");
	CHECK(String(rejected_permission.get("resource", String())).ends_with("generated.txt"));

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

TEST_CASE("[Editor][AgentV1] Session service registers migrated editor tools for agent calls") {
	Ref<AISessionService> service;
	service.instantiate();

	Ref<AIV1ToolRegistry> registry = service->get_tool_registry();
	REQUIRE(registry.is_valid());

	const char *expected_tools[] = {
		"project_list_tree",
		"project_read_file",
		"project_search_text",
		"project_attach_multimodal_file",
		"project_create_markdown",
		"agent_collect_requirements",
		"editor_get_context",
		"docs_search",
		"editor_run_scene",
		"editor_stop_running_scene",
		"editor_get_terminal_errors",
		"project_create_folder",
		"scene_describe_tree",
		"scene_inspect_node",
		"scene_list_properties",
		"scene_apply_patch",
		"scene_delete_node",
		"script_inspect",
		"script_create",
		"script_write",
		"script_patch_function",
		"script_bind_to_node",
		"script_unbind_from_node",
		"script_delete",
		"shader_create",
		"shader_edit",
		"shader_apply_to_node",
		"shader_set_parameters",
		"shader_delete",
	};

	Ref<AIV1ToolMaterialization> materialization = registry->materialize_struct();
	REQUIRE(materialization.is_valid());
	for (const char *expected_tool : expected_tools) {
		CHECK(registry->has_tool(expected_tool));
		CHECK(materialization->has_tool(expected_tool));
	}

	Dictionary list_call;
	list_call["id"] = "call-project-list-tree";
	list_call["name"] = "project_list_tree";
	list_call["input"] = Dictionary();

	Dictionary settle_input;
	settle_input["session_id"] = "session-migrated-tools";
	settle_input["assistant_message_id"] = "assistant-migrated-tools";
	settle_input["call"] = list_call;

	AIV1ToolSettlement settlement;
	AIError error;
	REQUIRE(materialization->settle_struct(settle_input, settlement, error));
	CHECK(settlement.success);
	CHECK(String(settlement.content).contains("Project tree under res://"));
	CHECK(String(settlement.metadata.get("tool_origin", String())) == "agent_v1");
	CHECK(String(settlement.metadata.get("legacy_tool_name", String())) == "project.list_tree");
}

TEST_CASE("[Editor][AgentV1] Project list tree accepts JSON number values for integer arguments") {
	Ref<AISessionService> service;
	service.instantiate();

	Ref<AIV1ToolRegistry> registry = service->get_tool_registry();
	REQUIRE(registry.is_valid());
	Ref<AIV1ToolMaterialization> materialization = registry->materialize_struct();
	REQUIRE(materialization.is_valid());
	REQUIRE(materialization->has_tool("project_list_tree"));

	Dictionary args;
	args["max_depth"] = 2.0;
	Dictionary list_call;
	list_call["id"] = "call-project-list-tree-float-depth";
	list_call["name"] = "project_list_tree";
	list_call["input"] = args;

	Dictionary settle_input;
	settle_input["session_id"] = "session-project-list-tree-float-depth";
	settle_input["assistant_message_id"] = "assistant-project-list-tree-float-depth";
	settle_input["call"] = list_call;

	AIV1ToolSettlement settlement;
	AIError error;
	REQUIRE(materialization->settle_struct(settle_input, settlement, error));
	CHECK(settlement.success);
	CHECK_FALSE(settlement.error.message.contains("Invalid tool argument type"));
	CHECK(String(settlement.content).contains("Project tree under res://"));
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

TEST_CASE("[Editor][AgentV1] Tool registry coerces numeric string arguments before validation") {
	Ref<AIV1ToolRegistry> registry;
	registry.instantiate();

	Ref<CoercedIntegerProbeTool> tool;
	tool.instantiate();
	Dictionary schema;
	schema["type"] = "object";
	Dictionary properties;
	Dictionary max_depth_property;
	max_depth_property["type"] = "integer";
	properties["max_depth"] = max_depth_property;
	Array required;
	required.push_back("max_depth");
	schema["properties"] = properties;
	schema["required"] = required;
	tool->configure("Records whether max_depth was coerced to an integer.", schema);
	REQUIRE(registry->register_tool_struct("coerce_integer_probe", tool, "test"));

	Ref<AIV1ToolMaterialization> materialization = registry->materialize_struct();
	REQUIRE(materialization->has_tool("coerce_integer_probe"));

	Dictionary input;
	input["max_depth"] = "3";
	Dictionary call;
	call["id"] = "call-coerce-integer";
	call["name"] = "coerce_integer_probe";
	call["input"] = input;
	Dictionary settle_input;
	settle_input["session_id"] = "session-coerce-integer";
	settle_input["assistant_message_id"] = "assistant-coerce-integer";
	settle_input["call"] = call;

	AIV1ToolSettlement settlement;
	AIError error;
	REQUIRE(materialization->settle_struct(settle_input, settlement, error));
	REQUIRE(settlement.success);
	CHECK(tool->saw_integer_argument);
	CHECK(tool->max_depth == 3);
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

TEST_CASE("[Editor][AgentV1] Compaction service preserves events and registers summary context") {
	Ref<AIEventStore> event_store;
	event_store.instantiate();
	event_store->set_base_dir(String());

	Ref<AISessionProjector> projector;
	projector.instantiate();

	Ref<AIContextSourceRegistry> context_registry;
	context_registry.instantiate();

	Ref<AIContextEpochStore> epoch_store;
	epoch_store.instantiate();

	Ref<AIContextEpochService> epoch_service;
	epoch_service.instantiate();
	epoch_service->set_event_store(event_store);
	epoch_service->set_projector(projector);
	epoch_service->set_epoch_store(epoch_store);

	String event_error;
	AIEventRow row;

	AIPrompt first_prompt;
	first_prompt.text = "Phase 11 user goal: preserve compacted facts. " + String("Important implementation detail. ").repeat(80);
	Dictionary first_user;
	first_user["id"] = "phase11-user-1";
	first_user["message_id"] = "phase11-user-1";
	first_user["prompt"] = first_prompt.to_dictionary();
	REQUIRE(event_store->append("phase11-compact", AIDomainEventTypes::prompt_promoted(), first_user, false, row, event_error));

	Dictionary first_step;
	first_step["assistant_message_id"] = "phase11-assistant-1";
	REQUIRE(event_store->append("phase11-compact", AIDomainEventTypes::step_started(), first_step, false, row, event_error));

	Dictionary first_text;
	first_text["assistant_message_id"] = "phase11-assistant-1";
	first_text["content_id"] = "phase11-text-1";
	first_text["text"] = "Decision: keep original events queryable after compaction. " + String("Durable audit trail remains authoritative. ").repeat(80);
	REQUIRE(event_store->append("phase11-compact", AIDomainEventTypes::text_ended(), first_text, false, row, event_error));
	REQUIRE(event_store->append("phase11-compact", AIDomainEventTypes::step_ended(), first_step, false, row, event_error));

	AIPrompt recent_prompt;
	recent_prompt.text = "Recent user message should stay outside the compacted prefix.";
	Dictionary recent_user;
	recent_user["id"] = "phase11-user-2";
	recent_user["message_id"] = "phase11-user-2";
	recent_user["prompt"] = recent_prompt.to_dictionary();
	REQUIRE(event_store->append("phase11-compact", AIDomainEventTypes::prompt_promoted(), recent_user, false, row, event_error));

	const int original_event_count = event_store->list("phase11-compact").size();

	Ref<AICompactionService> compaction;
	compaction.instantiate();
	compaction->set_event_store(event_store);
	compaction->set_projector(projector);
	compaction->set_context_source_registry(context_registry);
	compaction->set_context_epoch_service(epoch_service);

	Dictionary input;
	input["session_id"] = "phase11-compact";
	input["reason"] = "manual";
	input["target_token_budget"] = 12;
	const Dictionary result = compaction->compact(input);
	REQUIRE(bool(result.get("success", false)));
	CHECK(String(result.get("summary", String())).contains("Conversation Summary"));
	CHECK(int64_t(result.get("token_before", 0)) > int64_t(result.get("token_after", 0)));

	const Vector<AIEventRow> events = event_store->list("phase11-compact");
	CHECK(events.size() == original_event_count + 1);
	bool saw_original_prompt = false;
	bool saw_original_text = false;
	int64_t compaction_seq = 0;
	for (int i = 0; i < events.size(); i++) {
		saw_original_prompt = saw_original_prompt || events[i].type == AIDomainEventTypes::prompt_promoted();
		saw_original_text = saw_original_text || events[i].type == AIDomainEventTypes::text_ended();
		if (events[i].type == AIDomainEventTypes::compaction_ended()) {
			compaction_seq = events[i].seq;
		}
	}
	CHECK(saw_original_prompt);
	CHECK(saw_original_text);
	REQUIRE(compaction_seq > 0);

	const Vector<AISessionMessage> projected = projector->get_messages_struct("phase11-compact");
	const Vector<AISessionMessage> runner_history = AISessionHistory::entries_for_runner(projected, 0);
	REQUIRE(!runner_history.is_empty());
	CHECK(runner_history[0].type == AI_SESSION_MESSAGE_COMPACTION);
	for (int i = 1; i < runner_history.size(); i++) {
		CHECK(runner_history[i].seq >= compaction_seq);
	}

	AISystemContext context;
	AIError context_error;
	AILocationRef location;
	CHECK(context_registry->load_session_struct("phase11-compact", "main", location, "fake", "fake-model", context, context_error));
	CHECK(context.baseline.contains("Conversation Summary"));
	CHECK(context.baseline.contains("preserve compacted facts"));

	AISystemContext other_context;
	CHECK(context_registry->load_session_struct("phase11-other", "main", location, "fake", "fake-model", other_context, context_error));
	CHECK_FALSE(other_context.baseline.contains("Conversation Summary"));
}

TEST_CASE("[Editor][AgentV1] Repeated compaction carries forward prior summary facts") {
	Ref<AIEventStore> event_store;
	event_store.instantiate();
	event_store->set_base_dir(String());

	Ref<AISessionProjector> projector;
	projector.instantiate();

	Ref<AIContextSourceRegistry> context_registry;
	context_registry.instantiate();

	Ref<AIContextEpochStore> epoch_store;
	epoch_store.instantiate();

	Ref<AIContextEpochService> epoch_service;
	epoch_service.instantiate();
	epoch_service->set_event_store(event_store);
	epoch_service->set_projector(projector);
	epoch_service->set_epoch_store(epoch_store);

	Ref<AICompactionService> compaction;
	compaction.instantiate();
	compaction->set_event_store(event_store);
	compaction->set_projector(projector);
	compaction->set_context_source_registry(context_registry);
	compaction->set_context_epoch_service(epoch_service);

	String event_error;
	AIEventRow row;
	AIPrompt first_prompt;
	first_prompt.text = "First user goal with cumulative compaction facts.";
	Dictionary first_user;
	first_user["id"] = "phase11-cumulative-user-1";
	first_user["message_id"] = "phase11-cumulative-user-1";
	first_user["prompt"] = first_prompt.to_dictionary();
	REQUIRE(event_store->append("phase11-cumulative-compact", AIDomainEventTypes::prompt_promoted(), first_user, false, row, event_error));

	Dictionary first_step;
	first_step["assistant_message_id"] = "phase11-cumulative-assistant-1";
	REQUIRE(event_store->append("phase11-cumulative-compact", AIDomainEventTypes::step_started(), first_step, false, row, event_error));

	Dictionary first_text;
	first_text["assistant_message_id"] = "phase11-cumulative-assistant-1";
	first_text["content_id"] = "phase11-cumulative-text-1";
	first_text["text"] = "First durable decision must survive later compaction.";
	REQUIRE(event_store->append("phase11-cumulative-compact", AIDomainEventTypes::text_ended(), first_text, false, row, event_error));
	REQUIRE(event_store->append("phase11-cumulative-compact", AIDomainEventTypes::step_ended(), first_step, false, row, event_error));

	AIPrompt bridge_prompt;
	bridge_prompt.text = "Bridge message that remains after first compaction.";
	Dictionary bridge_user;
	bridge_user["id"] = "phase11-cumulative-user-2";
	bridge_user["message_id"] = "phase11-cumulative-user-2";
	bridge_user["prompt"] = bridge_prompt.to_dictionary();
	REQUIRE(event_store->append("phase11-cumulative-compact", AIDomainEventTypes::prompt_promoted(), bridge_user, false, row, event_error));

	Dictionary first_input;
	first_input["session_id"] = "phase11-cumulative-compact";
	first_input["reason"] = "manual";
	first_input["target_token_budget"] = 12;
	const Dictionary first_result = compaction->compact(first_input);
	REQUIRE(bool(first_result.get("success", false)));
	CHECK(String(first_result.get("summary", String())).contains("First durable decision"));

	Dictionary second_step;
	second_step["assistant_message_id"] = "phase11-cumulative-assistant-2";
	REQUIRE(event_store->append("phase11-cumulative-compact", AIDomainEventTypes::step_started(), second_step, false, row, event_error));

	Dictionary second_text;
	second_text["assistant_message_id"] = "phase11-cumulative-assistant-2";
	second_text["content_id"] = "phase11-cumulative-text-2";
	second_text["text"] = "Second durable decision joins the cumulative summary.";
	REQUIRE(event_store->append("phase11-cumulative-compact", AIDomainEventTypes::text_ended(), second_text, false, row, event_error));
	REQUIRE(event_store->append("phase11-cumulative-compact", AIDomainEventTypes::step_ended(), second_step, false, row, event_error));

	AIPrompt latest_prompt;
	latest_prompt.text = "Latest message stays outside second compaction.";
	Dictionary latest_user;
	latest_user["id"] = "phase11-cumulative-user-3";
	latest_user["message_id"] = "phase11-cumulative-user-3";
	latest_user["prompt"] = latest_prompt.to_dictionary();
	REQUIRE(event_store->append("phase11-cumulative-compact", AIDomainEventTypes::prompt_promoted(), latest_user, false, row, event_error));

	Dictionary second_input;
	second_input["session_id"] = "phase11-cumulative-compact";
	second_input["reason"] = "manual";
	second_input["target_token_budget"] = 12;
	const Dictionary second_result = compaction->compact(second_input);
	REQUIRE(bool(second_result.get("success", false)));
	const String second_summary = second_result.get("summary", String());
	CHECK(second_summary.contains("First durable decision"));
	CHECK(second_summary.contains("Second durable decision"));
	CHECK_FALSE(second_summary.contains("Previous Summary"));
	CHECK(count_occurrences(second_summary, "## Conversation Summary") == 1);

	const Vector<AISessionMessage> projected = projector->get_messages_struct("phase11-cumulative-compact");
	const Vector<AISessionMessage> runner_history = AISessionHistory::entries_for_runner(projected, 0);
	REQUIRE(!runner_history.is_empty());
	CHECK(runner_history[0].type == AI_SESSION_MESSAGE_COMPACTION);
	CHECK(runner_history[0].text.contains("First durable decision"));
	CHECK(runner_history[0].text.contains("Second durable decision"));

	AISystemContext context;
	AIError context_error;
	AILocationRef location;
	CHECK(context_registry->load_session_struct("phase11-cumulative-compact", "main", location, "fake", "fake-model", context, context_error));
	CHECK(count_occurrences(context.baseline, "## Conversation Summary") == 1);
	CHECK(context.baseline.contains("First durable decision"));
	CHECK(context.baseline.contains("Second durable decision"));
}

TEST_CASE("[Editor][AgentV1] Compaction summary is bounded for long sessions") {
	Ref<AIEventStore> event_store;
	event_store.instantiate();
	event_store->set_base_dir(String());

	Ref<AISessionProjector> projector;
	projector.instantiate();

	Ref<AICompactionService> compaction;
	compaction.instantiate();
	compaction->set_event_store(event_store);
	compaction->set_projector(projector);

	String event_error;
	AIEventRow row;
	for (int i = 0; i < 12; i++) {
		AIPrompt prompt;
		prompt.text = "Long compacted user goal " + itos(i) + ". " + String("persistent phase11 fact. ").repeat(50);
		Dictionary user;
		user["id"] = "phase11-bounded-user-" + itos(i);
		user["message_id"] = "phase11-bounded-user-" + itos(i);
		user["prompt"] = prompt.to_dictionary();
		REQUIRE(event_store->append("phase11-bounded-compact", AIDomainEventTypes::prompt_promoted(), user, false, row, event_error));

		Dictionary step;
		step["assistant_message_id"] = "phase11-bounded-assistant-" + itos(i);
		REQUIRE(event_store->append("phase11-bounded-compact", AIDomainEventTypes::step_started(), step, false, row, event_error));

		Dictionary text;
		text["assistant_message_id"] = "phase11-bounded-assistant-" + itos(i);
		text["content_id"] = "phase11-bounded-text-" + itos(i);
		text["text"] = "Long compacted assistant decision " + itos(i) + ". " + String("durable implementation detail. ").repeat(50);
		REQUIRE(event_store->append("phase11-bounded-compact", AIDomainEventTypes::text_ended(), text, false, row, event_error));
		REQUIRE(event_store->append("phase11-bounded-compact", AIDomainEventTypes::step_ended(), step, false, row, event_error));
	}

	AIPrompt recent_prompt;
	recent_prompt.text = "Recent message remains outside bounded compaction.";
	Dictionary recent_user;
	recent_user["id"] = "phase11-bounded-recent";
	recent_user["message_id"] = "phase11-bounded-recent";
	recent_user["prompt"] = recent_prompt.to_dictionary();
	REQUIRE(event_store->append("phase11-bounded-compact", AIDomainEventTypes::prompt_promoted(), recent_user, false, row, event_error));

	Dictionary input;
	input["session_id"] = "phase11-bounded-compact";
	input["reason"] = "manual";
	input["target_token_budget"] = 20;
	const Dictionary result = compaction->compact(input);
	REQUIRE(bool(result.get("success", false)));
	const String summary = result.get("summary", String());
	CHECK(summary.length() <= 512);
	CHECK(int64_t(result.get("token_after", 0)) <= 128);
}

TEST_CASE("[Editor][AgentV1] Runner triggers compaction before over-budget provider turn") {
	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());

	Dictionary history;
	history["max_tokens"] = 24;
	Dictionary override_config;
	override_config["history"] = history;
	service->get_config_service()->set_runtime_override(override_config);

	Dictionary create;
	create["id"] = "phase11-auto-compact";
	REQUIRE(bool(service->create(create).get("success", false)));

	String event_error;
	AIEventRow row;
	AIPrompt old_prompt;
	old_prompt.text = "Old long goal that should be compacted. " + String("phase11 auto compact detail. ").repeat(100);
	Dictionary old_user;
	old_user["id"] = "phase11-auto-old-user";
	old_user["message_id"] = "phase11-auto-old-user";
	old_user["prompt"] = old_prompt.to_dictionary();
	REQUIRE(service->get_event_store()->append("phase11-auto-compact", AIDomainEventTypes::prompt_promoted(), old_user, false, row, event_error));
	service->get_projector()->project(row);

	Dictionary old_step;
	old_step["assistant_message_id"] = "phase11-auto-old-assistant";
	REQUIRE(service->get_event_store()->append("phase11-auto-compact", AIDomainEventTypes::step_started(), old_step, false, row, event_error));
	service->get_projector()->project(row);

	Dictionary old_text;
	old_text["assistant_message_id"] = "phase11-auto-old-assistant";
	old_text["content_id"] = "phase11-auto-old-text";
	old_text["text"] = "Old assistant decision to preserve in summary. " + String("stable fact. ").repeat(100);
	REQUIRE(service->get_event_store()->append("phase11-auto-compact", AIDomainEventTypes::text_ended(), old_text, false, row, event_error));
	service->get_projector()->project(row);
	REQUIRE(service->get_event_store()->append("phase11-auto-compact", AIDomainEventTypes::step_ended(), old_step, false, row, event_error));
	service->get_projector()->project(row);

	Dictionary prompt;
	prompt["session_id"] = "phase11-auto-compact";
	prompt["message_id"] = "phase11-auto-new-user";
	prompt["text"] = "Continue after automatic compaction.";
	prompt["resume"] = false;
	REQUIRE(bool(service->prompt(prompt).get("success", false)));

	Dictionary run = service->get_session_runner()->run("phase11-auto-compact", false);
	REQUIRE(bool(run.get("success", false)));

	const Vector<AIEventRow> events = service->get_event_store()->list("phase11-auto-compact");
	bool saw_compaction = false;
	bool saw_provider_step = false;
	for (int i = 0; i < events.size(); i++) {
		saw_compaction = saw_compaction || events[i].type == AIDomainEventTypes::compaction_ended();
		saw_provider_step = saw_provider_step || (events[i].type == AIDomainEventTypes::step_started() && String(events[i].data.get("assistant_message_id", String())) != "phase11-auto-old-assistant");
	}
	CHECK(saw_compaction);
	CHECK(saw_provider_step);
}

TEST_CASE("[Editor][AgentV1] Startup recovery fails open steps and tools without dropping admitted input") {
	Ref<AISessionStore> session_store;
	session_store.instantiate();
	session_store->set_base_dir(String());

	Dictionary create;
	create["id"] = "phase11-recovery";
	AISessionRow session;
	bool created = false;
	String session_error;
	REQUIRE(session_store->create_or_reuse(create, session, created, session_error));

	Ref<AIEventStore> event_store;
	event_store.instantiate();
	event_store->set_base_dir(String());

	Ref<AISessionProjector> projector;
	projector.instantiate();

	String event_error;
	AIEventRow row;
	AIPrompt prompt;
	prompt.text = "Admitted input survives process restart.";
	Dictionary admitted;
	admitted["id"] = "phase11-admitted-input";
	admitted["session_id"] = "phase11-recovery";
	admitted["message_id"] = "phase11-admitted-message";
	admitted["prompt"] = prompt.to_dictionary();
	REQUIRE(event_store->append("phase11-recovery", AIDomainEventTypes::prompt_admitted(), admitted, false, row, event_error));

	Dictionary step_started;
	step_started["assistant_message_id"] = "phase11-recovery-assistant";
	step_started["request_id"] = "phase11-recovery-request";
	REQUIRE(event_store->append("phase11-recovery", AIDomainEventTypes::step_started(), step_started, false, row, event_error));

	Dictionary tool_called;
	tool_called["assistant_message_id"] = "phase11-recovery-assistant";
	tool_called["call_id"] = "phase11-recovery-call";
	tool_called["tool"] = "file_read";
	Dictionary tool_input;
	tool_input["path"] = "res://lost.txt";
	tool_called["input"] = tool_input;
	REQUIRE(event_store->append("phase11-recovery", AIDomainEventTypes::tool_called(), tool_called, false, row, event_error));

	Ref<AIStartupRecovery> recovery;
	recovery.instantiate();
	recovery->set_session_store(session_store);
	recovery->set_event_store(event_store);
	recovery->set_projector(projector);

	Dictionary report = recovery->recover();
	REQUIRE(bool(report.get("success", false)));
	CHECK(Array(report.get("interrupted_sessions", Array())).has("phase11-recovery"));
	CHECK(Array(report.get("failed_tool_calls", Array())).has("phase11-recovery-call"));

	report = recovery->recover();
	REQUIRE(bool(report.get("success", false)));

	const Vector<AIEventRow> events = event_store->list("phase11-recovery");
	int admitted_count = 0;
	int step_failed_count = 0;
	int tool_failed_count = 0;
	for (int i = 0; i < events.size(); i++) {
		if (events[i].type == AIDomainEventTypes::prompt_admitted()) {
			admitted_count++;
		} else if (events[i].type == AIDomainEventTypes::step_failed()) {
			step_failed_count++;
		} else if (events[i].type == AIDomainEventTypes::tool_failed()) {
			tool_failed_count++;
		}
	}
	CHECK(admitted_count == 1);
	CHECK(step_failed_count == 1);
	CHECK(tool_failed_count == 1);

	const Vector<AISessionMessage> messages = projector->get_messages_struct("phase11-recovery");
	REQUIRE(messages.size() == 1);
	CHECK(messages[0].metadata.get("status", String()) == "failed");
	REQUIRE(messages[0].content.size() == 1);
	CHECK(messages[0].content[0].tool_state.status == AI_TOOL_STATUS_FAILED);
}

TEST_CASE("[Editor][AgentV1] Interrupt persists request and terminal events for open activity") {
	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());

	Dictionary create;
	create["id"] = "phase11-interrupt";
	REQUIRE(bool(service->create(create).get("success", false)));

	String event_error;
	AIEventRow row;
	Dictionary step_started;
	step_started["assistant_message_id"] = "phase11-interrupt-assistant";
	step_started["request_id"] = "phase11-interrupt-request";
	REQUIRE(service->get_event_store()->append("phase11-interrupt", AIDomainEventTypes::step_started(), step_started, false, row, event_error));

	Dictionary tool_called;
	tool_called["assistant_message_id"] = "phase11-interrupt-assistant";
	tool_called["call_id"] = "phase11-interrupt-call";
	tool_called["tool"] = "file_read";
	REQUIRE(service->get_event_store()->append("phase11-interrupt", AIDomainEventTypes::tool_called(), tool_called, false, row, event_error));

	Dictionary interrupt;
	interrupt["session_id"] = "phase11-interrupt";
	interrupt["reason"] = "user requested stop";
	const Dictionary result = service->interrupt(interrupt);
	REQUIRE(bool(result.get("success", false)));

	const Vector<AIEventRow> events = service->get_event_store()->list("phase11-interrupt");
	bool saw_interrupt = false;
	bool saw_step_failed = false;
	bool saw_tool_failed = false;
	for (int i = 0; i < events.size(); i++) {
		saw_interrupt = saw_interrupt || events[i].type == AIDomainEventTypes::interrupt_requested();
		saw_step_failed = saw_step_failed || events[i].type == AIDomainEventTypes::step_failed();
		saw_tool_failed = saw_tool_failed || events[i].type == AIDomainEventTypes::tool_failed();
	}
	CHECK(saw_interrupt);
	CHECK(saw_step_failed);
	CHECK(saw_tool_failed);
}

TEST_CASE("[Editor][AgentV1] Interrupt cancels active drain before service cleanup writes terminal events") {
	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());

	Ref<AISessionExecution> execution;
	execution.instantiate();
	Ref<BlockingDrainRunner> runner;
	runner.instantiate();
	execution->set_runner(runner);
	service->set_execution(execution);
	execution->set_runner(runner);

	Dictionary create;
	create["id"] = "phase11-active-interrupt";
	REQUIRE(bool(service->create(create).get("success", false)));

	String event_error;
	AIEventRow row;
	Dictionary step_started;
	step_started["assistant_message_id"] = "phase11-active-interrupt-assistant";
	step_started["request_id"] = "phase11-active-interrupt-request";
	REQUIRE(service->get_event_store()->append("phase11-active-interrupt", AIDomainEventTypes::step_started(), step_started, false, row, event_error));

	Dictionary tool_called;
	tool_called["assistant_message_id"] = "phase11-active-interrupt-assistant";
	tool_called["call_id"] = "phase11-active-interrupt-call";
	tool_called["tool"] = "file_read";
	REQUIRE(service->get_event_store()->append("phase11-active-interrupt", AIDomainEventTypes::tool_called(), tool_called, false, row, event_error));

	AISessionExecutionState state;
	REQUIRE(execution->wake_struct("phase11-active-interrupt", 1, state));
	REQUIRE(state.active);
	for (int i = 0; i < 1000 && runner->drain_count == 0; i++) {
		OS::get_singleton()->delay_usec(1000);
	}
	REQUIRE(runner->drain_count == 1);

	Dictionary interrupt;
	interrupt["session_id"] = "phase11-active-interrupt";
	interrupt["reason"] = "user cancelled active drain";
	const Dictionary result = service->interrupt(interrupt);
	REQUIRE(bool(result.get("success", false)));
	CHECK(bool(Dictionary(result.get("interrupted", Dictionary())).get("interrupted", false)));

	execution->clear();
	CHECK(runner->saw_cancel);

	const Vector<AIEventRow> events = service->get_event_store()->list("phase11-active-interrupt");
	bool saw_interrupt = false;
	bool saw_step_failed = false;
	bool saw_tool_failed = false;
	for (int i = 0; i < events.size(); i++) {
		saw_interrupt = saw_interrupt || events[i].type == AIDomainEventTypes::interrupt_requested();
		saw_step_failed = saw_step_failed || events[i].type == AIDomainEventTypes::step_failed();
		saw_tool_failed = saw_tool_failed || events[i].type == AIDomainEventTypes::tool_failed();
	}
	CHECK(saw_interrupt);
	CHECK_FALSE(saw_step_failed);
	CHECK_FALSE(saw_tool_failed);
}

TEST_CASE("[Editor][AgentV1] Startup recovery retains permission-pending tools but interrupt fails them") {
	Ref<AISessionStore> session_store;
	session_store.instantiate();
	session_store->set_base_dir(String());

	Dictionary create;
	create["id"] = "phase11-permission-recovery";
	AISessionRow session;
	bool created = false;
	String session_error;
	REQUIRE(session_store->create_or_reuse(create, session, created, session_error));

	Ref<AIEventStore> event_store;
	event_store.instantiate();
	event_store->set_base_dir(String());

	Ref<AISessionProjector> projector;
	projector.instantiate();

	String event_error;
	AIEventRow row;
	Dictionary tool_called;
	tool_called["assistant_message_id"] = "phase11-permission-assistant";
	tool_called["call_id"] = "phase11-permission-call";
	tool_called["tool"] = "file_write";
	REQUIRE(event_store->append("phase11-permission-recovery", AIDomainEventTypes::tool_called(), tool_called, false, row, event_error));

	Dictionary permission_asked;
	permission_asked["request_id"] = "phase11-permission-request";
	permission_asked["session_id"] = "phase11-permission-recovery";
	permission_asked["action"] = "file.write";
	permission_asked["resource"] = "res://pending.txt";
	permission_asked["status"] = "pending";
	permission_asked["source"] = tool_called.duplicate(true);
	REQUIRE(event_store->append("phase11-permission-recovery", AIDomainEventTypes::permission_asked(), permission_asked, false, row, event_error));

	Ref<AIStartupRecovery> recovery;
	recovery.instantiate();
	recovery->set_session_store(session_store);
	recovery->set_event_store(event_store);
	recovery->set_projector(projector);

	const Dictionary report = recovery->recover();
	REQUIRE(bool(report.get("success", false)));
	CHECK_FALSE(Array(report.get("failed_tool_calls", Array())).has("phase11-permission-call"));

	int startup_tool_failed_count = 0;
	Vector<AIEventRow> events = event_store->list("phase11-permission-recovery");
	for (int i = 0; i < events.size(); i++) {
		if (events[i].type == AIDomainEventTypes::tool_failed()) {
			startup_tool_failed_count++;
		}
	}
	CHECK(startup_tool_failed_count == 0);

	Ref<AISessionService> service;
	service.instantiate();
	service->set_session_store(session_store);
	service->set_event_store(event_store);
	service->set_projector(projector);

	Dictionary interrupt;
	interrupt["session_id"] = "phase11-permission-recovery";
	interrupt["reason"] = "user cancelled pending permission";
	const Dictionary interrupt_result = service->interrupt(interrupt);
	REQUIRE(bool(interrupt_result.get("success", false)));

	events = event_store->list("phase11-permission-recovery");
	bool saw_tool_failed = false;
	for (int i = 0; i < events.size(); i++) {
		saw_tool_failed = saw_tool_failed || events[i].type == AIDomainEventTypes::tool_failed();
	}
	CHECK(saw_tool_failed);
}

TEST_CASE("[Editor][AgentV1] Projector exposes live assistant text deltas without duplicating final text") {
	Ref<AISessionProjector> projector;
	projector.instantiate();

	Dictionary started;
	started["assistant_message_id"] = "assistant-live-stream";
	started["request_id"] = "request-live-stream";
	AIEventRow start_row;
	start_row.id = "evt-live-stream-start";
	start_row.aggregate_id = "projector-live-stream";
	start_row.seq = 1;
	start_row.type = AIDomainEventTypes::step_started();
	start_row.data = started;
	REQUIRE(projector->project(start_row));

	Dictionary first_delta;
	first_delta["assistant_message_id"] = "assistant-live-stream";
	first_delta["content_id"] = "content-live-stream";
	first_delta["text"] = "Hel";
	AIEventRow first_row;
	first_row.id = "live-projector-delta-a";
	first_row.aggregate_id = "projector-live-stream";
	first_row.seq = 1;
	first_row.type = AIDomainEventTypes::text_delta();
	first_row.data = first_delta;
	first_row.live_only = true;
	REQUIRE(projector->project(first_row));

	Dictionary second_delta = first_delta.duplicate(true);
	second_delta["text"] = "lo";
	AIEventRow second_row = first_row;
	second_row.id = "live-projector-delta-b";
	second_row.data = second_delta;
	REQUIRE(projector->project(second_row));

	Vector<AISessionMessage> messages = projector->get_messages_struct("projector-live-stream");
	REQUIRE(messages.size() == 1);
	CHECK(messages[0].content.size() == 1);
	if (messages[0].content.size() == 1) {
		CHECK(messages[0].content[0].type == "text");
		CHECK(messages[0].content[0].text == "Hello");
	}

	Dictionary ended;
	ended["assistant_message_id"] = "assistant-live-stream";
	ended["content_id"] = "content-live-stream";
	ended["text"] = "Hello";
	AIEventRow end_row;
	end_row.id = "evt-live-stream-text-ended";
	end_row.aggregate_id = "projector-live-stream";
	end_row.seq = 2;
	end_row.type = AIDomainEventTypes::text_ended();
	end_row.data = ended;
	REQUIRE(projector->project(end_row));

	REQUIRE(projector->project(first_row));
	messages = projector->get_messages_struct("projector-live-stream");
	REQUIRE(messages.size() == 1);
	CHECK(messages[0].content.size() == 1);
	if (messages[0].content.size() == 1) {
		CHECK(messages[0].content[0].text == "Hello");
	}
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Adapter creates sessions and projects UI messages") {
	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());

	Ref<AIAgentV1UIAdapter> adapter;
	adapter.instantiate();
	adapter->set_session_service(service);

	Ref<AgentV1UIAdapterSignalCollector> collector;
	collector.instantiate();
	adapter->connect("messages_changed", callable_mp(collector.ptr(), &AgentV1UIAdapterSignalCollector::on_messages_changed));
	adapter->connect("run_state_changed", callable_mp(collector.ptr(), &AgentV1UIAdapterSignalCollector::on_run_state_changed));

	Dictionary create;
	create["id"] = "ui-session-a";
	create["directory"] = "res://";
	create["agent_id"] = "main";
	create["title"] = "UI Session";
	const Dictionary session = adapter->create_session(create);
	REQUIRE(bool(session.get("success", false)));
	CHECK(String(session.get("id", String())) == "ui-session-a");
	CHECK(adapter->get_active_session_id() == "ui-session-a");

	const Array sessions = adapter->list_sessions();
	REQUIRE(sessions.size() == 1);
	CHECK(String(Dictionary(sessions[0]).get("id", String())) == "ui-session-a");

	const Dictionary sent = adapter->send_message("hello from ui", String(), "main", Array(), false);
	REQUIRE(bool(sent.get("success", false)));

	const Array messages = adapter->get_messages("ui-session-a");
	REQUIRE(messages.size() == 1);
	const Dictionary message = messages[0];
	CHECK(String(message.get("role", String())) == "user");
	CHECK(String(message.get("content", String())) == "hello from ui");
	CHECK(String(message.get("session_id", String())) == "ui-session-a");

	const Dictionary metadata = message.get("metadata", Dictionary());
	CHECK(String(metadata.get("agent_v1_type", String())) == "user");
	CHECK(int64_t(metadata.get("seq", 0)) > 0);

	REQUIRE(collector->message_snapshots.size() >= 1);
	const Dictionary latest_snapshot = collector->message_snapshots[collector->message_snapshots.size() - 1];
	CHECK(String(latest_snapshot.get("session_id", String())) == "ui-session-a");
	CHECK(Array(latest_snapshot.get("messages", Array())).size() == 1);

	REQUIRE(collector->run_states.size() >= 1);
	const Dictionary run_state = collector->run_states[collector->run_states.size() - 1];
	CHECK(String(run_state.get("session_id", String())) == "ui-session-a");
	CHECK_FALSE(bool(run_state.get("busy", true)));
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Adapter keeps promoted user messages in chronological UI order") {
	Ref<AIFakeLLMRuntime> runtime;
	runtime.instantiate();

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());
	service->get_runtime_registry()->register_runtime("fake-ui-order", runtime);

	Dictionary provider;
	provider["type"] = "fake";
	provider["model"] = "fake-model";
	Dictionary providers;
	providers["fake-ui-order"] = provider;
	Dictionary main_agent;
	main_agent["provider"] = "fake-ui-order";
	main_agent["model"] = "fake-model";
	Dictionary agents;
	agents["main"] = main_agent;
	Dictionary override_config;
	override_config["default_provider"] = "fake-ui-order";
	override_config["default_model"] = "fake-model";
	override_config["providers"] = providers;
	override_config["agents"] = agents;
	service->get_config_service()->set_runtime_override(override_config);

	Ref<AIAgentV1UIAdapter> adapter;
	adapter.instantiate();
	adapter->set_session_service(service);

	Dictionary create;
	create["id"] = "ui-session-order";
	create["directory"] = "res://";
	create["agent_id"] = "main";
	REQUIRE(bool(adapter->create_session(create).get("success", false)));

	REQUIRE(bool(adapter->send_message("first user message", String(), "main", Array(), false).get("success", false)));
	runtime->set_response_text("first assistant message");
	REQUIRE(bool(service->get_session_runner()->run("ui-session-order", false).get("success", false)));

	REQUIRE(bool(adapter->send_message("second user message", String(), "main", Array(), false).get("success", false)));
	runtime->set_response_text("second assistant message");
	REQUIRE(bool(service->get_session_runner()->run("ui-session-order", false).get("success", false)));

	const Array messages = adapter->get_messages("ui-session-order");
	REQUIRE(messages.size() == 4);
	CHECK(String(Dictionary(messages[0]).get("role", String())) == "user");
	CHECK(String(Dictionary(messages[0]).get("content", String())) == "first user message");
	CHECK(String(Dictionary(messages[1]).get("role", String())) == "assistant");
	CHECK(String(Dictionary(messages[1]).get("content", String())) == "first assistant message");
	CHECK(String(Dictionary(messages[2]).get("role", String())) == "user");
	CHECK(String(Dictionary(messages[2]).get("content", String())) == "second user message");
	CHECK(String(Dictionary(messages[3]).get("role", String())) == "assistant");
	CHECK(String(Dictionary(messages[3]).get("content", String())) == "second assistant message");
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Adapter streams live assistant deltas through coalesced message snapshots") {
	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());

	Ref<AIAgentV1UIAdapter> adapter;
	adapter.instantiate();
	adapter->set_session_service(service);

	Ref<AgentV1UIAdapterSignalCollector> collector;
	collector.instantiate();
	adapter->connect("messages_changed", callable_mp(collector.ptr(), &AgentV1UIAdapterSignalCollector::on_messages_changed));

	Dictionary create;
	create["id"] = "ui-session-live-stream";
	create["directory"] = "res://";
	REQUIRE(bool(adapter->create_session(create).get("success", false)));
	collector->message_snapshots.clear();

	AIEventRow row;
	String event_error;
	Dictionary started;
	started["assistant_message_id"] = "assistant-ui-live-stream";
	started["request_id"] = "request-ui-live-stream";
	REQUIRE(service->get_event_store()->append("ui-session-live-stream", AIDomainEventTypes::step_started(), started, false, row, event_error));

	Dictionary first_delta;
	first_delta["assistant_message_id"] = "assistant-ui-live-stream";
	first_delta["content_id"] = "content-ui-live-stream";
	first_delta["text"] = "Hel";
	REQUIRE(service->get_event_store()->append("ui-session-live-stream", AIDomainEventTypes::text_delta(), first_delta, true, row, event_error));

	Dictionary second_delta = first_delta.duplicate(true);
	second_delta["text"] = "lo";
	REQUIRE(service->get_event_store()->append("ui-session-live-stream", AIDomainEventTypes::text_delta(), second_delta, true, row, event_error));

	flush_deferred_calls();
	flush_deferred_calls();

	REQUIRE(collector->message_snapshots.size() == 1);
	const Dictionary live_snapshot = collector->message_snapshots[0];
	const Array live_messages = live_snapshot.get("messages", Array());
	REQUIRE(live_messages.size() == 1);
	CHECK(String(Dictionary(live_messages[0]).get("role", String())) == "assistant");
	CHECK(String(Dictionary(live_messages[0]).get("content", String())) == "Hello");

	Dictionary ended;
	ended["assistant_message_id"] = "assistant-ui-live-stream";
	ended["content_id"] = "content-ui-live-stream";
	ended["text"] = "Hello";
	REQUIRE(service->get_event_store()->append("ui-session-live-stream", AIDomainEventTypes::text_ended(), ended, false, row, event_error));

	flush_deferred_calls();
	flush_deferred_calls();

	const Array final_messages = adapter->get_messages("ui-session-live-stream");
	REQUIRE(final_messages.size() == 1);
	CHECK(String(Dictionary(final_messages[0]).get("content", String())) == "Hello");
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Adapter restores the last active session without creating a new one") {
	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());

	Ref<AIAgentV1UIAdapter> adapter;
	adapter.instantiate();
	adapter->set_session_service(service);

	Dictionary first;
	first["id"] = "ui-restore-first";
	first["directory"] = "res://";
	first["agent_id"] = "main";
	first["title"] = "First";
	REQUIRE(bool(adapter->create_session(first).get("success", false)));

	Dictionary second;
	second["id"] = "ui-restore-second";
	second["directory"] = "res://";
	second["agent_id"] = "main";
	second["title"] = "Second";
	REQUIRE(bool(adapter->create_session(second).get("success", false)));
	REQUIRE(adapter->set_active_session("ui-restore-first"));

	Ref<AIAgentV1UIAdapter> restarted_adapter;
	restarted_adapter.instantiate();
	restarted_adapter->set_session_service(service);

	CHECK(restarted_adapter->restore_active_session());
	CHECK(restarted_adapter->get_active_session_id() == "ui-restore-first");
	CHECK(restarted_adapter->list_sessions().size() == 2);
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Adapter isolates sessions by project scope") {
	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());

	Ref<AIAgentV1UIAdapter> project_a_adapter;
	project_a_adapter.instantiate();
	project_a_adapter->set_session_service(service);
	project_a_adapter->set_project_scope("project-a", "res://project-a");

	Dictionary project_a_session;
	project_a_session["id"] = "ui-project-a-session";
	project_a_session["directory"] = "res://project-a";
	project_a_session["workspace_id"] = "project-a";
	project_a_session["title"] = "Project A";
	REQUIRE(bool(project_a_adapter->create_session(project_a_session).get("success", false)));

	Ref<AIAgentV1UIAdapter> project_b_adapter;
	project_b_adapter.instantiate();
	project_b_adapter->set_session_service(service);
	project_b_adapter->set_project_scope("project-b", "res://project-b");

	Dictionary project_b_session;
	project_b_session["id"] = "ui-project-b-session";
	project_b_session["directory"] = "res://project-b";
	project_b_session["workspace_id"] = "project-b";
	project_b_session["title"] = "Project B";
	REQUIRE(bool(project_b_adapter->create_session(project_b_session).get("success", false)));

	const Array project_a_sessions = project_a_adapter->list_sessions();
	REQUIRE(project_a_sessions.size() == 1);
	CHECK(String(Dictionary(project_a_sessions[0]).get("id", String())) == "ui-project-a-session");

	const Array project_b_sessions = project_b_adapter->list_sessions();
	REQUIRE(project_b_sessions.size() == 1);
	CHECK(String(Dictionary(project_b_sessions[0]).get("id", String())) == "ui-project-b-session");

	Ref<AIAgentV1UIAdapter> restarted_a_adapter;
	restarted_a_adapter.instantiate();
	restarted_a_adapter->set_session_service(service);
	restarted_a_adapter->set_project_scope("project-a", "res://project-a");

	CHECK(restarted_a_adapter->restore_active_session());
	CHECK(restarted_a_adapter->get_active_session_id() == "ui-project-a-session");
	CHECK_FALSE(restarted_a_adapter->set_active_session("ui-project-b-session"));
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Adapter archives and deletes sessions without leaving a stale active session") {
	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());

	Ref<AIAgentV1UIAdapter> adapter;
	adapter.instantiate();
	adapter->set_session_service(service);

	Dictionary archive;
	archive["id"] = "ui-session-archive";
	archive["directory"] = "res://";
	REQUIRE(bool(adapter->create_session(archive).get("success", false)));

	Dictionary remove;
	remove["id"] = "ui-session-delete";
	remove["directory"] = "res://";
	REQUIRE(bool(adapter->create_session(remove).get("success", false)));
	REQUIRE(adapter->set_active_session("ui-session-archive"));

	const Dictionary archived = adapter->archive_session("ui-session-archive");
	REQUIRE(bool(archived.get("success", false)));
	CHECK(adapter->get_active_session_id() == "ui-session-delete");
	CHECK_FALSE(adapter->set_active_session("ui-session-archive"));

	Array sessions = adapter->list_sessions();
	REQUIRE(sessions.size() == 1);
	CHECK(String(Dictionary(sessions[0]).get("id", String())) == "ui-session-delete");

	const Dictionary deleted = adapter->delete_session("ui-session-delete");
	REQUIRE(bool(deleted.get("success", false)));
	CHECK(adapter->get_active_session_id().is_empty());
	CHECK(adapter->list_sessions().is_empty());
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Adapter does not expose system context as chat messages") {
	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());

	Ref<AIAgentV1UIAdapter> adapter;
	adapter.instantiate();
	adapter->set_session_service(service);

	Dictionary create;
	create["id"] = "ui-session-system-context";
	create["directory"] = "res://";
	REQUIRE(bool(adapter->create_session(create).get("success", false)));

	Dictionary context;
	context["message_id"] = "ui-system-context-message";
	context["text"] = "You are running inside NextEngine editor agent infrastructure.";
	context["baseline"] = "You are NextEngine Agent.";
	context["agent_id"] = "main";

	AIEventRow row;
	String event_error;
	REQUIRE(service->get_event_store()->append("ui-session-system-context", AIDomainEventTypes::context_updated(), context, false, row, event_error));

	CHECK(adapter->get_messages("ui-session-system-context").is_empty());
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Adapter applies selected UI model profile per session without mutating runtime config") {
	Ref<AIFakeLLMRuntime> runtime_a;
	runtime_a.instantiate();
	runtime_a->set_response_text("model a");

	Ref<AIFakeLLMRuntime> runtime_b;
	runtime_b.instantiate();
	runtime_b->set_response_text("model b");

	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());
	service->get_runtime_registry()->register_runtime("profile-a", runtime_a);
	service->get_runtime_registry()->register_runtime("profile-b", runtime_b);

	Dictionary provider_a;
	provider_a["type"] = "fake";
	provider_a["model"] = "model-a";
	provider_a["ui_model_profile"] = true;
	Dictionary provider_b;
	provider_b["type"] = "fake";
	provider_b["model"] = "model-b";
	provider_b["ui_model_profile"] = true;
	Dictionary providers;
	providers["profile-a"] = provider_a;
	providers["profile-b"] = provider_b;

	Dictionary main_agent;
	main_agent["provider"] = "profile-a";
	main_agent["model"] = "model-a";
	Dictionary agents;
	agents["main"] = main_agent;

	Dictionary override_config;
	override_config["default_provider"] = "profile-a";
	override_config["default_model"] = "model-a";
	override_config["providers"] = providers;
	override_config["agents"] = agents;
	service->get_config_service()->set_runtime_override(override_config);

	Ref<AIAgentV1UIAdapter> adapter;
	adapter.instantiate();
	adapter->set_session_service(service);

	Dictionary create_b;
	create_b["id"] = "ui-session-profile-b";
	create_b["directory"] = "res://";
	create_b["agent_id"] = "main";
	REQUIRE(bool(adapter->create_session(create_b).get("success", false)));
	const Dictionary sent_b = adapter->send_message("run selected profile b", "profile-b", "main", Array(), false);
	REQUIRE(bool(sent_b.get("success", false)));

	Dictionary session_b = service->get_session_store()->get_session("ui-session-profile-b");
	REQUIRE(bool(session_b.get("success", false)));
	Dictionary session_b_metadata = session_b.get("metadata", Dictionary());
	Dictionary session_b_model_ref = session_b_metadata.get("model_ref", Dictionary());
	CHECK(String(session_b_model_ref.get("provider", String())) == "profile-b");
	CHECK(String(session_b_model_ref.get("model", String())) == "model-b");

	Dictionary create_a;
	create_a["id"] = "ui-session-profile-a";
	create_a["directory"] = "res://";
	create_a["agent_id"] = "main";
	REQUIRE(bool(adapter->create_session(create_a).get("success", false)));
	const Dictionary sent_a = adapter->send_message("run selected profile a", "profile-a", "main", Array(), false);
	REQUIRE(bool(sent_a.get("success", false)));

	const Dictionary effective_config = service->get_config_service()->get_config();
	CHECK(String(effective_config.get("default_provider", String())) == "profile-a");
	CHECK(String(effective_config.get("default_model", String())) == "model-a");
	const Dictionary effective_agents = effective_config.get("agents", Dictionary());
	const Dictionary effective_main_agent = effective_agents.get("main", Dictionary());
	CHECK(String(effective_main_agent.get("provider", String())) == "profile-a");
	CHECK(String(effective_main_agent.get("model", String())) == "model-a");

	Dictionary run_b = service->get_session_runner()->run("ui-session-profile-b", false);
	REQUIRE(bool(run_b.get("success", false)));

	const Dictionary request_b = runtime_b->get_last_request();
	CHECK(String(request_b.get("provider", String())) == "profile-b");
	CHECK(String(request_b.get("model", String())) == "model-b");
	CHECK(runtime_a->get_last_request().is_empty());

	Dictionary run_a = service->get_session_runner()->run("ui-session-profile-a", false);
	REQUIRE(bool(run_a.get("success", false)));

	const Dictionary request_a = runtime_a->get_last_request();
	CHECK(String(request_a.get("provider", String())) == "profile-a");
	CHECK(String(request_a.get("model", String())) == "model-a");
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Adapter rejects unknown UI model profile ids") {
	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());

	Ref<AIAgentV1UIAdapter> adapter;
	adapter.instantiate();
	adapter->set_session_service(service);

	Dictionary create;
	create["id"] = "ui-session-missing-profile";
	create["directory"] = "res://";
	create["agent_id"] = "main";
	REQUIRE(bool(adapter->create_session(create).get("success", false)));

	const Dictionary sent = adapter->send_message("should fail", "missing-profile", "main", Array(), false);
	CHECK_FALSE(bool(sent.get("success", true)));
	const Dictionary error = sent.get("error", Dictionary());
	CHECK(String(error.get("kind", String())) == "validation");
	const Dictionary details = error.get("details", Dictionary());
	CHECK(String(details.get("model_profile_id", String())) == "missing-profile");

	const Array messages = adapter->get_messages("ui-session-missing-profile");
	CHECK(messages.is_empty());
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Adapter maps permission pending requests for UI") {
	Ref<AISessionService> service;
	service.instantiate();
	service->get_session_store()->set_base_dir(String());
	service->get_input_store()->set_base_dir(String());
	service->get_event_store()->set_base_dir(String());

	Ref<AIAgentV1UIAdapter> adapter;
	adapter.instantiate();
	adapter->set_session_service(service);

	Ref<AgentV1UIAdapterSignalCollector> collector;
	collector.instantiate();
	adapter->connect("permission_requested", callable_mp(collector.ptr(), &AgentV1UIAdapterSignalCollector::on_permission_requested));

	Dictionary create;
	create["id"] = "ui-permission-session";
	create["directory"] = "res://";
	REQUIRE(bool(adapter->create_session(create).get("success", false)));

	Dictionary source;
	source["tool_name"] = "file_write";
	source["call_id"] = "call-ui";

	Dictionary input;
	input["session_id"] = "ui-permission-session";
	input["action"] = "file.write";
	input["resource"] = "res://ui.txt";
	input["source"] = source;
	const Dictionary decision = service->get_permission_service()->assert_permission(input);
	REQUIRE(bool(decision.get("pending", false)));

	const Array pending = adapter->refresh_pending_permissions();
	REQUIRE(pending.size() == 1);
	const Dictionary request = pending[0];
	CHECK(String(request.get("request_id", String())).begins_with("perm_"));
	CHECK(String(request.get("session_id", String())) == "ui-permission-session");
	CHECK(String(request.get("action", String())) == "file.write");
	CHECK(String(request.get("resource", String())) == "res://ui.txt");

	REQUIRE(collector->permission_requests.size() == 1);
	CHECK(String(Dictionary(collector->permission_requests[0]).get("request_id", String())) == String(request.get("request_id", String())));

	const Dictionary reply = adapter->reply_permission(String(request.get("request_id", String())), false, "not from UI");
	CHECK_FALSE(bool(reply.get("success", true)));
	CHECK(String(reply.get("reply", String())) == "reject");
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Config adapter exposes settings snapshot without old settings") {
	Ref<AIConfigService> config;
	config.instantiate();

	Dictionary fake_provider;
	fake_provider["type"] = "fake";
	fake_provider["model"] = "fake-model";
	Dictionary providers;
	providers["fake"] = fake_provider;

	Dictionary reviewer_agent;
	reviewer_agent["name"] = "Reviewer";
	reviewer_agent["provider"] = "fake";
	reviewer_agent["model"] = "fake-model";
	Dictionary agents;
	agents["reviewer"] = reviewer_agent;

	Dictionary override_config;
	override_config["default_provider"] = "fake";
	override_config["default_model"] = "fake-model";
	override_config["providers"] = providers;
	override_config["agents"] = agents;
	config->set_runtime_override(override_config);

	Ref<AIAgentV1UIConfigAdapter> adapter;
	adapter.instantiate();
	adapter->set_config_service(config);

	const Dictionary snapshot = adapter->get_settings_snapshot();
	const Array models = snapshot.get("models", Array());
	REQUIRE(models.size() == 1);
	CHECK(String(Dictionary(models[0]).get("provider", String())) == "fake");
	CHECK(String(Dictionary(models[0]).get("model", String())) == "fake-model");

	const Array agent_items = snapshot.get("agents", Array());
	REQUIRE(agent_items.size() >= 1);
	bool saw_reviewer = false;
	for (int i = 0; i < agent_items.size(); i++) {
		const Dictionary agent_item = agent_items[i];
		saw_reviewer = saw_reviewer || String(agent_item.get("id", String())) == "reviewer";
	}
	CHECK(saw_reviewer);

	Dictionary patch;
	patch["default_model"] = "fake-model-2";
	const Dictionary patched = adapter->patch_settings(patch, "runtime");
	REQUIRE(bool(patched.get("success", false)));
	CHECK(String(adapter->get_settings_snapshot().get("default_model", String())) == "fake-model-2");
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Config adapter manages model profiles through config service") {
	Ref<AIConfigService> config;
	config.instantiate();

	Dictionary override_config;
	override_config["default_provider"] = "fake";
	override_config["default_model"] = "fake-model";
	override_config["providers"] = Dictionary();
	config->set_runtime_override(override_config);

	Ref<AIAgentV1UIConfigAdapter> adapter;
	adapter.instantiate();
	adapter->set_config_service(config);

	Dictionary profile;
	profile["display_name"] = "OpenAI Primary";
	profile["provider_id"] = "openai";
	profile["model"] = "gpt-5.4";
	profile["api_key"] = "initial-key";
	profile["supports_multimodal"] = true;
	profile["max_output_tokens"] = 2048;
	profile["timeout_msec"] = 45000;
	profile["timeout_seconds"] = 45;
	profile["max_provider_turns"] = 12;
	profile["max_tool_calls"] = 6;
	profile["max_context_chars"] = 98765;
	const Dictionary added = adapter->add_model_profile(profile, "runtime");
	REQUIRE(bool(added.get("success", false)));
	const String profile_id = added.get("id", String());
	REQUIRE_FALSE(profile_id.is_empty());

	Array profiles = adapter->list_model_profiles();
	REQUIRE(profiles.size() == 1);
	Dictionary stored_profile = profiles[0];
	CHECK(String(stored_profile.get("id", String())) == profile_id);
	CHECK(String(stored_profile.get("provider_id", String())) == "openai");
	CHECK(String(stored_profile.get("provider_key", String())) == profile_id);
	CHECK(String(stored_profile.get("model", String())) == "gpt-5.4");
	CHECK(String(stored_profile.get("api_key", String())) == "initial-key");
	CHECK(bool(stored_profile.get("supports_multimodal", false)));
	CHECK(int(stored_profile.get("max_output_tokens", 0)) == 2048);
	CHECK(int(stored_profile.get("timeout_msec", 0)) == 45000);
	CHECK_FALSE(stored_profile.has("metadata"));
	CHECK_FALSE(stored_profile.has("timeout_seconds"));
	CHECK_FALSE(stored_profile.has("max_provider_turns"));
	CHECK_FALSE(stored_profile.has("max_tool_calls"));
	CHECK_FALSE(stored_profile.has("max_context_chars"));

	const Dictionary provider_config = config->get_provider_config(profile_id);
	CHECK(String(provider_config.get("type", String())) == "openai-compatible");
	CHECK(String(provider_config.get("model", String())) == "gpt-5.4");
	CHECK(bool(provider_config.get("ui_model_profile", false)));
	CHECK(bool(provider_config.get("supports_multimodal", false)));
	CHECK(int(provider_config.get("max_output_tokens", 0)) == 2048);
	CHECK(int(provider_config.get("timeout_msec", 0)) == 45000);
	CHECK_FALSE(provider_config.has("timeout_seconds"));
	CHECK_FALSE(provider_config.has("max_provider_turns"));
	CHECK_FALSE(provider_config.has("max_tool_calls"));
	CHECK_FALSE(provider_config.has("max_context_chars"));

	Dictionary stale_provider_patch;
	stale_provider_patch["max_provider_turns"] = 99;
	stale_provider_patch["max_tool_calls"] = 88;
	stale_provider_patch["max_context_chars"] = 777;
	stale_provider_patch["timeout_seconds"] = 12;
	Dictionary stale_providers_patch;
	stale_providers_patch[profile_id] = stale_provider_patch;
	Dictionary stale_patch;
	stale_patch["providers"] = stale_providers_patch;
	REQUIRE(bool(config->patch_config(stale_patch, "runtime").get("success", false)));
	CHECK(config->get_provider_config(profile_id).has("max_provider_turns"));

	Dictionary update;
	update["api_key"] = "edited-key";
	update["model"] = "gpt-5.4-mini";
	update["max_output_tokens"] = 1024;
	update["timeout_msec"] = 30000;
	const Dictionary updated = adapter->update_model_profile(profile_id, update, "runtime");
	REQUIRE(bool(updated.get("success", false)));
	stored_profile = adapter->get_model_profile(profile_id);
	CHECK(String(stored_profile.get("model", String())) == "gpt-5.4-mini");
	CHECK(String(stored_profile.get("api_key", String())) == "edited-key");
	CHECK(int(stored_profile.get("max_output_tokens", 0)) == 1024);
	CHECK(int(stored_profile.get("timeout_msec", 0)) == 30000);
	const Dictionary updated_provider_config = config->get_provider_config(profile_id);
	CHECK(int(updated_provider_config.get("max_output_tokens", 0)) == 1024);
	CHECK(int(updated_provider_config.get("timeout_msec", 0)) == 30000);
	CHECK_FALSE(updated_provider_config.has("timeout_seconds"));
	CHECK_FALSE(updated_provider_config.has("max_provider_turns"));
	CHECK_FALSE(updated_provider_config.has("max_tool_calls"));
	CHECK_FALSE(updated_provider_config.has("max_context_chars"));

	const Dictionary removed = adapter->remove_model_profile(profile_id, "runtime");
	REQUIRE(bool(removed.get("success", false)));
	CHECK(adapter->list_model_profiles().is_empty());
	const Dictionary config_after_remove = config->get_config();
	const Dictionary providers_after_remove = config_after_remove.get("providers", Dictionary());
	CHECK_FALSE(providers_after_remove.has(profile_id));
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Config adapter preserves model profile enabled state") {
	Ref<AIConfigService> config;
	config.instantiate();

	Dictionary override_config;
	override_config["default_provider"] = "fake";
	override_config["default_model"] = "fake-model";
	override_config["providers"] = Dictionary();
	config->set_runtime_override(override_config);

	Ref<AIAgentV1UIConfigAdapter> adapter;
	adapter.instantiate();
	adapter->set_config_service(config);

	Dictionary profile;
	profile["provider_id"] = "openai";
	profile["model"] = "gpt-5.4";
	profile["display_name"] = "Disabled OpenAI";
	profile["enabled"] = false;

	const Dictionary added = adapter->add_model_profile(profile, "runtime");
	REQUIRE(bool(added.get("success", false)));
	const String profile_id = added.get("id", String());
	REQUIRE_FALSE(profile_id.is_empty());

	const Dictionary stored = adapter->get_model_profile(profile_id);
	REQUIRE_FALSE(stored.is_empty());
	CHECK_FALSE(bool(stored.get("enabled", true)));
	CHECK(adapter->list_model_profiles(true).is_empty());
	CHECK(adapter->list_model_profiles(false).size() == 1);

	Dictionary enable_patch;
	enable_patch["enabled"] = true;
	const Dictionary updated = adapter->update_model_profile(profile_id, enable_patch, "runtime");
	REQUIRE(bool(updated.get("success", false)));
	CHECK(bool(adapter->get_model_profile(profile_id).get("enabled", false)));
	CHECK(adapter->list_model_profiles(true).size() == 1);
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Config adapter emits model profile DTOs on model changes") {
	Ref<AIConfigService> config;
	config.instantiate();

	Dictionary override_config;
	override_config["default_provider"] = "fake";
	override_config["default_model"] = "fake-model";
	override_config["providers"] = Dictionary();
	config->set_runtime_override(override_config);

	Ref<AIAgentV1UIConfigAdapter> adapter;
	adapter.instantiate();
	adapter->set_config_service(config);

	Ref<AgentV1UIConfigSignalCollector> collector;
	collector.instantiate();
	adapter->connect("models_changed", callable_mp(collector.ptr(), &AgentV1UIConfigSignalCollector::on_models_changed));

	Dictionary profile;
	profile["provider_id"] = "openai";
	profile["model"] = "gpt-5.4";
	profile["display_name"] = "OpenAI Profile DTO";

	const Dictionary added = adapter->add_model_profile(profile, "runtime");
	REQUIRE(bool(added.get("success", false)));
	const String profile_id = added.get("id", String());
	REQUIRE_FALSE(profile_id.is_empty());

	REQUIRE(collector->model_snapshots.size() >= 1);
	const Array emitted_models = collector->model_snapshots[collector->model_snapshots.size() - 1];
	REQUIRE(emitted_models.size() == 1);
	const Dictionary emitted = emitted_models[0];
	CHECK(String(emitted.get("id", String())) == profile_id);
	CHECK(String(emitted.get("provider_key", String())) == profile_id);
	CHECK(String(emitted.get("provider_id", String())) == "openai");
	CHECK(String(emitted.get("model", String())) == "gpt-5.4");
	CHECK_FALSE(emitted.has("metadata"));
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Bridge owns shared backend services for UI adapters") {
	AIAgentV1UIBridge::clear_singleton_for_test();

	Ref<AIAgentV1UIBridge> bridge = AIAgentV1UIBridge::get_singleton();
	REQUIRE(bridge.is_valid());

	Ref<AIAgentV1UIAdapter> conversation = bridge->get_conversation_adapter();
	Ref<AIAgentV1UIConfigAdapter> config_adapter = bridge->get_config_adapter();
	REQUIRE(conversation.is_valid());
	REQUIRE(config_adapter.is_valid());

	Ref<AISessionService> session_service = bridge->get_session_service();
	Ref<AIConfigService> config_service = bridge->get_config_service();
	Ref<AIAgentService> agent_service = bridge->get_agent_service();
	Ref<AIV1SkillService> skill_service = bridge->get_skill_service();
	REQUIRE(session_service.is_valid());
	REQUIRE(config_service.is_valid());
	REQUIRE(agent_service.is_valid());
	REQUIRE(skill_service.is_valid());

	CHECK(conversation->get_session_service() == session_service);
	CHECK(config_adapter->get_config_service() == config_service);
	CHECK(config_adapter->get_agent_service() == agent_service);
	CHECK(config_adapter->get_skill_service() == skill_service);
	CHECK(session_service->get_config_service() == config_service);
	CHECK(session_service->get_agent_service() == agent_service);
	CHECK(session_service->get_skill_service() == skill_service);
	CHECK(bridge == AIAgentV1UIBridge::get_singleton());

	AIAgentV1UIBridge::clear_singleton_for_test();
}

} // namespace TestAgentV1
