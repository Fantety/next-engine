/**************************************************************************/
/*  ai_agent_dock.h                                                        */
/**************************************************************************/

#pragma once

#include "editor/docks/editor_dock.h"

#include "editor/ai_component/agent/ai_agent_session.h"
#include "editor/ai_component/ui/ai_composer.h"
#include "editor/ai_component/ui/ai_message_list.h"

class AIAgentDock : public EditorDock {
	GDCLASS(AIAgentDock, EditorDock);

	AIMessageList *message_list = nullptr;
	AIComposer *composer = nullptr;
	AIAgentSession *session = nullptr;

	static inline AIAgentDock *singleton = nullptr;

	void _send_requested(const String &p_message, const String &p_model);
	void _cancel_requested();
	void _message_added(const Dictionary &p_message);
	void _message_updated(int p_index, const Dictionary &p_message);
	void _state_changed(int p_state);
	void _settings_changed();
	void _ensure_session();
	AIProviderConfig _get_provider_config(const String &p_model) const;

protected:
	static void _bind_methods();

public:
	AIAgentDock();
	static AIAgentDock *get_singleton();
};
