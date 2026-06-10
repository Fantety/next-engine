/**************************************************************************/
/*  ai_main_thread_dispatcher.cpp                                         */
/**************************************************************************/

#include "ai_main_thread_dispatcher.h"

#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/variant/variant.h"

Mutex AIMainThreadDispatcher::dispatch_mutex;
Vector<AIMainThreadDispatcher::DispatchItem> AIMainThreadDispatcher::dispatch_items;
uint64_t AIMainThreadDispatcher::next_dispatch_id = 0;

bool AIMainThreadDispatcher::_call_callable(const Callable &p_callable, const Array &p_arguments, Variant &r_result, String &r_error) {
	Vector<Variant> argument_storage;
	Vector<const Variant *> argument_ptrs;
	argument_storage.resize(p_arguments.size());
	argument_ptrs.resize(p_arguments.size());

	for (int i = 0; i < p_arguments.size(); i++) {
		argument_storage.write[i] = p_arguments[i];
		argument_ptrs.write[i] = &argument_storage[i];
	}

	Callable::CallError ce;
	p_callable.callp(argument_ptrs.is_empty() ? nullptr : const_cast<const Variant **>(argument_ptrs.ptr()), argument_ptrs.size(), r_result, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		r_error = Variant::get_callable_error_text(p_callable, argument_ptrs.is_empty() ? nullptr : const_cast<const Variant **>(argument_ptrs.ptr()), argument_ptrs.size(), ce);
		return false;
	}
	return true;
}

bool AIMainThreadDispatcher::_remove_queued_call(uint64_t p_item_id) {
	if (p_item_id == 0) {
		return false;
	}

	MutexLock lock(dispatch_mutex);
	for (int i = dispatch_items.size() - 1; i >= 0; i--) {
		if (dispatch_items[i].id == p_item_id) {
			dispatch_items.remove_at(i);
			return true;
		}
	}
	return false;
}

void AIMainThreadDispatcher::_execute_sync_call(uint64_t p_request_ptr) {
	SyncCallRequest *request = reinterpret_cast<SyncCallRequest *>(p_request_ptr);
	if (!request) {
		return;
	}

	request->success = _call_callable(request->callable, request->arguments, request->result, request->error);
	request->done.post();
}

Error AIMainThreadDispatcher::queue_call(const Callable &p_callable, const Array &p_arguments, uint64_t &r_item_id) {
	r_item_id = 0;
	CallQueue *message_queue = MessageQueue::get_main_singleton();
	if (!message_queue) {
		return ERR_UNAVAILABLE;
	}

	{
		MutexLock lock(dispatch_mutex);
		DispatchItem item;
		item.id = ++next_dispatch_id;
		item.callable = p_callable;
		item.arguments = p_arguments.duplicate(true);
		r_item_id = item.id;
		dispatch_items.push_back(item);
	}

	const Error err = message_queue->push_callable(callable_mp_static(&AIMainThreadDispatcher::flush_pending_calls));
	if (err != OK && _remove_queued_call(r_item_id)) {
		r_item_id = 0;
		return err;
	}
	return OK;
}

void AIMainThreadDispatcher::flush_pending_calls() {
	if (!Thread::is_main_thread()) {
		return;
	}

	while (true) {
		Vector<DispatchItem> items;
		{
			MutexLock lock(dispatch_mutex);
			if (dispatch_items.is_empty()) {
				return;
			}
			items = dispatch_items;
			dispatch_items.clear();
		}

		for (int i = 0; i < items.size(); i++) {
			Variant result;
			String error;
			if (!_call_callable(items[i].callable, items[i].arguments, result, error)) {
				ERR_PRINT("Failed to dispatch AI main-thread call: " + error + ".");
			}
		}
	}
}

bool AIMainThreadDispatcher::dispatch_sync(const Callable &p_callable, const Array &p_arguments, Variant &r_result, String &r_error, const Ref<AICancelToken> &p_cancel_token, int p_wait_usec) {
	r_result = Variant();
	r_error = String();

	if (Thread::is_main_thread()) {
		return _call_callable(p_callable, p_arguments, r_result, r_error);
	}

	if (!MessageQueue::get_main_singleton()) {
		r_error = "Main thread dispatch is not available.";
		return false;
	}

	if (p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested()) {
		r_error = p_cancel_token->get_cancel_message("Main thread dispatch cancelled.");
		return false;
	}

	SyncCallRequest request;
	request.callable = p_callable;
	request.arguments = p_arguments.duplicate(true);

	Array dispatcher_arguments;
	dispatcher_arguments.push_back(reinterpret_cast<uint64_t>(&request));

	uint64_t item_id = 0;
	const Error err = queue_call(callable_mp_static(&AIMainThreadDispatcher::_execute_sync_call), dispatcher_arguments, item_id);
	if (err != OK) {
		r_error = "Failed to schedule main thread dispatch.";
		return false;
	}

	while (!request.done.try_wait()) {
		if (p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested() && _remove_queued_call(item_id)) {
			r_error = p_cancel_token->get_cancel_message("Main thread dispatch cancelled.");
			return false;
		}

		if (OS::get_singleton()) {
			OS::get_singleton()->delay_usec(MAX(0, p_wait_usec));
		} else {
			Thread::yield();
		}
	}

	r_result = request.result;
	r_error = request.error;
	return request.success;
}
