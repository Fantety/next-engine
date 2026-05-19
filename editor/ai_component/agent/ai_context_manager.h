/**************************************************************************/
/*  ai_context_manager.h                                                  */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

#include "editor/ai_component/agent/ai_agent_message.h"

struct AIContextBuildResult {
	Array messages;
	Dictionary metadata;
};

class AIContextManager : public RefCounted {
	GDCLASS(AIContextManager, RefCounted);

	int max_input_chars = 96000;
	int max_context_chars = 24000;
	int max_history_chars = 64000;
	int max_tool_result_chars = 16000;
	int min_recent_messages = 4;

	String _truncate_text(const String &p_text, int p_max_chars, bool &r_truncated) const;
	String _build_context_text(const Array &p_context_documents, int &r_truncated_documents) const;
	Dictionary _message_to_provider_dict(const AIAgentMessage &p_message) const;
	int _estimate_message_chars(const Dictionary &p_message) const;
	int _estimate_messages_chars(const Array &p_messages) const;
	int _count_truncated_tool_results(const Array &p_messages) const;

protected:
	static void _bind_methods();

public:
	void set_max_input_chars(int p_max_input_chars);
	int get_max_input_chars() const;

	void set_max_context_chars(int p_max_context_chars);
	int get_max_context_chars() const;

	void set_max_history_chars(int p_max_history_chars);
	int get_max_history_chars() const;

	void set_max_tool_result_chars(int p_max_tool_result_chars);
	int get_max_tool_result_chars() const;

	void set_min_recent_messages(int p_min_recent_messages);
	int get_min_recent_messages() const;

	AIContextBuildResult build_messages(const String &p_system_prompt, const Vector<AIAgentMessage> &p_messages, const Array &p_context_documents = Array()) const;
};
