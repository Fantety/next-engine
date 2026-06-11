/**************************************************************************/
/*  ai_tool_v1.cpp                                                        */
/**************************************************************************/

#include "ai_tool_v1.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

Dictionary AIV1ToolExecutionContext::to_dictionary() const {
	Dictionary result;
	result["session_id"] = session_id;
	result["agent_id"] = agent_id;
	result["assistant_message_id"] = assistant_message_id;
	result["call_id"] = call_id;
	result["tool_name"] = tool_name;
	result["root_dir"] = root_dir;
	result["source"] = source;
	result["metadata"] = metadata;
	return result;
}

Dictionary AIV1ToolExecutionContext::make_permission_input(const String &p_action, const String &p_resource, const String &p_reason) const {
	Dictionary input;
	input["session_id"] = session_id;
	input["action"] = p_action;
	input["resource"] = p_resource;
	input["reason"] = p_reason;
	input["source"] = source;
	return input;
}

Dictionary AIV1ToolExecutionResult::to_dictionary() const {
	Dictionary output;
	output["success"] = success && !error.is_error();
	output["result"] = result;
	output["content"] = content;
	output["structured"] = structured;
	output["output_paths"] = output_paths;
	output["error"] = error.to_dictionary();
	output["metadata"] = metadata;
	return output;
}

AIV1ToolExecutionResult AIV1ToolExecutionResult::ok(const Variant &p_result, const Variant &p_content, const Variant &p_structured) {
	AIV1ToolExecutionResult output;
	output.success = true;
	output.result = p_result;
	output.content = p_content;
	output.structured = p_structured;
	return output;
}

AIV1ToolExecutionResult AIV1ToolExecutionResult::fail(const AIError &p_error) {
	AIV1ToolExecutionResult output;
	output.success = false;
	output.error = p_error.is_error() ? p_error : AIError::make(AI_ERROR_INTERNAL, "Tool execution failed.");
	return output;
}

void AIV1Tool::_bind_methods() {
	ClassDB::bind_method(D_METHOD("configure", "description", "input_schema", "executor", "metadata"), &AIV1Tool::configure, DEFVAL(Callable()), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("set_description", "description"), &AIV1Tool::set_description);
	ClassDB::bind_method(D_METHOD("get_description"), &AIV1Tool::get_description);
	ClassDB::bind_method(D_METHOD("set_input_schema", "schema"), &AIV1Tool::set_input_schema);
	ClassDB::bind_method(D_METHOD("get_input_schema"), &AIV1Tool::get_input_schema);
	ClassDB::bind_method(D_METHOD("set_metadata", "metadata"), &AIV1Tool::set_metadata);
	ClassDB::bind_method(D_METHOD("get_metadata"), &AIV1Tool::get_metadata);
	ClassDB::bind_method(D_METHOD("set_executor", "executor"), &AIV1Tool::set_executor);
	ClassDB::bind_method(D_METHOD("get_executor"), &AIV1Tool::get_executor);
	ClassDB::bind_method(D_METHOD("execute", "arguments"), &AIV1Tool::execute);
}

bool AIV1Tool::_execute_callable(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) const {
	if (!executor.is_valid()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Tool has no executor.");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Dictionary context_dict = p_context.to_dictionary();
	Variant arg0 = p_arguments;
	Variant arg1 = context_dict;
	const Variant *args[2] = { &arg0, &arg1 };
	Variant ret;
	Callable::CallError ce;
	executor.callp(args, 2, ret, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Tool callable failed: " + Variant::get_callable_error_text(executor, args, 2, ce) + ".");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	if (ret.get_type() == Variant::DICTIONARY) {
		const Dictionary result_dict = ret;
		const bool success = bool(result_dict.get("success", true));
		if (!success) {
			const Dictionary error_dict = result_dict.get("error", Dictionary());
			r_error = AIError::make(AIError::string_to_kind(error_dict.get("kind", error_dict.get("type", "internal"))), error_dict.get("message", "Tool callable failed."), error_dict.get("details", Dictionary()));
			r_result = AIV1ToolExecutionResult::fail(r_error);
			return false;
		}

		r_result = AIV1ToolExecutionResult::ok(result_dict.get("result", ret), result_dict.get("content", result_dict.get("result", ret)), result_dict.get("structured", result_dict.get("result", ret)));
		if (result_dict.get("metadata", Variant()).get_type() == Variant::DICTIONARY) {
			r_result.metadata = Dictionary(result_dict["metadata"]).duplicate(true);
		}
		return true;
	}

	r_result = AIV1ToolExecutionResult::ok(ret, ret, ret);
	return true;
}

void AIV1Tool::configure(const String &p_description, const Dictionary &p_input_schema, const Callable &p_executor, const Dictionary &p_metadata) {
	description = p_description;
	input_schema = p_input_schema.duplicate(true);
	executor = p_executor;
	metadata = p_metadata.duplicate(true);
}

void AIV1Tool::set_description(const String &p_description) {
	description = p_description;
}

String AIV1Tool::get_description() const {
	return description;
}

void AIV1Tool::set_input_schema(const Dictionary &p_schema) {
	input_schema = p_schema.duplicate(true);
}

Dictionary AIV1Tool::get_input_schema() const {
	return input_schema.duplicate(true);
}

void AIV1Tool::set_metadata(const Dictionary &p_metadata) {
	metadata = p_metadata.duplicate(true);
}

Dictionary AIV1Tool::get_metadata() const {
	return metadata.duplicate(true);
}

void AIV1Tool::set_executor(const Callable &p_executor) {
	executor = p_executor;
}

Callable AIV1Tool::get_executor() const {
	return executor;
}

AIModelToolDefinition AIV1Tool::to_model_definition(const String &p_name, const Dictionary &p_identity) const {
	AIModelToolDefinition definition;
	definition.name = p_name;
	definition.description = description;
	definition.input_schema = input_schema.duplicate(true);
	definition.metadata = metadata.duplicate(true);
	definition.metadata["registration_identity"] = p_identity;
	return definition;
}

bool AIV1Tool::execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) {
	return _execute_callable(p_arguments, p_context, r_result, r_error);
}

Dictionary AIV1Tool::execute(const Dictionary &p_arguments) {
	AIV1ToolExecutionContext context;
	AIV1ToolExecutionResult result;
	AIError error;
	execute_struct(p_arguments, context, result, error);
	return result.to_dictionary();
}
