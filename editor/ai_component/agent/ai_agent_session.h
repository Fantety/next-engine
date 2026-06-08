/**************************************************************************/
/*  ai_agent_session.h                                                     */
/**************************************************************************/

#pragma once

#include "core/object/class_db.h"
#include "core/os/mutex.h"
#include "core/os/safe_binary_mutex.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/templates/hash_map.h"

#include "editor/ai_component/agent/ai_agent_runtime.h"
#include "editor/ai_component/agent/ai_agent_runtime_runner.h"
#include "editor/ai_component/agent/ai_main_agent.h"
#include "editor/ai_component/agent/ai_session_base.h"
#include "editor/ai_component/context/ai_best_practices_context_provider.h"
#include "editor/ai_component/context/ai_editor_context_provider.h"
#include "editor/ai_component/context/ai_project_tree_context_provider.h"
#include "editor/ai_component/providers/ai_openai_runtime_client.h"
#include "editor/ai_component/rules/ai_rules_context_provider.h"
#include "editor/ai_component/storage/ai_conversation_store.h"
#include "editor/ai_component/skills/ai_skill_context_provider.h"
#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/ai_tool_call.h"
#include "editor/ai_component/tools/ai_tool_registry.h"

class AIAgentSession : public AISessionBase {
	GDCLASS(AIAgentSession, AISessionBase);

	String session_id;
	String title = "New Chat";
	Vector<AIAgentMessage> messages;
	AIAgentState state = AI_AGENT_STATE_IDLE;
	AIAgentProfile agent_profile;
	Dictionary token_usage;
	Dictionary pending_tool_approval;
	bool pending_tool_runtime_reload = false;
	uint64_t turn_generation = 0;

	struct ApprovedToolThreadParams {
		AIAgentSession *session = nullptr;
		Ref<AITool> tool;
		AIToolCall call;
		String agent_profile_id;
		bool review_changes = false;
		String session_id;
		uint64_t turn_generation = 0;
	};

	Ref<AIMainAgent> main_agent;
	Ref<AIAgentRuntime> runtime;
	Ref<AIAgentRuntimeRunner> runtime_runner;
	Ref<AIOpenAICompatibleRuntimeClient> runtime_client;
	Ref<AIToolRegistry> tool_registry;
	Ref<AIConversationStore> store;
	Ref<AIProjectTreeContextProvider> project_tree_context;
	Ref<AIEditorContextProvider> editor_context;
	Ref<AIBestPracticesContextProvider> best_practices_context;
	Ref<AIRulesContextProvider> rules_context;
	Ref<AISkillIndexContextProvider> skill_context;
	Thread approved_tool_thread;
	SafeFlag approved_tool_running;
	Semaphore approved_tool_finished;
	Mutex approved_tool_result_mutex;
	AIToolCall approved_tool_call;
	AIToolResult approved_tool_result;
	String approved_tool_result_session_id;
	uint64_t approved_tool_result_turn_generation = 0;
	bool approved_tool_result_available = false;
	bool shutting_down = false;

	int active_assistant_index = -1;
	int runtime_base_message_count = 0;
	int runtime_progress_message_count = 0;
	HashMap<int, int> runtime_to_local_message_indices;

	void _configure_tool_runtime();
	void _sync_editor_context_profile();
	void _mcp_tools_changed();
	void _load_initial_session();
	void _apply_runtime_result(const AIAgentRuntimeResult &p_result);
	void _set_state(AIAgentState p_state);
	Array _collect_context();
	void _save();
	void _recalculate_token_usage();
	void _append_tool_result_message(const AIToolCall &p_call, const AIToolResult &p_tool_result);
	bool _update_tool_call_status(const AIToolCall &p_call);
	bool _start_runtime_turn();
	bool _is_busy() const;
	void _wait_for_approved_tool_thread();
	static void _approved_tool_thread_func(void *p_userdata);
	void _set_approved_tool_result(const AIToolCall &p_call, const AIToolResult &p_result, const String &p_session_id, uint64_t p_turn_generation);
	void _on_approved_tool_finished(const String &p_session_id, uint64_t p_turn_generation);

	void _on_provider_request_failed(const String &p_message);
	void _on_runtime_finished();
	void _on_runtime_message_added(int p_index, const Dictionary &p_message);
	void _on_runtime_message_updated(int p_index, const Dictionary &p_message);
	void _remove_message_at(int p_index);

protected:
	static void _bind_methods();

public:
	AIAgentSession();
	~AIAgentSession();

	void configure_provider(const AIProviderConfig &p_config);
	void set_agent_profile_id(const String &p_profile_id);
	String get_agent_profile_id() const;
	Ref<AIMainAgent> get_main_agent() const;
	Ref<AIAgentRuntime> get_agent_runtime() const;
	Ref<AIAgentRuntimeRunner> get_agent_runtime_runner() const;
	Ref<AIToolRegistry> get_tool_registry() const;
	bool is_tool_runtime_available() const;
	void reload_tool_runtime();
	void send_user_message(const String &p_message);
	void send_user_message(const String &p_message, const Array &p_attachments);
	void cancel_request();
	void start_new_session();
	bool load_session(const String &p_session_id);
	bool delete_session(const String &p_session_id);
	bool approve_pending_tool();
	bool reject_pending_tool();
	bool submit_pending_requirement_form(const Dictionary &p_answers);
	Dictionary get_pending_tool_approval() const;
	Array get_messages_as_array() const;
	String get_session_id() const;
	String get_title() const;
	AIAgentState get_state() const;
	Dictionary get_token_usage() const;
	Array list_sessions() const;
	void replace_messages_for_test(const Vector<AIAgentMessage> &p_messages, int p_active_assistant_index);
	void apply_runtime_result_for_test(const AIAgentRuntimeResult &p_result);
	void add_runtime_message_for_test(int p_index, const AIAgentMessage &p_message);
	void update_runtime_message_for_test(int p_index, const AIAgentMessage &p_message);
	static Dictionary make_user_message_for_test(const String &p_message, const Array &p_attachments);
	Error save_for_test();
	void wait_for_approved_tool_for_test();
	void set_conversation_project_scope_for_test(const String &p_project_scope_key);
	Ref<AIConversationStore> get_conversation_store_for_test() const;
};
