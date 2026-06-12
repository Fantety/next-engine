/**************************************************************************/
/*  ai_editor_tools_v1.h                                                   */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/tools/ai_tool_v1.h"

#include "core/object/ref_counted.h"
#include "core/os/safe_binary_mutex.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

struct AIV1EditorToolResult {
	String content;
	String error;
	Dictionary metadata;
	bool truncated = false;

	bool is_error() const;
	Dictionary to_dict() const;
};

class AIV1ToolExecutionState : public RefCounted {
	GDCLASS(AIV1ToolExecutionState, RefCounted);

	String session_id;
	String agent_id;
	String tool_call_id;
	Ref<AICancelToken> cancel_token;
	SafeFlag cancel_requested;
	bool review_changes = false;

	static thread_local Ref<AIV1ToolExecutionState> current;

protected:
	static void _bind_methods();

public:
	static void set_current(const Ref<AIV1ToolExecutionState> &p_context);
	static Ref<AIV1ToolExecutionState> get_current();
	static void clear_current();
	static bool is_current_cancel_requested();

	void set_session_id(const String &p_session_id);
	String get_session_id() const;

	void set_agent_id(const String &p_agent_id);
	String get_agent_id() const;
	String get_agent_profile_id() const;

	void set_tool_call_id(const String &p_tool_call_id);
	String get_tool_call_id() const;

	void set_review_changes(bool p_review_changes);
	bool should_review_changes() const;

	void set_cancel_token(const Ref<AICancelToken> &p_cancel_token);
	Ref<AICancelToken> get_cancel_token() const;
	void request_cancel();
	void clear_cancel_request();
	bool is_cancel_requested() const;
};

class AIV1EditorTool : public AIV1Tool {
	GDCLASS(AIV1EditorTool, AIV1Tool);

	String permission_action;
	String permission_effect;
	String permission_reason;

protected:
	static void _bind_methods();

	bool assert_tool_permission(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIError &r_error) const;
	String get_permission_resource(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context) const;

public:
	virtual String get_name() const = 0;
	virtual String get_description() const = 0;
	virtual Dictionary get_parameters_schema() const = 0;
	virtual AIV1EditorToolResult execute_tool(const Dictionary &p_arguments) = 0;

	void configure_editor_tool(const String &p_permission_action, const String &p_permission_effect, const String &p_permission_reason, const Dictionary &p_metadata = Dictionary());
	virtual bool execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) override;
};

namespace AIV1ToolHelpers {

Dictionary make_string_property(const String &p_description);
Dictionary make_boolean_property(const String &p_description);
Dictionary make_object_schema(const Dictionary &p_properties, const Array &p_required = Array());
String get_stripped_string(const Dictionary &p_arguments, const String &p_key, const String &p_default = String());
bool get_bool(const Dictionary &p_arguments, const String &p_key, bool p_default = false);
AIV1EditorToolResult make_missing_required_error(const String &p_key);

template <typename TEditingResult>
AIV1EditorToolResult from_editing_result(const TEditingResult &p_editing_result, const String &p_fallback_error) {
	AIV1EditorToolResult result;
	result.metadata = p_editing_result.metadata;
	if (!p_editing_result.success) {
		result.error = p_editing_result.error.is_empty() ? p_fallback_error : p_editing_result.error;
		return result;
	}

	result.content = p_editing_result.message;
	return result;
}

} // namespace AIV1ToolHelpers

class AIV1ToolRegistry;

namespace AIV1EditorTools {

void register_editor_tools(const Ref<AIV1ToolRegistry> &p_registry);

} // namespace AIV1EditorTools
