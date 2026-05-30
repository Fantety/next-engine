/**************************************************************************/
/*  test_ai_agent_tools.cpp                                               */
/**************************************************************************/

#include "tests/test_macros.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "editor/ai_component/agent/ai_agent_profile.h"
#include "editor/ai_component/agent/ai_main_agent.h"
#include "editor/ai_component/agent/ai_mcp_tool_discovery.h"
#include "editor/ai_component/providers/ai_mcp_client.h"
#include "editor/ai_component/providers/ai_mcp_http_client.h"
#include "editor/ai_component/providers/ai_mcp_protocol.h"
#include "editor/ai_component/providers/ai_mcp_settings.h"
#include "editor/ai_component/providers/ai_mcp_stdio_client.h"
#include "editor/ai_component/providers/ai_mcp_status_tracker.h"
#include "editor/ai_component/providers/ai_openai_compatible_codec.h"
#include "editor/ai_component/planning/ai_manage_plan_tool.h"
#include "editor/ai_component/planning/ai_plan_manager.h"
#include "editor/ai_component/review/ai_change_set_store.h"
#include "editor/ai_component/review/ai_diff_service.h"
#include "editor/ai_component/rules/ai_rule_settings.h"
#include "editor/ai_component/rules/ai_rules_context_provider.h"
#include "editor/ai_component/skills/ai_activate_skill_tool.h"
#include "editor/ai_component/skills/ai_skill_context_provider.h"
#include "editor/ai_component/skills/ai_skill_settings.h"
#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/ai_tool_call.h"
#include "editor/ai_component/tools/ai_tool_permission.h"
#include "editor/ai_component/tools/ai_tool_registry.h"
#include "editor/ai_component/tools/ai_mcp_tool.h"
#include "editor/ai_component/tools/editor/ai_get_editor_context_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_add_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_create_scene_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_delete_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_list_properties_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_move_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_open_scene_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_rename_node_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_save_current_scene_tool.h"
#include "editor/ai_component/tools/editor/ai_scene_set_property_tool.h"
#include "editor/ai_component/tools/editor/ai_script_bind_to_node_tool.h"
#include "editor/ai_component/tools/editor/ai_script_create_tool.h"
#include "editor/ai_component/tools/editor/ai_script_delete_tool.h"
#include "editor/ai_component/tools/editor/ai_script_inspect_tool.h"
#include "editor/ai_component/tools/editor/ai_script_patch_function_tool.h"
#include "editor/ai_component/tools/editor/ai_script_unbind_from_node_tool.h"
#include "editor/ai_component/tools/editor/ai_script_write_tool.h"
#include "editor/ai_component/tools/editor/ai_shader_apply_to_node_tool.h"
#include "editor/ai_component/tools/editor/ai_shader_create_tool.h"
#include "editor/ai_component/tools/editor/ai_shader_delete_tool.h"
#include "editor/ai_component/tools/editor/ai_shader_edit_tool.h"
#include "editor/ai_component/tools/project/ai_create_folder_tool.h"
#include "editor/ai_component/tools/project/ai_list_project_tool.h"
#include "editor/ai_component/tools/project/ai_read_file_tool.h"
#include "editor/ai_component/tools/project/ai_search_project_tool.h"
#include "tests/test_utils.h"

TEST_FORCE_LINK(test_ai_agent_tools);

namespace TestAIAgentTools {

class EchoAITool : public AITool {
	GDCLASS(EchoAITool, AITool);

public:
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
		AIToolResult result;
		result.content = String(p_arguments.get("value", ""));
		return result;
	}
};

TEST_CASE("[Editor][AI] Tool registry exposes OpenAI-compatible schemas") {
	Ref<AIToolRegistry> registry;
	registry.instantiate();

	Ref<EchoAITool> tool;
	tool.instantiate();

	CHECK(registry->register_tool(tool));
	CHECK_FALSE(registry->register_tool(tool));
	CHECK(registry->has_tool("test.echo"));
	CHECK(registry->get_tool("test.echo") == tool);

	Array schemas = registry->get_tool_schemas();
	REQUIRE(schemas.size() == 1);
	Dictionary schema = schemas[0];
	CHECK(String(schema["type"]) == "function");

	Dictionary function_schema = schema["function"];
	CHECK(String(function_schema["name"]) == "test.echo");
	CHECK(String(function_schema["description"]) == "Echoes a test value.");

	Dictionary parameters = function_schema["parameters"];
	CHECK(String(parameters["type"]) == "object");
}

TEST_CASE("[Editor][AI] Tool registry exposes registered tool names") {
	Ref<AIToolRegistry> registry;
	registry.instantiate();

	Ref<EchoAITool> tool;
	tool.instantiate();
	CHECK(registry->register_tool(tool));

	Vector<String> names = registry->get_tool_names();
	REQUIRE(names.size() == 1);
	CHECK(names[0] == "test.echo");

	registry->clear();
	CHECK(registry->get_tool_names().is_empty());
	CHECK_FALSE(registry->has_tool("test.echo"));
}

TEST_CASE("[Editor][AI] Tool registry stores permissions and exposes available schemas") {
	Ref<AIToolRegistry> registry;
	registry.instantiate();

	Ref<EchoAITool> allowed_tool;
	allowed_tool.instantiate();
	CHECK(registry->register_tool(allowed_tool, AI_TOOL_PERMISSION_ASK, "Needs review."));

	CHECK(registry->get_tool_permission("test.echo") == AI_TOOL_PERMISSION_ASK);
	CHECK(registry->get_tool_permission_reason("test.echo") == "Needs review.");
	CHECK(registry->set_tool_permission("test.echo", AI_TOOL_PERMISSION_DENY, "Disabled for this agent."));
	CHECK(registry->get_tool_permission("test.echo") == AI_TOOL_PERMISSION_DENY);

	CHECK(registry->get_tool_schemas().size() == 1);
	CHECK(registry->get_available_tool_schemas().is_empty());

	CHECK(registry->set_tool_permission("test.echo", AI_TOOL_PERMISSION_ALLOW));
	Array schemas = registry->get_available_tool_schemas();
	REQUIRE(schemas.size() == 1);
	Dictionary schema = schemas[0];
	Dictionary function_schema = schema["function"];
	CHECK(String(function_schema["name"]) == "test.echo");
	CHECK(registry->get_tool_permission("missing.tool") == AI_TOOL_PERMISSION_DENY);
}

TEST_CASE("[Editor][AI] Skill settings manage prompt context skills") {
	Array original_skills = AISkillSettings::get_skill_storage_for_test();
	AISkillSettings::clear_skills_for_test();

	const String skill_id = AISkillSettings::add_skill("TDD", "Use when implementing changes.", "Write tests first.", true);
	CHECK(!skill_id.is_empty());

	Vector<AISkillConfig> skills = AISkillSettings::get_skills(false);
	REQUIRE(skills.size() == 1);
	CHECK(skills[0].id == skill_id);
	CHECK(skills[0].display_name == "TDD");
	CHECK(skills[0].description == "Use when implementing changes.");
	CHECK(skills[0].content == "Write tests first.");
	CHECK(skills[0].kind == "prompt_context");
	CHECK(skills[0].enabled);

	CHECK(AISkillSettings::update_skill(skill_id, "TDD Updated", "Use when changing behavior.", "Updated body.", false));
	AISkillConfig updated = AISkillSettings::get_skill(skill_id);
	CHECK(updated.display_name == "TDD Updated");
	CHECK(updated.description == "Use when changing behavior.");
	CHECK(updated.content == "Updated body.");
	CHECK_FALSE(updated.enabled);
	CHECK(AISkillSettings::get_skills(true).is_empty());

	CHECK(AISkillSettings::remove_skill(skill_id));
	CHECK(AISkillSettings::get_skills(false).is_empty());

	AISkillSettings::set_skill_storage_for_test(original_skills);
}

TEST_CASE("[Editor][AI] Skill settings import local SKILL.md folders") {
	Array original_skills = AISkillSettings::get_skill_storage_for_test();
	AISkillSettings::clear_skills_for_test();

	const String skill_dir = TestUtils::get_temp_path("ai_skill_import");
	DirAccess::make_dir_recursive_absolute(skill_dir);

	Ref<FileAccess> skill_file = FileAccess::open(skill_dir.path_join("SKILL.md"), FileAccess::WRITE);
	REQUIRE(skill_file.is_valid());
	skill_file->store_string("---\nname: imported-skill\ndescription: Use when importing local skills.\n---\n\n# Imported Skill\n\nFollow local instructions.");
	skill_file.unref();

	String error;
	String skill_id;
	CHECK(AISkillSettings::import_skill_folder(skill_dir, error, &skill_id));
	CHECK(error.is_empty());
	CHECK_FALSE(skill_id.is_empty());

	AISkillConfig imported = AISkillSettings::get_skill(skill_id);
	CHECK(imported.display_name == "imported-skill");
	CHECK(imported.description == "Use when importing local skills.");
	CHECK(imported.content.contains("# Imported Skill"));
	CHECK(imported.content.contains("Follow local instructions."));
	CHECK(imported.kind == "prompt_context");
	CHECK(imported.enabled);

	const String fallback_dir = TestUtils::get_temp_path("fallback_skill");
	DirAccess::make_dir_recursive_absolute(fallback_dir);
	Ref<FileAccess> fallback_file = FileAccess::open(fallback_dir.path_join("SKILL.md"), FileAccess::WRITE);
	REQUIRE(fallback_file.is_valid());
	fallback_file->store_string("# Fallback Skill\n\nNo front matter.");
	fallback_file.unref();

	String fallback_error;
	String fallback_id;
	CHECK(AISkillSettings::import_skill_folder(fallback_dir + "/", fallback_error, &fallback_id));
	CHECK(fallback_error.is_empty());
	CHECK(AISkillSettings::get_skill(fallback_id).display_name == "fallback_skill");

	String missing_error;
	CHECK_FALSE(AISkillSettings::import_skill_folder(skill_dir.path_join("missing"), missing_error));
	CHECK(missing_error.contains("SKILL.md"));

	DirAccess::remove_absolute(skill_dir.path_join("SKILL.md"));
	DirAccess::remove_absolute(skill_dir);
	DirAccess::remove_absolute(fallback_dir.path_join("SKILL.md"));
	DirAccess::remove_absolute(fallback_dir);
	AISkillSettings::set_skill_storage_for_test(original_skills);
}

TEST_CASE("[Editor][AI] Skill context provider exposes only enabled skill metadata") {
	Array original_skills = AISkillSettings::get_skill_storage_for_test();
	AISkillSettings::clear_skills_for_test();

	AISkillSettings::add_skill("TDD", "Use when implementing changes.", "SECRET FULL BODY", true);
	AISkillSettings::add_skill("Disabled", "Hidden trigger.", "Hidden body.", false);

	Ref<AISkillIndexContextProvider> provider;
	provider.instantiate();
	Array context = provider->collect_context();
	REQUIRE(context.size() == 1);

	Dictionary doc = context[0];
	CHECK(String(doc["title"]) == "Available Agent Skills");
	String content = doc["content"];
	CHECK(content.contains("TDD"));
	CHECK(content.contains("Use when implementing changes."));
	CHECK(content.contains("agent.activate_skill"));
	CHECK(content.contains("prompt/context"));
	CHECK_FALSE(content.contains("SECRET FULL BODY"));
	CHECK_FALSE(content.contains("Disabled"));

	AISkillSettings::set_skill_storage_for_test(original_skills);
}

TEST_CASE("[Editor][AI] Activate skill tool returns enabled prompt context content") {
	Array original_skills = AISkillSettings::get_skill_storage_for_test();
	AISkillSettings::clear_skills_for_test();

	const String skill_id = AISkillSettings::add_skill("TDD", "Use when implementing changes.", "Full skill body.", true);
	Ref<AIActivateSkillTool> tool;
	tool.instantiate();

	Dictionary args;
	args["skill_id"] = skill_id;
	AIToolResult result = tool->execute(args);
	CHECK_FALSE(result.is_error());
	CHECK(result.content.contains("Full skill body."));
	CHECK(String(result.metadata["skill_id"]) == skill_id);
	CHECK(String(result.metadata["skill_kind"]) == "prompt_context");
	CHECK(String(result.metadata["tool_origin"]) == "agent_skill");

	AISkillSettings::set_skill_storage_for_test(original_skills);
}

TEST_CASE("[Editor][AI] Activate skill tool rejects missing disabled and unsupported skills") {
	Array original_skills = AISkillSettings::get_skill_storage_for_test();
	AISkillSettings::clear_skills_for_test();

	Dictionary disabled;
	disabled["id"] = "skill:disabled";
	disabled["display_name"] = "Disabled";
	disabled["description"] = "Hidden trigger.";
	disabled["content"] = "Hidden body.";
	disabled["kind"] = "prompt_context";
	disabled["enabled"] = false;

	Dictionary unsupported;
	unsupported["id"] = "skill:exec";
	unsupported["display_name"] = "Executable";
	unsupported["description"] = "Should not run.";
	unsupported["content"] = "run me";
	unsupported["kind"] = "executable";
	unsupported["enabled"] = true;

	Array storage;
	storage.push_back(disabled);
	storage.push_back(unsupported);
	AISkillSettings::set_skill_storage_for_test(storage);

	Ref<AIActivateSkillTool> tool;
	tool.instantiate();

	Dictionary missing_args;
	missing_args["skill_id"] = "missing";
	CHECK(tool->execute(missing_args).is_error());

	Dictionary disabled_args;
	disabled_args["skill_id"] = "skill:disabled";
	CHECK(tool->execute(disabled_args).is_error());

	Dictionary unsupported_args;
	unsupported_args["skill_id"] = "skill:exec";
	CHECK(tool->execute(unsupported_args).is_error());

	AISkillSettings::set_skill_storage_for_test(original_skills);
}

TEST_CASE("[Editor][AI] MCP protocol maps tools to provider-safe names") {
	const String list_response = "{\"jsonrpc\":\"2.0\",\"id\":7,\"result\":{\"tools\":[{\"name\":\"read.file\",\"description\":\"Read a file from the MCP server.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}}]}}";

	Dictionary list_result;
	String error;
	CHECK(AIMCPProtocol::parse_response(list_response, 7, list_result, error));
	CHECK(error.is_empty());

	Dictionary ignored_result;
	CHECK(AIMCPProtocol::parse_response_line("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/message\",\"params\":{\"level\":\"info\",\"data\":\"ready\"}}", 7, ignored_result, error) == AI_MCP_RESPONSE_SKIPPED);
	CHECK(AIMCPProtocol::parse_response_line("{\"jsonrpc\":\"2.0\",\"id\":99,\"result\":{}}", 7, ignored_result, error) == AI_MCP_RESPONSE_SKIPPED);

	Vector<AIMCPToolDescriptor> tools;
	CHECK(AIMCPProtocol::parse_tools_list_result(list_result, "filesystem server", "Filesystem", tools, error));
	REQUIRE(tools.size() == 1);
	CHECK(tools[0].name == "read.file");
	CHECK(AIMCPProtocol::make_agent_tool_name(tools[0].server_id, tools[0].name) == "mcp_filesystem_server_read_file");

	CHECK(String(tools[0].input_schema["type"]) == "object");
}

TEST_CASE("[Editor][AI] MCP tool wraps descriptors without executing the server") {
	AIMCPServerConfig server;
	server.id = "filesystem";
	server.display_name = "Filesystem";
	server.transport = "stdio";
	server.command = "npx";

	AIMCPToolDescriptor descriptor;
	descriptor.server_id = server.id;
	descriptor.server_name = server.display_name;
	descriptor.name = "read_file";
	descriptor.description = "Read a file.";
	descriptor.input_schema["type"] = "object";

	Ref<AIMCPTool> tool;
	tool.instantiate();
	tool->setup(server, descriptor);

	CHECK(tool->get_name() == "mcp_filesystem_read_file");
	CHECK(tool->get_description().contains("MCP Server: Filesystem"));
	CHECK(String(tool->get_parameters_schema()["type"]) == "object");

	CHECK(AIToolPermissionPolicy::evaluate(AI_TOOL_PERMISSION_ASK, tool->get_name()).permission == AI_TOOL_PERMISSION_ASK);
}

TEST_CASE("[Editor][AI] MCP discovery keeps agent tool names unique") {
	AIMCPServerConfig server;
	server.id = "filesystem";
	server.display_name = "Filesystem";
	server.transport = "stdio";
	server.command = "npx";

	AIMCPToolDescriptor first_descriptor;
	first_descriptor.server_id = server.id;
	first_descriptor.server_name = server.display_name;
	first_descriptor.name = "read.file";
	first_descriptor.description = "Read a file.";
	first_descriptor.input_schema["type"] = "object";

	AIMCPToolDescriptor duplicate_descriptor = first_descriptor;
	duplicate_descriptor.display_name = "Read File Duplicate";

	AIMCPToolDescriptor second_descriptor = first_descriptor;
	second_descriptor.name = "write_file";

	AIMCPServerDiscoveryResult result;
	result.server = server;
	result.status = "ok";
	result.tools.push_back(first_descriptor);
	result.tools.push_back(duplicate_descriptor);
	result.tools.push_back(second_descriptor);

	Vector<AIMCPServerDiscoveryResult> results;
	results.push_back(result);

	Vector<AIMCPDiscoveredTool> tools = AIMCPToolDiscovery::build_discovered_tools(results);
	REQUIRE(tools.size() == 2);
	CHECK(AIMCPProtocol::make_agent_tool_name(tools[0].server.id, tools[0].descriptor.name) == "mcp_filesystem_read_file");
	CHECK(AIMCPProtocol::make_agent_tool_name(tools[1].server.id, tools[1].descriptor.name) == "mcp_filesystem_write_file");
}

TEST_CASE("[Editor][AI] Tool registry rejects duplicate MCP agent tool names") {
	AIMCPServerConfig server;
	server.id = "filesystem";
	server.display_name = "Filesystem";
	server.transport = "stdio";
	server.command = "npx";

	AIMCPToolDescriptor descriptor;
	descriptor.server_id = server.id;
	descriptor.server_name = server.display_name;
	descriptor.name = "read_file";
	descriptor.description = "Read a file.";
	descriptor.input_schema["type"] = "object";

	Ref<AIMCPTool> first_tool;
	first_tool.instantiate();
	first_tool->setup(server, descriptor);

	Ref<AIMCPTool> duplicate_tool;
	duplicate_tool.instantiate();
	duplicate_tool->setup(server, descriptor);

	Ref<AIToolRegistry> registry;
	registry.instantiate();
	CHECK(registry->register_tool(first_tool));
	CHECK_FALSE(registry->register_tool(duplicate_tool));
	CHECK(registry->get_tool_schemas().size() == 1);
}

TEST_CASE("[Editor][AI] MCP client factory selects transport implementations") {
	AIMCPServerConfig stdio_server;
	stdio_server.transport = "stdio";
	stdio_server.command = "npx";
	Ref<AIMCPClient> stdio_client = AIMCPClientFactory::create_client(stdio_server);
	REQUIRE(stdio_client.is_valid());
	CHECK(Object::cast_to<AIMCPStdioClient>(*stdio_client) != nullptr);

	AIMCPServerConfig http_server;
	http_server.transport = "streamable_http";
	http_server.url = "https://mcp.example.test/mcp";
	Ref<AIMCPClient> http_client = AIMCPClientFactory::create_client(http_server);
	REQUIRE(http_client.is_valid());
	CHECK(Object::cast_to<AIMCPHTTPClient>(*http_client) != nullptr);

	AIMCPServerConfig sse_server;
	sse_server.transport = "sse";
	sse_server.url = "https://mcp.example.test/sse";
	Ref<AIMCPClient> sse_client = AIMCPClientFactory::create_client(sse_server);
	REQUIRE(sse_client.is_valid());
	CHECK(Object::cast_to<AIMCPHTTPClient>(*sse_client) != nullptr);
}

TEST_CASE("[Editor][AI] MCP HTTP client maps wildcard bind hosts to loopback connection hosts") {
	CHECK(AIMCPHTTPClient::normalize_connection_host_for_test("0.0.0.0") == "127.0.0.1");
	CHECK(AIMCPHTTPClient::normalize_connection_host_for_test("::") == "::1");
	CHECK(AIMCPHTTPClient::normalize_connection_host_for_test("localhost") == "localhost");
	CHECK(AIMCPHTTPClient::normalize_connection_host_for_test("mcp.example.test") == "mcp.example.test");
}

TEST_CASE("[Editor][AI] MCP HTTP client extracts SSE data events") {
	Vector<String> events = AIMCPHTTPClient::extract_sse_data_events_for_test("event: message\ndata: {\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{}}\n\n: keepalive\n\ndata: {\"jsonrpc\":\"2.0\",\"id\":2,\"result\":{}}\n\n");
	REQUIRE(events.size() == 2);
	CHECK(events[0] == "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{}}");
	CHECK(events[1] == "{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":{}}");
}

TEST_CASE("[Editor][AI] MCP HTTP client consumes only complete SSE events") {
	PackedByteArray buffer = String("data: {\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{}}\n\ndata: {\"jsonrpc\":\"2.0\"").to_utf8_buffer();
	Vector<String> events = AIMCPHTTPClient::consume_sse_data_events_for_test(buffer);
	REQUIRE(events.size() == 1);
	CHECK(events[0] == "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{}}");

	String remaining = String::utf8(reinterpret_cast<const char *>(buffer.ptr()), buffer.size());
	CHECK(remaining == "data: {\"jsonrpc\":\"2.0\"");

	PackedByteArray suffix = String(",\"id\":2,\"result\":{}}\n\n").to_utf8_buffer();
	buffer.append_array(suffix);
	events = AIMCPHTTPClient::consume_sse_data_events_for_test(buffer);
	REQUIRE(events.size() == 1);
	CHECK(events[0] == "{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":{}}");
	CHECK(buffer.is_empty());
}

TEST_CASE("[Editor][AI] MCP status tracker summarizes server discovery results") {
	AIMCPServerConfig ok_server;
	ok_server.id = "mcp:ok";
	ok_server.display_name = "Filesystem";
	ok_server.transport = "stdio";
	ok_server.command = "npx";
	ok_server.enabled = true;

	AIMCPServerConfig failed_server;
	failed_server.id = "mcp:failed";
	failed_server.display_name = "Remote";
	failed_server.transport = "streamable_http";
	failed_server.url = "https://mcp.example.test/mcp";
	failed_server.enabled = true;

	AIMCPServerConfig disabled_server;
	disabled_server.id = "mcp:disabled";
	disabled_server.display_name = "Disabled";
	disabled_server.transport = "sse";
	disabled_server.url = "https://mcp.example.test/sse";
	disabled_server.enabled = false;

	Vector<AIMCPServerConfig> servers;
	servers.push_back(ok_server);
	servers.push_back(failed_server);
	servers.push_back(disabled_server);

	Ref<AIMCPStatusTracker> tracker;
	tracker.instantiate();
	tracker->begin_refresh(servers);
	tracker->record_success(ok_server, 2);
	tracker->record_failure(failed_server, "connection refused");

	Array statuses = tracker->get_statuses();
	REQUIRE(statuses.size() == 3);

	Dictionary ok_status = statuses[0];
	CHECK(String(ok_status["status"]) == "ok");
	CHECK((int)ok_status["tool_count"] == 2);
	CHECK(String(ok_status["endpoint"]) == "npx");

	Dictionary failed_status = statuses[1];
	CHECK(String(failed_status["status"]) == "failed");
	CHECK(String(failed_status["error"]) == "connection refused");
	CHECK(String(failed_status["endpoint"]) == "https://mcp.example.test/mcp");

	Dictionary disabled_status = statuses[2];
	CHECK(String(disabled_status["status"]) == "disabled");
	CHECK_FALSE(bool(disabled_status["enabled"]));

	Dictionary summary = tracker->get_summary();
	CHECK((int)summary["total"] == 3);
	CHECK((int)summary["enabled"] == 2);
	CHECK((int)summary["ok"] == 1);
	CHECK((int)summary["failed"] == 1);
	CHECK((int)summary["disabled"] == 1);
	CHECK((int)summary["tool_count"] == 2);
	CHECK(tracker->has_failures());
}

TEST_CASE("[Editor][AI] MCP status tracker records invalid enabled servers as failures") {
	AIMCPServerConfig invalid_server;
	invalid_server.id = "mcp:invalid";
	invalid_server.display_name = "Invalid Remote";
	invalid_server.transport = "streamable_http";
	invalid_server.enabled = true;

	Vector<AIMCPServerConfig> servers;
	servers.push_back(invalid_server);

	Ref<AIMCPStatusTracker> tracker;
	tracker.instantiate();
	tracker->begin_refresh(servers);

	String error;
	CHECK_FALSE(AIMCPSettings::is_server_config_usable(invalid_server, &error));
	tracker->record_failure(invalid_server, error);

	Array statuses = tracker->get_statuses();
	REQUIRE(statuses.size() == 1);
	Dictionary status = statuses[0];
	CHECK(String(status["status"]) == "failed");
	CHECK(String(status["error"]).contains("URL"));

	Dictionary summary = tracker->get_summary();
	CHECK((int)summary["failed"] == 1);
	CHECK(tracker->has_failures());
}

TEST_CASE("[Editor][AI] Tool calls serialize stable execution state") {
	AIToolCall call;
	call.id = "call_123";
	call.tool_name = "project.read_file";
	call.status = AI_TOOL_CALL_STATUS_RUNNING;
	call.created_at = 123;
	call.updated_at = 456;
	call.arguments["path"] = "res://player.gd";

	Dictionary dict = call.to_dict();
	AIToolCall restored = AIToolCall::from_dict(dict);

	CHECK(restored.id == "call_123");
	CHECK(restored.tool_name == "project.read_file");
	CHECK(restored.status == AI_TOOL_CALL_STATUS_RUNNING);
	CHECK(restored.created_at == 123);
	CHECK(restored.updated_at == 456);
	CHECK(String(restored.arguments["path"]) == "res://player.gd");
}

TEST_CASE("[Editor][AI] Agent profiles only describe agent identity") {
	AIAgentProfile plan = AIAgentProfile::get_plan_profile();
	AIAgentProfile build = AIAgentProfile::get_build_profile();
	AIAgentProfile review = AIAgentProfile::get_review_profile();
	AIAgentProfile write = AIAgentProfile::get_write_profile();

	CHECK(plan.id == "plan");
	CHECK(plan.display_name == "Plan");
	CHECK(build.id == "build");
	CHECK(build.display_name == "Build");
	CHECK(review.id == "review");
	CHECK(review.display_name == "Review");
	CHECK(write.id == "write");
	CHECK(write.display_name == "Write");
}

TEST_CASE("[Editor][AI] Main agent registers tool permissions on the agent") {
	Ref<AIMainAgent> agent;
	agent.instantiate();
	Ref<AIToolRegistry> registry = agent->get_tool_registry();
	REQUIRE(registry.is_valid());

	CHECK(registry->get_tool_permission("project.list_tree") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("project.read_file") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("project.search_text") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("agent.activate_skill") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("agent.manage_plan") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("project.create_folder") == AI_TOOL_PERMISSION_DENY);
	CHECK(registry->get_tool_permission("editor.get_context") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("scene.create_scene") == AI_TOOL_PERMISSION_DENY);
	CHECK(registry->get_tool_permission("scene.list_properties") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("script.inspect") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("script.write") == AI_TOOL_PERMISSION_DENY);
	CHECK(registry->get_tool_permission("script.delete") == AI_TOOL_PERMISSION_DENY);
	CHECK(registry->get_tool_permission("shader.create") == AI_TOOL_PERMISSION_DENY);
	CHECK(registry->get_tool_permission("shader.edit") == AI_TOOL_PERMISSION_DENY);
	CHECK(registry->get_tool_permission("shader.delete") == AI_TOOL_PERMISSION_DENY);
	CHECK(registry->get_tool_permission("shader.apply_to_node") == AI_TOOL_PERMISSION_DENY);
	CHECK(registry->get_tool_permission("unknown.tool") == AI_TOOL_PERMISSION_DENY);

	agent->set_agent_profile_id("build");
	CHECK(registry->get_tool_permission("project.list_tree") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("script.write") == AI_TOOL_PERMISSION_DENY);

	agent->set_agent_profile_id("review");
	CHECK(registry->get_tool_permission("project.create_folder") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("script.write") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("script.delete") == AI_TOOL_PERMISSION_ASK);
	CHECK(registry->get_tool_permission("shader.create") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("shader.edit") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("shader.delete") == AI_TOOL_PERMISSION_ASK);
	CHECK(registry->get_tool_permission("shader.apply_to_node") == AI_TOOL_PERMISSION_ALLOW);

	agent->set_agent_profile_id("write");
	CHECK(registry->get_tool_permission("project.create_folder") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("scene.create_scene") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("scene.add_node") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("scene.delete_node") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("scene.rename_node") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("scene.move_node") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("scene.set_property") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("scene.save_current_scene") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("scene.open_scene") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("script.create") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("script.write") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("script.patch_function") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("script.bind_to_node") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("script.unbind_from_node") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("script.delete") == AI_TOOL_PERMISSION_ASK);
	CHECK(registry->get_tool_permission("shader.create") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("shader.edit") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(registry->get_tool_permission("shader.delete") == AI_TOOL_PERMISSION_ASK);
	CHECK(registry->get_tool_permission("shader.apply_to_node") == AI_TOOL_PERMISSION_ALLOW);

	agent->set_profile(AIAgentProfile::get_plan_profile());
	CHECK(registry->get_tool_permission("script.write") == AI_TOOL_PERMISSION_DENY);
	CHECK(registry->get_tool_permission("script.delete") == AI_TOOL_PERMISSION_DENY);
	CHECK(registry->get_tool_permission("shader.create") == AI_TOOL_PERMISSION_DENY);
	CHECK(registry->get_tool_permission("shader.edit") == AI_TOOL_PERMISSION_DENY);
	CHECK(registry->get_tool_permission("shader.delete") == AI_TOOL_PERMISSION_DENY);

	CHECK_FALSE(registry->has_tool("project.write_file"));
	CHECK(registry->get_tool_permission("project.write_file") == AI_TOOL_PERMISSION_DENY);

	CHECK(AIToolPermissionPolicy::permission_to_string(AI_TOOL_PERMISSION_ALLOW) == "allow");
	CHECK(AIToolPermissionPolicy::permission_to_string(AI_TOOL_PERMISSION_ASK) == "ask");
	CHECK(AIToolPermissionPolicy::permission_to_string(AI_TOOL_PERMISSION_DENY) == "deny");
	CHECK(AIToolPermissionPolicy::string_to_permission("allow") == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::string_to_permission("ask") == AI_TOOL_PERMISSION_ASK);
	CHECK(AIToolPermissionPolicy::string_to_permission("deny") == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(AI_TOOL_PERMISSION_ASK, "script.delete").permission == AI_TOOL_PERMISSION_ASK);
}

TEST_CASE("[Editor][AI] Plan manager keeps one active plan and archives completed work") {
	Ref<AIPlanManager> manager = AIPlanManager::get_singleton();
	manager->clear_for_test();

	Array tasks;
	tasks.push_back("Inspect context");
	tasks.push_back("Implement feature");

	String error;
	CHECK(manager->create_plan("Planning support", tasks, error));
	CHECK(error.is_empty());
	CHECK(manager->has_active_plan());

	Dictionary active_plan = manager->get_active_plan();
	CHECK(String(active_plan["title"]) == "Planning support");
	Array active_tasks = active_plan["tasks"];
	REQUIRE(active_tasks.size() == 2);

	Dictionary first_task = active_tasks[0];
	Dictionary second_task = active_tasks[1];
	CHECK(String(first_task["id"]) == "task:1");
	CHECK(String(first_task["status"]) == "pending");

	CHECK_FALSE(manager->create_plan("Second plan", tasks, error));
	CHECK(error.contains("active plan"));

	CHECK(manager->update_task(first_task["id"], "in_progress", error));
	active_plan = manager->get_active_plan();
	active_tasks = active_plan["tasks"];
	first_task = active_tasks[0];
	CHECK(String(first_task["status"]) == "in_progress");
	CHECK(manager->has_active_plan());

	CHECK_FALSE(manager->update_task(first_task["id"], "blocked", error));
	CHECK(error.contains("status"));

	CHECK(manager->update_task(first_task["id"], "completed", error));
	CHECK(manager->has_active_plan());

	CHECK(manager->update_task(second_task["id"], "completed", error));
	CHECK_FALSE(manager->has_active_plan());
	CHECK(manager->get_active_plan().is_empty());

	Dictionary archived_plan = manager->get_archived_plan();
	CHECK(String(archived_plan["title"]) == "Planning support");
	Array archived_tasks = archived_plan["tasks"];
	REQUIRE(archived_tasks.size() == 2);
	Dictionary archived_first_task = archived_tasks[0];
	Dictionary archived_second_task = archived_tasks[1];
	CHECK(String(archived_first_task["status"]) == "completed");
	CHECK(String(archived_second_task["status"]) == "completed");

	manager->clear_for_test();
}

TEST_CASE("[Editor][AI] Manage plan tool exposes plan state metadata") {
	Ref<AIPlanManager> manager = AIPlanManager::get_singleton();
	manager->clear_for_test();

	Ref<AIManagePlanTool> tool;
	tool.instantiate();

	CHECK(tool->get_name() == "agent.manage_plan");
	const String description = tool->get_description();
	CHECK(description.contains("create"));
	CHECK(description.contains("in_progress"));
	CHECK(description.contains("completed"));
	CHECK(description.contains("final response"));
	CHECK(description.contains("auto-archives"));

	Dictionary schema = tool->get_parameters_schema();
	Dictionary properties = schema["properties"];
	CHECK(properties.has("action"));
	CHECK(properties.has("tasks"));
	CHECK(properties.has("task_id"));
	CHECK(properties.has("status"));
	Dictionary action_property = properties["action"];
	CHECK(String(action_property["description"]).contains("before substantive work"));
	Dictionary tasks_property = properties["tasks"];
	CHECK(String(tasks_property["description"]).contains("concise"));
	Dictionary status_property = properties["status"];
	CHECK(String(status_property["description"]).contains("Mark the current task"));

	Dictionary create_args;
	create_args["action"] = "create";
	create_args["title"] = "Tool plan";
	Array tasks;
	tasks.push_back("First task");
	tasks.push_back("Second task");
	create_args["tasks"] = tasks;

	AIToolResult create_result = tool->execute(create_args);
	CHECK_FALSE(create_result.is_error());
	CHECK(create_result.content.contains("Created active plan."));
	CHECK(create_result.content.contains("task:1"));
	CHECK(String(create_result.metadata["plan_action"]) == "create");
	Dictionary active_plan = create_result.metadata["active_plan"];
	CHECK(String(active_plan["title"]) == "Tool plan");

	Array active_tasks = active_plan["tasks"];
	REQUIRE(active_tasks.size() == 2);
	Dictionary first_task = active_tasks[0];
	Dictionary second_task = active_tasks[1];
	const String first_task_id = String(first_task["id"]);
	const String second_task_id = String(second_task["id"]);

	Dictionary update_args;
	update_args["action"] = "set_task_status";
	update_args["task_id"] = first_task_id;
	update_args["status"] = "completed";
	AIToolResult update_result = tool->execute(update_args);
	CHECK_FALSE(update_result.is_error());
	CHECK(update_result.content.contains("Updated plan task."));
	CHECK(update_result.content.contains("completed"));
	CHECK(String(update_result.metadata["plan_action"]) == "set_task_status");
	Dictionary updated_active_plan = update_result.metadata["active_plan"];
	CHECK_FALSE(updated_active_plan.is_empty());

	update_args["task_id"] = second_task_id;
	AIToolResult archive_result = tool->execute(update_args);
	CHECK_FALSE(archive_result.is_error());
	CHECK(archive_result.content.contains("archived"));
	Dictionary final_active_plan = archive_result.metadata["active_plan"];
	Dictionary archived_tool_plan = archive_result.metadata["archived_plan"];
	CHECK(final_active_plan.is_empty());
	CHECK_FALSE(archived_tool_plan.is_empty());
	CHECK_FALSE(manager->has_active_plan());

	Dictionary missing_status_args;
	missing_status_args["action"] = "set_task_status";
	missing_status_args["task_id"] = "task:1";
	CHECK(tool->execute(missing_status_args).is_error());

	Dictionary unsupported_args;
	unsupported_args["action"] = "unknown";
	CHECK(tool->execute(unsupported_args).is_error());

	manager->clear_for_test();
}

TEST_CASE("[Editor][AI] Rule settings manage bounded prompt rules") {
	Array original_rules = AIRuleSettings::get_rule_storage_for_test();
	AIRuleSettings::clear_rules_for_test();

	const String long_rule = String("Keep responses focused. ").repeat(8);
	const String rule_id = AIRuleSettings::add_rule(long_rule, true);
	REQUIRE(!rule_id.is_empty());

	Vector<AIRuleConfig> rules = AIRuleSettings::get_rules(false);
	REQUIRE(rules.size() == 1);
	CHECK(rules[0].content.length() == 100);
	CHECK(rules[0].enabled);

	CHECK(AIRuleSettings::set_rule_enabled(rule_id, false));
	CHECK(AIRuleSettings::get_rules(true).is_empty());

	CHECK(AIRuleSettings::update_rule(rule_id, "Use tabs for indentation.", true));
	AIRuleConfig updated = AIRuleSettings::get_rule(rule_id);
	CHECK(updated.content == "Use tabs for indentation.");
	CHECK(updated.enabled);

	CHECK(AIRuleSettings::remove_rule(rule_id));
	CHECK(AIRuleSettings::get_rules(false).is_empty());

	AIRuleSettings::set_rule_storage_for_test(original_rules);
}

TEST_CASE("[Editor][AI] Rules context provider injects enabled rules") {
	Array original_rules = AIRuleSettings::get_rule_storage_for_test();
	AIRuleSettings::clear_rules_for_test();

	AIRuleSettings::add_rule("Prefer concise answers.", true);
	AIRuleSettings::add_rule("Disabled rule should not appear.", false);

	Ref<AIRulesContextProvider> provider;
	provider.instantiate();
	Array context = provider->collect_context();

	REQUIRE(context.size() == 1);
	Dictionary document = context[0];
	CHECK(String(document["title"]) == "User Rules");
	CHECK(String(document["source"]) == "ai_agent/rules");
	CHECK(String(document["content"]).contains("Prefer concise answers."));
	CHECK_FALSE(String(document["content"]).contains("Disabled rule should not appear."));

	AIRuleSettings::set_rule_storage_for_test(original_rules);
}

TEST_CASE("[Editor][AI] Diff service creates reviewable text change metadata") {
	Dictionary change = AIDiffService::build_text_change("res://scripts/player.gd", "modify", "extends Node\n", "extends Node\n\nfunc _ready():\n\tpass\n", "gdscript");

	CHECK(String(change["path"]) == "res://scripts/player.gd");
	CHECK(String(change["type"]) == "modify");
	CHECK(String(change["language"]) == "gdscript");
	CHECK(String(change["old_text"]) == "extends Node\n");
	CHECK(String(change["new_text"]).contains("func _ready"));
	CHECK((int)change["added_lines"] > 0);
	CHECK(String(change["diff"]).contains("--- res://scripts/player.gd"));
	CHECK(String(change["diff"]).contains("+++ res://scripts/player.gd"));
}

TEST_CASE("[Editor][AI] Change set store keeps pending review changes in memory") {
	Ref<AIChangeSetStore> store = AIChangeSetStore::get_singleton();
	store->clear_for_test();

	Array changes;
	changes.push_back(AIDiffService::build_text_change("res://scripts/player.gd", "modify", "old\n", "new\n", "gdscript"));
	String change_set_id = store->add_change_set("Update player script", "session-1", "tool-call-1", changes);

	CHECK_FALSE(change_set_id.is_empty());
	CHECK(store->get_pending_count() == 1);

	Dictionary change_set = store->get_change_set(change_set_id);
	CHECK(String(change_set["status"]) == "pending");
	CHECK(String(change_set["session_id"]) == "session-1");
	CHECK(String(change_set["tool_call_id"]) == "tool-call-1");
	CHECK((int)change_set["added_lines"] == 1);
	CHECK((int)change_set["removed_lines"] == 1);

	String error;
	CHECK(store->keep_change_set(change_set_id, error));
	CHECK(store->get_pending_count() == 0);
	CHECK(String(store->get_change_set(change_set_id)["status"]) == "kept");

	store->clear_for_test();
}

TEST_CASE("[Editor][AI] Change set store merges repeated file edits in one session") {
	Ref<AIChangeSetStore> store = AIChangeSetStore::get_singleton();
	store->clear_for_test();

	Array first_changes;
	first_changes.push_back(AIDiffService::build_text_change("res://scripts/player.gd", "modify", "old\n", "middle\n", "gdscript"));
	String first_id = store->add_change_set("First edit", "session-merge", "tool-call-1", first_changes);

	Array second_changes;
	second_changes.push_back(AIDiffService::build_text_change("res://scripts/player.gd", "modify", "middle\n", "final\n", "gdscript"));
	String second_id = store->add_change_set("Second edit", "session-merge", "tool-call-2", second_changes);

	CHECK(first_id == second_id);
	CHECK(store->get_pending_count() == 1);

	Dictionary change_set = store->get_change_set(first_id);
	CHECK(String(change_set["title"]) == "First edit");
	CHECK(String(change_set["tool_call_id"]) == "tool-call-2");
	Dictionary merged_metadata = change_set.get("metadata", Dictionary());
	CHECK(bool(merged_metadata.get("merged_review_change", false)));
	CHECK(String(merged_metadata.get("last_title", String())) == "Second edit");
	CHECK(String(merged_metadata.get("last_tool_call_id", String())) == "tool-call-2");

	Array changes = change_set["changes"];
	REQUIRE(changes.size() == 1);
	Dictionary merged_change = changes[0];
	CHECK(String(merged_change["path"]) == "res://scripts/player.gd");
	CHECK(String(merged_change["type"]) == "modify");
	CHECK(String(merged_change["old_text"]) == "old\n");
	CHECK(String(merged_change["new_text"]) == "final\n");
	CHECK(String(merged_change["diff"]).contains("-old"));
	CHECK(String(merged_change["diff"]).contains("+final"));
	CHECK_FALSE(String(merged_change["diff"]).contains("middle"));

	store->clear_for_test();
}

TEST_CASE("[Editor][AI] Change set store drops pending review when final text returns to original") {
	Ref<AIChangeSetStore> store = AIChangeSetStore::get_singleton();
	store->clear_for_test();

	Array first_changes;
	first_changes.push_back(AIDiffService::build_text_change("res://scripts/player.gd", "modify", "original\n", "changed\n", "gdscript"));
	String change_set_id = store->add_change_set("Edit script", "session-noop", "tool-call-1", first_changes);

	Array second_changes;
	second_changes.push_back(AIDiffService::build_text_change("res://scripts/player.gd", "modify", "changed\n", "original\n", "gdscript"));
	CHECK(store->add_change_set("Restore script", "session-noop", "tool-call-2", second_changes).is_empty());

	CHECK(store->get_pending_count() == 0);
	CHECK(String(store->get_change_set(change_set_id)["status"]) == "kept");

	store->clear_for_test();
}

TEST_CASE("[Editor][AI] Change set store drops create-then-delete review changes") {
	Ref<AIChangeSetStore> store = AIChangeSetStore::get_singleton();
	store->clear_for_test();

	Array create_changes;
	create_changes.push_back(AIDiffService::build_text_change("res://scripts/temp.gd", "create", "", "extends Node\n", "gdscript"));
	String change_set_id = store->add_change_set("Create temp script", "session-create-delete", "tool-call-1", create_changes);

	Array delete_changes;
	delete_changes.push_back(AIDiffService::build_text_change("res://scripts/temp.gd", "delete", "extends Node\n", "", "gdscript"));
	CHECK(store->add_change_set("Delete temp script", "session-create-delete", "tool-call-2", delete_changes).is_empty());

	CHECK(store->get_pending_count() == 0);
	CHECK(String(store->get_change_set(change_set_id)["status"]) == "kept");

	store->clear_for_test();
}

TEST_CASE("[Editor][AI] Scene editing tools expose explicit schemas") {
	Ref<AISceneCreateSceneTool> create_scene;
	create_scene.instantiate();
	CHECK(create_scene->get_name() == "scene.create_scene");
	Dictionary create_schema = create_scene->get_parameters_schema();
	Dictionary create_properties = create_schema["properties"];
	CHECK(create_properties.has("root_type"));
	CHECK(create_properties.has("root_name"));
	CHECK(create_properties.has("path"));
	Array create_required = create_schema["required"];
	CHECK(create_required.has("root_type"));
	CHECK(create_required.has("path"));

	Ref<AISceneAddNodeTool> add_node;
	add_node.instantiate();
	CHECK(add_node->get_name() == "scene.add_node");
	Dictionary add_schema = add_node->get_parameters_schema();
	Dictionary add_properties = add_schema["properties"];
	CHECK(add_properties.has("parent_path"));
	CHECK(add_properties.has("type"));
	CHECK(add_properties.has("name"));

	Ref<AISceneDeleteNodeTool> delete_node;
	delete_node.instantiate();
	CHECK(delete_node->get_name() == "scene.delete_node");
	Dictionary delete_schema = delete_node->get_parameters_schema();
	Dictionary delete_properties = delete_schema["properties"];
	CHECK(delete_properties.has("node_path"));

	Ref<AISceneListPropertiesTool> list_properties;
	list_properties.instantiate();
	CHECK(list_properties->get_name() == "scene.list_properties");
	Dictionary list_properties_schema = list_properties->get_parameters_schema();
	Dictionary list_properties_properties = list_properties_schema["properties"];
	CHECK(list_properties_properties.has("node_path"));
	CHECK(list_properties_properties.has("filter"));
	CHECK(list_properties_properties.has("max_properties"));
	CHECK(list_properties_properties.has("include_read_only"));
	CHECK(list_properties_properties.has("include_current_values"));

	Ref<AISceneRenameNodeTool> rename_node;
	rename_node.instantiate();
	CHECK(rename_node->get_name() == "scene.rename_node");
	Dictionary rename_schema = rename_node->get_parameters_schema();
	Dictionary rename_properties = rename_schema["properties"];
	CHECK(rename_properties.has("node_path"));
	CHECK(rename_properties.has("new_name"));
	Array rename_required = rename_schema["required"];
	CHECK(rename_required.has("node_path"));
	CHECK(rename_required.has("new_name"));

	Ref<AISceneMoveNodeTool> move_node;
	move_node.instantiate();
	CHECK(move_node->get_name() == "scene.move_node");
	Dictionary move_schema = move_node->get_parameters_schema();
	Dictionary move_properties = move_schema["properties"];
	CHECK(move_properties.has("node_path"));
	CHECK(move_properties.has("new_parent_path"));
	CHECK(move_properties.has("position"));

	Ref<AISceneSetPropertyTool> set_property;
	set_property.instantiate();
	CHECK(set_property->get_name() == "scene.set_property");
	Dictionary set_schema = set_property->get_parameters_schema();
	Dictionary set_properties = set_schema["properties"];
	CHECK(set_properties.has("node_path"));
	CHECK(set_properties.has("property_path"));
	CHECK(set_properties.has("value"));

	Ref<AISceneSaveCurrentSceneTool> save_current_scene;
	save_current_scene.instantiate();
	CHECK(save_current_scene->get_name() == "scene.save_current_scene");

	Ref<AISceneOpenSceneTool> open_scene;
	open_scene.instantiate();
	CHECK(open_scene->get_name() == "scene.open_scene");
	Dictionary open_schema = open_scene->get_parameters_schema();
	Dictionary open_properties = open_schema["properties"];
	CHECK(open_properties.has("path"));

	Ref<AICreateFolderTool> create_folder;
	create_folder.instantiate();
	CHECK(create_folder->get_name() == "project.create_folder");
	Dictionary folder_schema = create_folder->get_parameters_schema();
	Dictionary folder_properties = folder_schema["properties"];
	CHECK(folder_properties.has("path"));

	Ref<AIScriptInspectTool> script_inspect;
	script_inspect.instantiate();
	CHECK(script_inspect->get_name() == "script.inspect");
	Dictionary inspect_properties = script_inspect->get_parameters_schema()["properties"];
	CHECK(inspect_properties.has("path"));

	Ref<AIScriptCreateTool> script_create;
	script_create.instantiate();
	CHECK(script_create->get_name() == "script.create");
	Dictionary script_create_properties = script_create->get_parameters_schema()["properties"];
	CHECK(script_create_properties.has("path"));
	CHECK(script_create_properties.has("extends"));
	CHECK(script_create_properties.has("source"));
	CHECK(script_create_properties.has("overwrite"));

	Ref<AIScriptWriteTool> script_write;
	script_write.instantiate();
	CHECK(script_write->get_name() == "script.write");
	Dictionary script_write_properties = script_write->get_parameters_schema()["properties"];
	CHECK(script_write_properties.has("path"));
	CHECK(script_write_properties.has("source"));

	Ref<AIScriptPatchFunctionTool> script_patch_function;
	script_patch_function.instantiate();
	CHECK(script_patch_function->get_name() == "script.patch_function");
	Dictionary script_patch_properties = script_patch_function->get_parameters_schema()["properties"];
	CHECK(script_patch_properties.has("path"));
	CHECK(script_patch_properties.has("function_name"));
	CHECK(script_patch_properties.has("function_source"));
	CHECK(script_patch_properties.has("create_if_missing"));

	Ref<AIScriptBindToNodeTool> script_bind_to_node;
	script_bind_to_node.instantiate();
	CHECK(script_bind_to_node->get_name() == "script.bind_to_node");
	Dictionary bind_properties = script_bind_to_node->get_parameters_schema()["properties"];
	CHECK(bind_properties.has("node_path"));
	CHECK(bind_properties.has("script_path"));

	Ref<AIScriptUnbindFromNodeTool> script_unbind_from_node;
	script_unbind_from_node.instantiate();
	CHECK(script_unbind_from_node->get_name() == "script.unbind_from_node");
	Dictionary unbind_properties = script_unbind_from_node->get_parameters_schema()["properties"];
	CHECK(unbind_properties.has("node_path"));

	Ref<AIScriptDeleteTool> script_delete;
	script_delete.instantiate();
	CHECK(script_delete->get_name() == "script.delete");
	Dictionary delete_script_properties = script_delete->get_parameters_schema()["properties"];
	CHECK(delete_script_properties.has("path"));

	Ref<AIShaderCreateTool> shader_create;
	shader_create.instantiate();
	CHECK(shader_create->get_name() == "shader.create");
	Dictionary shader_create_schema = shader_create->get_parameters_schema();
	Dictionary shader_create_properties = shader_create_schema["properties"];
	CHECK(shader_create_properties.has("path"));
	CHECK(shader_create_properties.has("shader_type"));
	CHECK(shader_create_properties.has("shader_code"));
	CHECK(shader_create_properties.has("overwrite"));
	Array shader_create_required = shader_create_schema["required"];
	CHECK(shader_create_required.has("path"));

	Ref<AIShaderEditTool> shader_edit;
	shader_edit.instantiate();
	CHECK(shader_edit->get_name() == "shader.edit");
	Dictionary shader_edit_schema = shader_edit->get_parameters_schema();
	Dictionary shader_edit_properties = shader_edit_schema["properties"];
	CHECK(shader_edit_properties.has("path"));
	CHECK(shader_edit_properties.has("shader_code"));
	Array shader_edit_required = shader_edit_schema["required"];
	CHECK(shader_edit_required.has("path"));
	CHECK(shader_edit_required.has("shader_code"));

	Ref<AIShaderDeleteTool> shader_delete;
	shader_delete.instantiate();
	CHECK(shader_delete->get_name() == "shader.delete");
	Dictionary shader_delete_properties = shader_delete->get_parameters_schema()["properties"];
	CHECK(shader_delete_properties.has("path"));

	Ref<AIShaderApplyToNodeTool> shader_apply;
	shader_apply.instantiate();
	CHECK(shader_apply->get_name() == "shader.apply_to_node");
	Dictionary shader_schema = shader_apply->get_parameters_schema();
	Dictionary shader_properties = shader_schema["properties"];
	CHECK(shader_properties.has("node_path"));
	CHECK(shader_properties.has("shader_path"));
	CHECK(shader_properties.has("target_property"));
	CHECK_FALSE(shader_properties.has("shader_code"));
	CHECK_FALSE(shader_properties.has("material_property"));
	CHECK_FALSE(shader_properties.has("overwrite_shader"));
	CHECK(shader_properties.has("shader_parameters"));
	Array shader_required = shader_schema["required"];
	CHECK(shader_required.has("node_path"));
	CHECK(shader_required.has("shader_path"));
	CHECK(shader_required.has("target_property"));
}

TEST_CASE("[Editor][AI] Scene editing tools validate required arguments before touching editor state") {
	Ref<AISceneCreateSceneTool> create_scene;
	create_scene.instantiate();
	Dictionary create_arguments;
	CHECK(create_scene->execute(create_arguments).is_error());
	create_arguments["root_type"] = "Node2D";
	CHECK(create_scene->execute(create_arguments).is_error());

	Ref<AISceneAddNodeTool> add_node;
	add_node.instantiate();
	Dictionary add_arguments;
	CHECK(add_node->execute(add_arguments).is_error());

	Ref<AISceneDeleteNodeTool> delete_node;
	delete_node.instantiate();
	Dictionary delete_arguments;
	CHECK(delete_node->execute(delete_arguments).is_error());

	Ref<AISceneListPropertiesTool> list_properties;
	list_properties.instantiate();
	Dictionary list_properties_arguments;
	CHECK(list_properties->execute(list_properties_arguments).is_error());

	Ref<AISceneRenameNodeTool> rename_node;
	rename_node.instantiate();
	Dictionary rename_arguments;
	CHECK(rename_node->execute(rename_arguments).is_error());
	rename_arguments["node_path"] = "Player";
	CHECK(rename_node->execute(rename_arguments).is_error());

	Ref<AISceneMoveNodeTool> move_node;
	move_node.instantiate();
	Dictionary move_arguments;
	CHECK(move_node->execute(move_arguments).is_error());
	move_arguments["node_path"] = "Player";
	CHECK(move_node->execute(move_arguments).is_error());

	Ref<AISceneSetPropertyTool> set_property;
	set_property.instantiate();
	Dictionary property_arguments;
	CHECK(set_property->execute(property_arguments).is_error());
	property_arguments["node_path"] = "Player";
	CHECK(set_property->execute(property_arguments).is_error());
	property_arguments["property_path"] = "position";
	CHECK(set_property->execute(property_arguments).is_error());

	Ref<AISceneOpenSceneTool> open_scene;
	open_scene.instantiate();
	Dictionary open_arguments;
	CHECK(open_scene->execute(open_arguments).is_error());

	Ref<AICreateFolderTool> create_folder;
	create_folder.instantiate();
	Dictionary folder_arguments;
	CHECK(create_folder->execute(folder_arguments).is_error());
	folder_arguments["path"] = "C:/outside";
	CHECK(create_folder->execute(folder_arguments).is_error());
	folder_arguments["path"] = "res://";
	CHECK(create_folder->execute(folder_arguments).is_error());

	Ref<AIScriptInspectTool> script_inspect;
	script_inspect.instantiate();
	Dictionary inspect_arguments;
	CHECK(script_inspect->execute(inspect_arguments).is_error());

	Ref<AIScriptCreateTool> script_create;
	script_create.instantiate();
	Dictionary script_create_arguments;
	CHECK(script_create->execute(script_create_arguments).is_error());

	Ref<AIScriptWriteTool> script_write;
	script_write.instantiate();
	Dictionary script_write_arguments;
	CHECK(script_write->execute(script_write_arguments).is_error());
	script_write_arguments["path"] = "res://scripts/player.gd";
	CHECK(script_write->execute(script_write_arguments).is_error());

	Ref<AIScriptPatchFunctionTool> script_patch_function;
	script_patch_function.instantiate();
	Dictionary patch_arguments;
	CHECK(script_patch_function->execute(patch_arguments).is_error());
	patch_arguments["path"] = "res://scripts/player.gd";
	CHECK(script_patch_function->execute(patch_arguments).is_error());
	patch_arguments["function_name"] = "_ready";
	CHECK(script_patch_function->execute(patch_arguments).is_error());

	Ref<AIScriptBindToNodeTool> script_bind_to_node;
	script_bind_to_node.instantiate();
	Dictionary bind_arguments;
	CHECK(script_bind_to_node->execute(bind_arguments).is_error());
	bind_arguments["node_path"] = ".";
	CHECK(script_bind_to_node->execute(bind_arguments).is_error());

	Ref<AIScriptUnbindFromNodeTool> script_unbind_from_node;
	script_unbind_from_node.instantiate();
	Dictionary unbind_arguments;
	CHECK(script_unbind_from_node->execute(unbind_arguments).is_error());

	Ref<AIScriptDeleteTool> script_delete;
	script_delete.instantiate();
	Dictionary delete_script_arguments;
	CHECK(script_delete->execute(delete_script_arguments).is_error());

	Ref<AIShaderCreateTool> shader_create;
	shader_create.instantiate();
	Dictionary shader_create_arguments;
	CHECK(shader_create->execute(shader_create_arguments).is_error());

	Ref<AIShaderEditTool> shader_edit;
	shader_edit.instantiate();
	Dictionary shader_edit_arguments;
	CHECK(shader_edit->execute(shader_edit_arguments).is_error());
	shader_edit_arguments["path"] = "res://shaders/player_flash.gdshader";
	CHECK(shader_edit->execute(shader_edit_arguments).is_error());

	Ref<AIShaderDeleteTool> shader_delete;
	shader_delete.instantiate();
	Dictionary shader_delete_arguments;
	CHECK(shader_delete->execute(shader_delete_arguments).is_error());

	Ref<AIShaderApplyToNodeTool> shader_apply;
	shader_apply.instantiate();
	Dictionary shader_arguments;
	CHECK(shader_apply->execute(shader_arguments).is_error());
	shader_arguments["node_path"] = "Player";
	CHECK(shader_apply->execute(shader_arguments).is_error());
	shader_arguments["shader_path"] = "res://shaders/player_flash.gdshader";
	CHECK(shader_apply->execute(shader_arguments).is_error());
	shader_arguments["target_property"] = "material";
	shader_arguments["shader_parameters"] = "not an object";
	CHECK(shader_apply->execute(shader_arguments).is_error());
}

TEST_CASE("[Editor][AI] Read-only project tools enforce project boundaries") {
	Ref<AIReadFileTool> read_file;
	read_file.instantiate();

	Dictionary arguments;
	arguments["path"] = "C:/outside.txt";
	AIToolResult outside_result = read_file->execute(arguments);
	CHECK(outside_result.is_error());

	arguments["path"] = "res://../outside.txt";
	AIToolResult traversal_result = read_file->execute(arguments);
	CHECK(traversal_result.is_error());

	arguments["path"] = "res://icon.png";
	AIToolResult extension_result = read_file->execute(arguments);
	CHECK(extension_result.is_error());
}

TEST_CASE("[Editor][AI] Read-only project tools return bounded textual results") {
	Ref<AIListProjectTool> list_project;
	list_project.instantiate();

	Dictionary list_arguments;
	list_arguments["path"] = "res://";
	list_arguments["max_depth"] = 1;
	list_arguments["max_entries"] = 20;
	AIToolResult tree_result = list_project->execute(list_arguments);
	CHECK_FALSE(tree_result.is_error());
	CHECK(tree_result.content.contains("res://"));

	Ref<AISearchProjectTool> search_project;
	search_project.instantiate();

	Dictionary search_arguments;
	search_arguments["path"] = "res://";
	search_arguments["query"] = "project";
	search_arguments["max_results"] = 5;
	AIToolResult search_result = search_project->execute(search_arguments);
	CHECK_FALSE(search_result.is_error());
	CHECK(search_result.content.length() <= 4096);
}

TEST_CASE("[Editor][AI] Editor context tool exposes safe metadata only") {
	Ref<AIGetEditorContextTool> editor_context;
	editor_context.instantiate();

	Dictionary arguments;
	AIToolResult result = editor_context->execute(arguments);

	CHECK_FALSE(result.is_error());
	CHECK(result.content.contains("Editor Context"));
	CHECK_FALSE(result.content.contains("api_key"));
	CHECK_FALSE(result.content.contains("API Key"));
	CHECK_FALSE(result.content.contains("Authorization"));
}

TEST_CASE("[Editor][AI] OpenAI-compatible codec serializes optional tool schemas") {
	Array messages;
	Dictionary user_message;
	user_message["role"] = "user";
	user_message["content"] = "List files";
	messages.push_back(user_message);

	Ref<AIToolRegistry> registry;
	registry.instantiate();

	Ref<AIListProjectTool> list_project;
	list_project.instantiate();
	CHECK(registry->register_tool(list_project));

	String body_text = String::utf8(reinterpret_cast<const char *>(AIOpenAICompatibleCodec::build_body(messages, "test-model", registry->get_tool_schemas()).ptr()));
	CHECK(body_text.contains("\"model\":\"test-model\""));
	CHECK(body_text.contains("\"tools\""));
	CHECK(body_text.contains("\"project.list_tree\""));
}

} // namespace TestAIAgentTools
