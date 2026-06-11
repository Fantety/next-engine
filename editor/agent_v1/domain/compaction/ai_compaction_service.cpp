/**************************************************************************/
/*  ai_compaction_service.cpp                                             */
/**************************************************************************/

#include "ai_compaction_service.h"

#include "editor/agent_v1/core/base/ai_id.h"
#include "editor/agent_v1/domain/projection/ai_token_estimator.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

void AICompactionService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_event_store", "store"), &AICompactionService::set_event_store);
	ClassDB::bind_method(D_METHOD("get_event_store"), &AICompactionService::get_event_store);
	ClassDB::bind_method(D_METHOD("set_projector", "projector"), &AICompactionService::set_projector);
	ClassDB::bind_method(D_METHOD("get_projector"), &AICompactionService::get_projector);
	ClassDB::bind_method(D_METHOD("set_context_source_registry", "registry"), &AICompactionService::set_context_source_registry);
	ClassDB::bind_method(D_METHOD("get_context_source_registry"), &AICompactionService::get_context_source_registry);
	ClassDB::bind_method(D_METHOD("set_context_epoch_service", "service"), &AICompactionService::set_context_epoch_service);
	ClassDB::bind_method(D_METHOD("get_context_epoch_service"), &AICompactionService::get_context_epoch_service);
	ClassDB::bind_method(D_METHOD("compact", "input"), &AICompactionService::compact);
	ClassDB::bind_method(D_METHOD("maybe_compact", "input"), &AICompactionService::maybe_compact);
}

Dictionary AICompactionService::_make_error_result(const AIError &p_error) {
	Dictionary result;
	result["success"] = false;
	result["error"] = p_error.to_dictionary();
	return result;
}

bool AICompactionService::_message_has_open_tool(const AISessionMessage &p_message) {
	if (p_message.type != AI_SESSION_MESSAGE_ASSISTANT) {
		return false;
	}
	for (int i = 0; i < p_message.content.size(); i++) {
		const AIAssistantContent &content = p_message.content[i];
		if (content.type == "tool" && (content.tool_state.status == AI_TOOL_STATUS_PENDING || content.tool_state.status == AI_TOOL_STATUS_RUNNING)) {
			return true;
		}
	}
	return false;
}

bool AICompactionService::_message_is_compaction_candidate(const AISessionMessage &p_message) {
	if (p_message.type != AI_SESSION_MESSAGE_USER && p_message.type != AI_SESSION_MESSAGE_ASSISTANT) {
		return false;
	}
	return !_message_has_open_tool(p_message);
}

int64_t AICompactionService::_estimate_messages(const Vector<AISessionMessage> &p_messages) {
	int64_t total = 0;
	for (int i = 0; i < p_messages.size(); i++) {
		total += AITokenEstimator::estimate_message_struct(p_messages[i]);
	}
	return total;
}

int64_t AICompactionService::_latest_compaction_seq(const Vector<AISessionMessage> &p_messages) {
	int64_t seq = 0;
	for (int i = 0; i < p_messages.size(); i++) {
		if (p_messages[i].type == AI_SESSION_MESSAGE_COMPACTION && p_messages[i].seq > seq) {
			seq = p_messages[i].seq;
		}
	}
	return seq;
}

String AICompactionService::_latest_compaction_summary(const Vector<AISessionMessage> &p_messages) {
	int64_t seq = 0;
	String summary;
	for (int i = 0; i < p_messages.size(); i++) {
		if (p_messages[i].type != AI_SESSION_MESSAGE_COMPACTION || p_messages[i].seq <= seq) {
			continue;
		}
		seq = p_messages[i].seq;
		summary = p_messages[i].text.strip_edges();
		if (summary.is_empty()) {
			summary = String(p_messages[i].metadata.get("summary", String())).strip_edges();
		}
	}
	return summary;
}

Vector<AISessionMessage> AICompactionService::_messages_after_compaction(const Vector<AISessionMessage> &p_messages, int64_t p_compaction_seq) {
	Vector<AISessionMessage> result;
	for (int i = 0; i < p_messages.size(); i++) {
		if (p_compaction_seq > 0 && p_messages[i].seq <= p_compaction_seq) {
			continue;
		}
		result.push_back(p_messages[i]);
	}
	return result;
}

Vector<AISessionMessage> AICompactionService::_select_compaction_input(const Vector<AISessionMessage> &p_messages) {
	Vector<AISessionMessage> candidates;
	for (int i = 0; i < p_messages.size(); i++) {
		if (_message_is_compaction_candidate(p_messages[i])) {
			candidates.push_back(p_messages[i]);
		}
	}

	if (candidates.size() <= 1) {
		return Vector<AISessionMessage>();
	}

	Vector<AISessionMessage> selected;
	const int keep_recent = 1;
	const int limit = MAX(0, candidates.size() - keep_recent);
	for (int i = 0; i < limit; i++) {
		selected.push_back(candidates[i]);
	}
	return selected;
}

Array AICompactionService::_message_ids(const Vector<AISessionMessage> &p_messages) {
	Array ids;
	for (int i = 0; i < p_messages.size(); i++) {
		ids.push_back(p_messages[i].id);
	}
	return ids;
}

String AICompactionService::_message_summary_line(const AISessionMessage &p_message) {
	if (p_message.type == AI_SESSION_MESSAGE_USER) {
		return "- User: " + p_message.text.strip_edges().left(240);
	}

	if (p_message.type != AI_SESSION_MESSAGE_ASSISTANT) {
		return String();
	}

	String text;
	for (int i = 0; i < p_message.content.size(); i++) {
		const AIAssistantContent &content = p_message.content[i];
		if (content.type == "text" && !content.text.strip_edges().is_empty()) {
			if (!text.is_empty()) {
				text += " ";
			}
			text += content.text.strip_edges();
		} else if (content.type == "tool") {
			if (!text.is_empty()) {
				text += " ";
			}
			text += "Tool " + content.name + " settled with status " + ai_tool_status_to_string(content.tool_state.status) + ".";
		}
	}
	if (text.is_empty()) {
		return "- Assistant: completed a turn.";
	}
	return "- Assistant: " + text.left(240);
}

void AICompactionService::_append_previous_summary_lines(const String &p_previous_summary, String &r_user_goal, String &r_decisions, String &r_tool_results) {
	const Vector<String> lines = p_previous_summary.split("\n", false);
	for (int i = 0; i < lines.size(); i++) {
		const String line = lines[i].strip_edges();
		if (line.begins_with("- User:")) {
			r_user_goal += line + "\n";
		} else if (line.begins_with("- Assistant:")) {
			if (line.contains("Tool ") && line.contains("settled with status")) {
				r_tool_results += line + "\n";
			} else {
				r_decisions += line + "\n";
			}
		} else if (line.begins_with("- Tool")) {
			r_tool_results += line + "\n";
		}
	}
}

String AICompactionService::_truncate_summary_to_budget(const String &p_summary, int64_t p_target_token_budget) {
	const int64_t minimum_summary_tokens = 128;
	const int64_t default_summary_tokens = 1024;
	const int64_t effective_tokens = p_target_token_budget > 0 ? MAX(p_target_token_budget, minimum_summary_tokens) : default_summary_tokens;
	const int64_t max_chars = effective_tokens * 4;
	if (p_summary.length() <= max_chars) {
		return p_summary;
	}

	const String marker = "\n\n[Summary truncated; original session events remain the source of truth.]";
	if (marker.length() >= max_chars) {
		return p_summary.left(max_chars);
	}
	return p_summary.left(max_chars - marker.length()).strip_edges() + marker;
}

String AICompactionService::_build_summary(const Vector<AISessionMessage> &p_messages, const String &p_previous_summary, int64_t p_target_token_budget) {
	String user_goal;
	String decisions;
	String tool_results;
	_append_previous_summary_lines(p_previous_summary, user_goal, decisions, tool_results);

	for (int i = 0; i < p_messages.size(); i++) {
		const String line = _message_summary_line(p_messages[i]);
		if (line.is_empty()) {
			continue;
		}
		if (p_messages[i].type == AI_SESSION_MESSAGE_USER) {
			user_goal += line + "\n";
		} else if (p_messages[i].type == AI_SESSION_MESSAGE_ASSISTANT) {
			bool has_tool = false;
			for (int j = 0; j < p_messages[i].content.size(); j++) {
				has_tool = has_tool || p_messages[i].content[j].type == "tool";
			}
			if (has_tool) {
				tool_results += line + "\n";
			} else {
				decisions += line + "\n";
			}
		}
	}

	if (user_goal.is_empty()) {
		user_goal = "- No explicit user goal was present in the compacted prefix.\n";
	}
	if (decisions.is_empty()) {
		decisions = "- No durable assistant decisions were present in the compacted prefix.\n";
	}
	if (tool_results.is_empty()) {
		tool_results = "- No completed tool result facts were present in the compacted prefix.\n";
	}

	String summary = "## Conversation Summary\n\n";
	summary += "### User Goal\n" + user_goal + "\n";
	summary += "### Decisions Made\n" + decisions + "\n";
	summary += "### Files / Artifacts Mentioned\n- See original event log for exact source artifacts.\n\n";
	summary += "### Tool Results\n" + tool_results + "\n";
	summary += "### Open Tasks\n- Continue from the un-compacted recent messages.\n\n";
	summary += "### Constraints and Preferences\n- Original session events remain the source of truth and were not deleted.\n";
	return _truncate_summary_to_budget(summary, p_target_token_budget);
}

bool AICompactionService::_project_history(const String &p_session_id, AIError &r_error) const {
	if (event_store.is_null() || projector.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "CompactionService dependencies are not wired.");
		return false;
	}
	projector->project_from_store(event_store, p_session_id, projector->get_projected_seq(p_session_id));
	r_error = AIError::none();
	return true;
}

void AICompactionService::_register_context_source(const String &p_session_id, int64_t p_seq, const String &p_compaction_id, const String &p_summary, int64_t p_token_before, int64_t p_token_after) {
	if (context_source_registry.is_null()) {
		return;
	}

	AISystemContextSource source;
	source.domain = "compaction:" + p_compaction_id;
	source.text = p_summary;
	source.required = true;
	source.available = true;
	source.priority = 250;
	source.metadata["session_id"] = p_session_id;
	source.metadata["seq"] = p_seq;
	source.metadata["compaction_id"] = p_compaction_id;
	source.metadata["source_id"] = "compaction:" + p_compaction_id;
	source.metadata["token_before"] = p_token_before;
	source.metadata["token_after"] = p_token_after;
	context_source_registry->clear_session_sources_with_domain_prefix_struct(p_session_id, "compaction");
	context_source_registry->add_session_source_struct(p_session_id, source);
}

void AICompactionService::set_event_store(const Ref<AIEventStore> &p_store) {
	event_store = p_store;
}

Ref<AIEventStore> AICompactionService::get_event_store() const {
	return event_store;
}

void AICompactionService::set_projector(const Ref<AISessionProjector> &p_projector) {
	projector = p_projector;
}

Ref<AISessionProjector> AICompactionService::get_projector() const {
	return projector;
}

void AICompactionService::set_context_source_registry(const Ref<AIContextSourceRegistry> &p_registry) {
	context_source_registry = p_registry;
}

Ref<AIContextSourceRegistry> AICompactionService::get_context_source_registry() const {
	return context_source_registry;
}

void AICompactionService::set_context_epoch_service(const Ref<AIContextEpochService> &p_service) {
	context_epoch_service = p_service;
}

Ref<AIContextEpochService> AICompactionService::get_context_epoch_service() const {
	return context_epoch_service;
}

bool AICompactionService::compact_struct(const String &p_session_id, const String &p_reason, int64_t p_target_token_budget, Dictionary &r_result, AIError &r_error) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Session id is required to compact.");
		return false;
	}
	if (!_project_history(session_id, r_error)) {
		return false;
	}

	const Vector<AISessionMessage> all_messages = projector->get_messages_struct(session_id);
	const String previous_summary = _latest_compaction_summary(all_messages);
	const Vector<AISessionMessage> active_messages = _messages_after_compaction(all_messages, _latest_compaction_seq(all_messages));
	const Vector<AISessionMessage> selected = _select_compaction_input(active_messages);
	if (selected.is_empty()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "No completed early history is available for compaction.");
		return false;
	}

	const int64_t token_before = _estimate_messages(active_messages);
	const String summary = _build_summary(selected, previous_summary, p_target_token_budget);
	const int64_t token_after = MAX(int64_t(1), int64_t(summary.length() / 4));
	const String compaction_id = AIId::make("compact");

	Dictionary data;
	data["compaction_id"] = compaction_id;
	data["message_id"] = AIId::make("compaction");
	data["session_id"] = session_id;
	data["reason"] = p_reason.strip_edges().is_empty() ? String("manual") : p_reason.strip_edges();
	data["target_token_budget"] = p_target_token_budget;
	data["input_message_ids"] = _message_ids(selected);
	data["summary"] = summary;
	data["token_before"] = token_before;
	data["token_after"] = token_after;
	data["state"] = "completed";

	AIEventRow row;
	String event_error;
	if (!event_store->append(session_id, AIDomainEventTypes::compaction_ended(), data, false, row, event_error)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, event_error);
		return false;
	}
	if (projector.is_valid()) {
		projector->project(row);
	}

	_register_context_source(session_id, row.seq, compaction_id, summary, token_before, token_after);
	if (context_epoch_service.is_valid()) {
		AIError replacement_error;
		if (!context_epoch_service->request_replacement_struct(session_id, row.seq, replacement_error)) {
			r_error = replacement_error;
			return false;
		}
	}

	r_result = data.duplicate(true);
	r_result["seq"] = row.seq;
	r_result["success"] = true;
	r_error = AIError::none();
	return true;
}

bool AICompactionService::maybe_compact_struct(const String &p_session_id, const String &p_reason, int64_t p_target_token_budget, bool &r_compacted, Dictionary &r_result, AIError &r_error) {
	r_compacted = false;
	r_result = Dictionary();
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Session id is required to compact.");
		return false;
	}
	if (p_target_token_budget <= 0) {
		r_error = AIError::none();
		return true;
	}
	if (!_project_history(session_id, r_error)) {
		return false;
	}

	const Vector<AISessionMessage> all_messages = projector->get_messages_struct(session_id);
	const Vector<AISessionMessage> active_messages = _messages_after_compaction(all_messages, _latest_compaction_seq(all_messages));
	if (_estimate_messages(active_messages) <= p_target_token_budget) {
		r_error = AIError::none();
		return true;
	}
	if (_select_compaction_input(active_messages).is_empty()) {
		r_error = AIError::none();
		return true;
	}

	if (!compact_struct(session_id, p_reason.strip_edges().is_empty() ? String("auto") : p_reason, p_target_token_budget, r_result, r_error)) {
		return false;
	}
	r_compacted = true;
	return true;
}

Dictionary AICompactionService::compact(const Dictionary &p_input) {
	Dictionary result;
	AIError error;
	if (!compact_struct(p_input.get("session_id", p_input.get("sessionID", String())), p_input.get("reason", "manual"), int64_t(p_input.get("target_token_budget", p_input.get("targetTokenBudget", 0))), result, error)) {
		return _make_error_result(error);
	}
	return result;
}

Dictionary AICompactionService::maybe_compact(const Dictionary &p_input) {
	Dictionary result;
	AIError error;
	bool compacted = false;
	if (!maybe_compact_struct(p_input.get("session_id", p_input.get("sessionID", String())), p_input.get("reason", "auto"), int64_t(p_input.get("target_token_budget", p_input.get("targetTokenBudget", 0))), compacted, result, error)) {
		return _make_error_result(error);
	}
	result["success"] = true;
	result["compacted"] = compacted;
	return result;
}
