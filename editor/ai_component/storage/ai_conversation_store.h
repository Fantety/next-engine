/**************************************************************************/
/*  ai_conversation_store.h                                                */
/**************************************************************************/

#pragma once

#include "core/templates/vector.h"

#include "editor/ai_component/agent/ai_agent_message.h"
#include "editor/ai_component/storage/ai_storage_base.h"

class AIConversationStore : public AIStorageBase {
	GDCLASS(AIConversationStore, AIStorageBase);

	String _get_session_path(const String &p_session_id) const;
	String _get_metadata_path(const String &p_session_id) const;

protected:
	static void _bind_methods();

public:
	AIConversationStore();

	void set_project_scope(const String &p_project_scope_key);
	String get_base_dir_for_test() const;
	Error save_conversation(const String &p_session_id, const String &p_title, const Vector<AIAgentMessage> &p_messages);
	bool load_conversation(const String &p_session_id, String &r_title, Vector<AIAgentMessage> &r_messages) const;
	bool load_conversation_metadata(const String &p_session_id, Dictionary &r_metadata) const;
	bool delete_conversation(const String &p_session_id) const;
	bool get_most_recent_conversation_id(String &r_session_id) const;
	Array list_conversations() const;
};
