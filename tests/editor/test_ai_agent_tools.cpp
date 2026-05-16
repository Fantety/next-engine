/**************************************************************************/
/*  test_ai_agent_tools.cpp                                               */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/agent/ai_agent_profile.h"
#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/ai_tool_call.h"
#include "editor/ai_component/tools/ai_tool_permission.h"
#include "editor/ai_component/tools/ai_tool_registry.h"

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

TEST_CASE("[Editor][AI] Agent profiles centralize read-only tool permissions") {
	AIAgentProfile plan = AIAgentProfile::get_plan_profile();
	AIAgentProfile build = AIAgentProfile::get_build_profile();

	Dictionary arguments;
	CHECK(plan.id == "plan");
	CHECK(build.id == "build");

	CHECK(AIToolPermissionPolicy::evaluate(plan, "project.list_tree", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "project.read_file", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "project.search_text", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "editor.get_context", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "project.write_file", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "unknown.tool", arguments).decision == AI_TOOL_PERMISSION_DENY);

	CHECK(AIToolPermissionPolicy::evaluate(build, "project.list_tree", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(build, "project.write_file", arguments).decision == AI_TOOL_PERMISSION_DENY);

	CHECK(AIToolPermissionPolicy::decision_to_string(AI_TOOL_PERMISSION_ALLOW) == "allow");
	CHECK(AIToolPermissionPolicy::decision_to_string(AI_TOOL_PERMISSION_ASK) == "ask");
	CHECK(AIToolPermissionPolicy::decision_to_string(AI_TOOL_PERMISSION_DENY) == "deny");
}

} // namespace TestAIAgentTools
