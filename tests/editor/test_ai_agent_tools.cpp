/**************************************************************************/
/*  test_ai_agent_tools.cpp                                               */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/agent/ai_agent_profile.h"
#include "editor/ai_component/providers/ai_openai_compatible_codec.h"
#include "editor/ai_component/review/ai_change_set_store.h"
#include "editor/ai_component/review/ai_diff_service.h"
#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/ai_tool_call.h"
#include "editor/ai_component/tools/ai_tool_permission.h"
#include "editor/ai_component/tools/ai_tool_registry.h"
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
#include "editor/ai_component/tools/project/ai_create_folder_tool.h"
#include "editor/ai_component/tools/project/ai_list_project_tool.h"
#include "editor/ai_component/tools/project/ai_read_file_tool.h"
#include "editor/ai_component/tools/project/ai_search_project_tool.h"

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
	AIAgentProfile review = AIAgentProfile::get_review_profile();
	AIAgentProfile write = AIAgentProfile::get_write_profile();

	Dictionary arguments;
	CHECK(plan.id == "plan");
	CHECK(build.id == "build");
	CHECK(review.id == "review");
	CHECK(write.id == "write");

	CHECK(AIToolPermissionPolicy::evaluate(plan, "project.list_tree", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "project.read_file", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "project.search_text", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "project.create_folder", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "editor.get_context", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "scene.create_scene", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "scene.add_node", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "scene.delete_node", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "scene.list_properties", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "scene.rename_node", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "scene.move_node", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "scene.set_property", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "scene.save_current_scene", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "scene.open_scene", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "script.inspect", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "script.create", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "script.write", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "script.patch_function", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "script.bind_to_node", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "script.unbind_from_node", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "script.delete", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "shader.apply_to_node", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "project.write_file", arguments).decision == AI_TOOL_PERMISSION_DENY);
	CHECK(AIToolPermissionPolicy::evaluate(plan, "unknown.tool", arguments).decision == AI_TOOL_PERMISSION_DENY);

	CHECK(AIToolPermissionPolicy::evaluate(build, "project.list_tree", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(build, "project.write_file", arguments).decision == AI_TOOL_PERMISSION_DENY);

	CHECK(AIToolPermissionPolicy::evaluate(review, "project.list_tree", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(review, "project.create_folder", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(review, "script.write", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(review, "script.delete", arguments).decision == AI_TOOL_PERMISSION_ASK);
	CHECK(AIToolPermissionPolicy::evaluate(review, "shader.apply_to_node", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(review, "project.write_file", arguments).decision == AI_TOOL_PERMISSION_DENY);

	CHECK(AIToolPermissionPolicy::evaluate(write, "project.list_tree", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "project.create_folder", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "editor.get_context", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "scene.create_scene", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "scene.add_node", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "scene.delete_node", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "scene.list_properties", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "scene.rename_node", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "scene.move_node", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "scene.set_property", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "scene.save_current_scene", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "scene.open_scene", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "script.inspect", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "script.create", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "script.write", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "script.patch_function", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "script.bind_to_node", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "script.unbind_from_node", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "script.delete", arguments).decision == AI_TOOL_PERMISSION_ASK);
	CHECK(AIToolPermissionPolicy::evaluate(write, "shader.apply_to_node", arguments).decision == AI_TOOL_PERMISSION_ALLOW);
	CHECK(AIToolPermissionPolicy::evaluate(write, "project.write_file", arguments).decision == AI_TOOL_PERMISSION_DENY);

	CHECK(AIToolPermissionPolicy::decision_to_string(AI_TOOL_PERMISSION_ALLOW) == "allow");
	CHECK(AIToolPermissionPolicy::decision_to_string(AI_TOOL_PERMISSION_ASK) == "ask");
	CHECK(AIToolPermissionPolicy::decision_to_string(AI_TOOL_PERMISSION_DENY) == "deny");
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

	Ref<AIShaderApplyToNodeTool> shader_apply;
	shader_apply.instantiate();
	CHECK(shader_apply->get_name() == "shader.apply_to_node");
	Dictionary shader_schema = shader_apply->get_parameters_schema();
	Dictionary shader_properties = shader_schema["properties"];
	CHECK(shader_properties.has("node_path"));
	CHECK(shader_properties.has("shader_path"));
	CHECK(shader_properties.has("shader_code"));
	CHECK(shader_properties.has("material_property"));
	CHECK(shader_properties.has("overwrite_shader"));
	CHECK(shader_properties.has("shader_parameters"));
	Array shader_required = shader_schema["required"];
	CHECK(shader_required.has("node_path"));
	CHECK(shader_required.has("shader_path"));
	CHECK(shader_required.has("shader_code"));
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

	Ref<AIShaderApplyToNodeTool> shader_apply;
	shader_apply.instantiate();
	Dictionary shader_arguments;
	CHECK(shader_apply->execute(shader_arguments).is_error());
	shader_arguments["node_path"] = "Player";
	CHECK(shader_apply->execute(shader_arguments).is_error());
	shader_arguments["shader_path"] = "res://shaders/player_flash.gdshader";
	CHECK(shader_apply->execute(shader_arguments).is_error());
	shader_arguments["shader_code"] = "shader_type canvas_item;\nvoid fragment() { COLOR = vec4(1.0); }\n";
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
