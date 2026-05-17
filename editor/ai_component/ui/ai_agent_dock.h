/**************************************************************************/
/*  ai_agent_dock.h                                                        */
/**************************************************************************/

#pragma once

#include "editor/docks/editor_dock.h"

#include "editor/ai_component/agent/ai_agent_session.h"
#include "editor/ai_component/ui/ai_composer.h"
#include "editor/ai_component/ui/ai_message_list.h"
#include "scene/gui/button.h"
#include "scene/gui/option_button.h"
#include "scene/gui/progress_bar.h"

class ConfirmationDialog;

class AIAgentDock : public EditorDock {
	GDCLASS(AIAgentDock, EditorDock);

	OptionButton *session_selector = nullptr;
	Button *new_session_button = nullptr;
	Button *delete_session_button = nullptr;
	ConfirmationDialog *delete_session_dialog = nullptr;
	AIMessageList *message_list = nullptr;
	ProgressBar *request_progress = nullptr;
	AIComposer *composer = nullptr;
	AIAgentSession *session = nullptr;
	String pending_delete_session_id;

	static inline AIAgentDock *singleton = nullptr;

	void _send_requested(const String &p_message, const String &p_model);
	void _cancel_requested();
	void _message_added(const Dictionary &p_message);
	void _message_updated(int p_index, const Dictionary &p_message);
	void _message_removed(int p_index);
	void _state_changed(int p_state);
	void _settings_changed();
	void _new_session_pressed();
	void _delete_session_pressed();
	void _confirm_delete_session();
	void _session_selected(int p_index);
	void _ensure_session();
	String _get_selected_session_id() const;
	void _refresh_session_list();
	void _select_current_session();
	void _reload_messages_from_session();
	AIProviderConfig _get_provider_config(const String &p_model_id) const;

protected:
	static void _bind_methods();

public:
	AIAgentDock();
	static AIAgentDock *get_singleton();
};
