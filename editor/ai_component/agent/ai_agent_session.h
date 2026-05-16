/**************************************************************************/
/*  ai_agent_session.h                                                     */
/**************************************************************************/

#pragma once

#include "core/object/class_db.h"
#include "scene/main/node.h"

#include "editor/ai_component/agent/ai_agent_runner.h"
#include "editor/ai_component/context/ai_editor_context_provider.h"
#include "editor/ai_component/context/ai_project_tree_context_provider.h"
#include "editor/ai_component/providers/ai_openai_compatible_provider.h"
#include "editor/ai_component/storage/ai_conversation_store.h"

class AIAgentSession : public Node {
	GDCLASS(AIAgentSession, Node);

	String session_id;
	String title = "New Chat";
	Vector<AIAgentMessage> messages;
	AIAgentState state = AI_AGENT_STATE_IDLE;

	Ref<AIAgentRunner> runner;
	AIOpenAICompatibleProvider *provider = nullptr;
	Ref<AIConversationStore> store;
	Ref<AIProjectTreeContextProvider> project_tree_context;
	Ref<AIEditorContextProvider> editor_context;

	int active_assistant_index = -1;

	void _set_state(AIAgentState p_state);
	Array _collect_context();
	void _save();

	void _on_provider_response_started();
	void _on_provider_response_delta(const String &p_delta);
	void _on_provider_response_finished(const String &p_finish_reason);
	void _on_provider_request_failed(const String &p_message);

protected:
	static void _bind_methods();

public:
	AIAgentSession();

	void configure_provider(const AIProviderConfig &p_config);
	void send_user_message(const String &p_message);
	void cancel_request();
	void start_new_session();
	bool load_session(const String &p_session_id);
	Array get_messages_as_array() const;
	String get_session_id() const;
	String get_title() const;
	AIAgentState get_state() const;
	Array list_sessions() const;
};
