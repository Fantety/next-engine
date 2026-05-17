/**************************************************************************/
/*  ai_conversation_store.h                                                */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/vector.h"

#include "editor/ai_component/agent/ai_agent_message.h"

class AIConversationStore : public RefCounted {
	GDCLASS(AIConversationStore, RefCounted);

	String base_dir = "user://ai_agent/conversations";

	Error _ensure_base_dir() const;
	String _get_session_path(const String &p_session_id) const;
	static String _sanitize_scope_key(const String &p_scope_key);

protected:
	static void _bind_methods();

public:
	void set_project_scope(const String &p_project_scope_key);
	String get_base_dir_for_test() const;
	Error save_conversation(const String &p_session_id, const String &p_title, const Vector<AIAgentMessage> &p_messages);
	bool load_conversation(const String &p_session_id, String &r_title, Vector<AIAgentMessage> &r_messages) const;
	bool load_conversation_metadata(const String &p_session_id, Dictionary &r_metadata) const;
	bool delete_conversation(const String &p_session_id) const;
	bool get_most_recent_conversation_id(String &r_session_id) const;
	Array list_conversations() const;
};
