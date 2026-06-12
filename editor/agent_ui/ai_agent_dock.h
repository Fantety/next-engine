/**************************************************************************/
/*  ai_agent_dock.h                                                        */
/**************************************************************************/

#pragma once

#include "editor/docks/editor_dock.h"

#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_bridge.h"
#include "editor/agent_ui/ai_change_review_panel.h"
#include "editor/agent_ui/ai_composer.h"
#include "editor/agent_ui/component/ai_message_list.h"
#include "editor/agent_ui/component/ai_todo_list_panel.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"

class ConfirmationDialog;
class HBoxContainer;
class ColorRect;
class AIRequirementFormDialog;
class ItemList;
class PopupPanel;
class VBoxContainer;

class AIAgentDock : public EditorDock {
	GDCLASS(AIAgentDock, EditorDock);

	OptionButton *session_selector = nullptr;
	Button *new_session_button = nullptr;
	Button *delete_session_button = nullptr;
	Button *mcp_status_button = nullptr;
	Button *skill_status_button = nullptr;
	PopupPanel *mcp_status_popup = nullptr;
	ItemList *mcp_status_list = nullptr;
	PopupPanel *skill_status_popup = nullptr;
	ItemList *skill_status_list = nullptr;
	ConfirmationDialog *delete_session_dialog = nullptr;
	ConfirmationDialog *tool_approval_dialog = nullptr;
	AIRequirementFormDialog *requirement_form_dialog = nullptr;
	AIChangeReviewPanel *change_review_panel = nullptr;
	AIMessageList *message_list = nullptr;
	AITodoListPanel *todo_panel = nullptr;
	HBoxContainer *request_status_row = nullptr;
	ColorRect *request_progress = nullptr;
	Label *token_usage_label = nullptr;
	AIComposer *composer = nullptr;
	Ref<AIAgentV1UIBridge> bridge;
	VBoxContainer *normal_panel = nullptr;
	String pending_delete_session_id;
	Dictionary pending_tool_approval;
	bool mcp_failure_toast_visible = false;
	bool session_list_refresh_queued = false;

	static inline AIAgentDock *singleton = nullptr;

	Ref<AIAgentV1UIBridge> _get_adapter();
	String _normalize_agent_profile_id(const String &p_agent_profile_id) const;
	bool _is_run_busy() const;
	void _send_to_agent_v1(const String &p_message, const String &p_model, const String &p_agent_profile_id, const Array &p_attachments, bool p_resume);
	void _send_requested(const String &p_message, const String &p_model, const String &p_agent_profile_id, const Array &p_attachments);
	void _agent_profile_selected(const String &p_agent_profile_id);
	void _cancel_requested();
	void _sessions_changed(const Array &p_sessions);
	void _active_session_changed(const Dictionary &p_session);
	void _messages_changed(const String &p_session_id, const Array &p_messages);
	void _todos_changed(const String &p_session_id, const Array &p_todos);
	void _run_state_changed(const Dictionary &p_state);
	void _permission_requested(const Dictionary &p_request);
	void _permission_resolved(const Dictionary &p_reply);
	void _mcp_status_changed(const Array &p_statuses, const Dictionary &p_summary);
	void _skill_status_changed(const Array &p_statuses, const Dictionary &p_summary);
	void _tool_approval_requested(const Dictionary &p_approval);
	void _settings_changed();
	void _next_marquee_settings_changed();
	void _mcp_settings_changed();
	void _mcp_status_pressed();
	void _skill_settings_changed();
	void _skill_status_pressed();
	void _new_session_pressed();
	void _delete_session_pressed();
	void _confirm_delete_session();
	void _confirm_tool_approval();
	void _reject_tool_approval();
	void _requirement_form_submitted(const Dictionary &p_answers);
	void _session_selected(int p_index);
	void _ensure_session();
	void _sync_composer_agent_profile();
	String _get_selected_session_id() const;
	void _queue_refresh_session_list();
	void _flush_session_list_refresh();
	void _refresh_session_list();
	void _select_current_session();
	void _reload_messages_from_session();
	void _reload_todos_from_session();
	void _refresh_todo_panel(const Array &p_todos);
	void _refresh_mcp_status_button();
	void _refresh_mcp_status_popup();
	void _refresh_skill_status_button();
	void _refresh_skill_status_popup();
	void _refresh_token_usage();
	void _ensure_request_progress_material();
	void _refresh_request_progress_material();
	void _clear_request_progress_material();
	String _format_token_count(int p_tokens) const;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIAgentDock();
	~AIAgentDock();
	static AIAgentDock *get_singleton();
};
