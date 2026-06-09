/**************************************************************************/
/*  ai_tool_execution_context.cpp                                          */
/**************************************************************************/

#include "ai_tool_execution_context.h"

thread_local Ref<AIToolExecutionContext> AIToolExecutionContext::current;

void AIToolExecutionContext::_bind_methods() {
}

void AIToolExecutionContext::set_current(const Ref<AIToolExecutionContext> &p_context) {
	current = p_context;
}

Ref<AIToolExecutionContext> AIToolExecutionContext::get_current() {
	return current;
}

void AIToolExecutionContext::clear_current() {
	current.unref();
}

void AIToolExecutionContext::set_session_id(const String &p_session_id) {
	session_id = p_session_id;
}

String AIToolExecutionContext::get_session_id() const {
	return session_id;
}

void AIToolExecutionContext::set_agent_profile_id(const String &p_agent_profile_id) {
	agent_profile_id = p_agent_profile_id;
}

String AIToolExecutionContext::get_agent_profile_id() const {
	return agent_profile_id;
}

void AIToolExecutionContext::set_tool_call_id(const String &p_tool_call_id) {
	tool_call_id = p_tool_call_id;
}

String AIToolExecutionContext::get_tool_call_id() const {
	return tool_call_id;
}

void AIToolExecutionContext::set_review_changes(bool p_review_changes) {
	review_changes = p_review_changes;
}

bool AIToolExecutionContext::should_review_changes() const {
	return review_changes;
}

void AIToolExecutionContext::request_cancel() {
	cancel_requested.set();
}

void AIToolExecutionContext::clear_cancel_request() {
	cancel_requested.clear();
}

bool AIToolExecutionContext::is_cancel_requested() const {
	return cancel_requested.is_set();
}
