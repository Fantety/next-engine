/**************************************************************************/
/*  ai_tool_v1.h                                                          */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_cancel_token.h"
#include "editor/agent_v1/core/base/ai_error.h"
#include "editor/agent_v1/core/runtime/ai_model_request.h"
#include "editor/agent_v1/permission/ai_permission_service.h"

#include "core/object/ref_counted.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"

struct AIV1ToolExecutionContext {
	String session_id;
	String agent_id;
	String assistant_message_id;
	String call_id;
	String tool_name;
	String root_dir;
	Dictionary source;
	Dictionary metadata;
	Ref<AIPermissionService> permission_service;
	Ref<AICancelToken> cancel_token;

	Dictionary to_dictionary() const;
	Dictionary make_permission_input(const String &p_action, const String &p_resource, const String &p_reason = String()) const;
};

struct AIV1ToolExecutionResult {
	bool success = false;
	Variant result;
	Variant content;
	Variant structured;
	PackedStringArray output_paths;
	AIError error;
	Dictionary metadata;

	Dictionary to_dictionary() const;
	static AIV1ToolExecutionResult ok(const Variant &p_result = Variant(), const Variant &p_content = Variant(), const Variant &p_structured = Variant());
	static AIV1ToolExecutionResult fail(const AIError &p_error);
};

class AIV1Tool : public RefCounted {
	GDCLASS(AIV1Tool, RefCounted);

	String description;
	Dictionary input_schema;
	Dictionary metadata;
	Callable executor;

protected:
	static void _bind_methods();
	bool _execute_callable(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) const;

public:
	void configure(const String &p_description, const Dictionary &p_input_schema, const Callable &p_executor = Callable(), const Dictionary &p_metadata = Dictionary());
	void set_description(const String &p_description);
	String get_description() const;
	void set_input_schema(const Dictionary &p_schema);
	Dictionary get_input_schema() const;
	void set_metadata(const Dictionary &p_metadata);
	Dictionary get_metadata() const;
	void set_executor(const Callable &p_executor);
	Callable get_executor() const;

	AIModelToolDefinition to_model_definition(const String &p_name, const Dictionary &p_identity) const;
	virtual bool execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error);
	Dictionary execute(const Dictionary &p_arguments);
};
