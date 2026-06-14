/**************************************************************************/
/*  ai_editor_tool_service.h                                              */
/**************************************************************************/

#pragma once

#include "core/object/callable_mp.h"
#include "core/object/message_queue.h"
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"
#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

class Node;

class AIV1EditorToolService : public RefCounted {
	GDCLASS(AIV1EditorToolService, RefCounted);

protected:
	static constexpr uint32_t MAIN_THREAD_DISPATCH_WAIT_USEC = 1000;
	static constexpr int MAIN_THREAD_DISPATCH_FLUSH_BUDGET = 8;

	struct MainThreadDispatchItem {
		uint64_t id = 0;
		Callable callable;
		Variant argument;
	};

	static Mutex main_thread_dispatch_mutex;
	static Vector<MainThreadDispatchItem> main_thread_dispatch_items;
	static uint64_t main_thread_dispatch_next_id;
	static bool main_thread_dispatch_flush_queued;

	static void _bind_methods();
	static Error _queue_main_thread_dispatch(const Callable &p_callable, const Variant &p_argument, uint64_t &r_item_id);
	static bool _remove_queued_main_thread_dispatch(uint64_t p_item_id);
	static Error _schedule_main_thread_dispatch_flush(bool p_next_process_frame);

	template <typename TResult, typename TRequest, typename TService>
	static TResult _dispatch_main_thread_request(TRequest &r_request, TService *p_dispatcher, void (TService::*p_execute_request)(uint64_t), Mutex &r_request_mutex, const String &p_schedule_error) {
		if (Thread::is_main_thread()) {
			(p_dispatcher->*p_execute_request)(reinterpret_cast<uint64_t>(&r_request));
			return r_request.result;
		}

		MutexLock lock(r_request_mutex);
		if (!MessageQueue::get_main_singleton()) {
			TResult result;
			result.error = "Main thread dispatch is not available.";
			return result;
		}

		Ref<AIV1ToolExecutionState> execution_context = AIV1ToolExecutionState::get_current();
		if (execution_context.is_valid() && execution_context->is_cancel_requested()) {
			TResult result;
			result.error = "Tool execution cancelled.";
			return result;
		}

		Variant request_ptr = reinterpret_cast<uint64_t>(&r_request);
		uint64_t dispatch_item_id = 0;
		Error err = _queue_main_thread_dispatch(callable_mp(p_dispatcher, p_execute_request), request_ptr, dispatch_item_id);
		if (err != OK) {
			TResult result;
			result.error = p_schedule_error;
			return result;
		}

		while (!r_request.done.try_wait()) {
			if (execution_context.is_valid() && execution_context->is_cancel_requested() && dispatch_item_id != 0 && _remove_queued_main_thread_dispatch(dispatch_item_id)) {
				TResult result;
				result.error = "Tool execution cancelled.";
				return result;
			}

			if (OS::get_singleton()) {
				OS::get_singleton()->delay_usec(MAIN_THREAD_DISPATCH_WAIT_USEC);
			} else {
				Thread::yield();
			}
		}
		return r_request.result;
	}

	Node *_get_edited_scene(String &r_error) const;
	Node *_resolve_node_path(Node *p_scene_root, const String &p_path, bool p_allow_root, String &r_error, bool p_disallow_property_subpaths = false, const String &p_not_found_error = String()) const;
	bool _ensure_project_parent_directory(const String &p_path, const String &p_resource_label, String &r_error) const;
	bool _ensure_editor_filesystem_parent_directory(const String &p_path, const String &p_resource_label, String &r_error) const;
	bool _save_current_scene_with_packed_scene_main_thread(Node *p_scene, const String &p_unsaved_error, String &r_saved_path, String &r_error) const;
	bool _save_current_scene_with_editor_main_thread(Node *p_scene, const String &p_unsaved_error, String &r_saved_path, String &r_error) const;
	void _refresh_file_system(const String &p_path) const;
	void _update_scene_tree() const;

public:
	static void flush_pending_main_thread_dispatches_for_wait();
	static int get_main_thread_dispatch_flush_budget_for_test();
	static int get_pending_main_thread_dispatch_count_for_test();
	static Error queue_main_thread_dispatch_for_test(const Callable &p_callable, const Variant &p_argument, uint64_t &r_item_id);
	static Node *resolve_scene_node_path_for_test(Node *p_scene_root, const String &p_path, bool p_allow_root, String &r_error);
};
