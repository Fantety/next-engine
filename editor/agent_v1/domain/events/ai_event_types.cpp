/**************************************************************************/
/*  ai_event_types.cpp                                                    */
/**************************************************************************/

#include "ai_event_types.h"

#include "core/variant/variant.h"

String AIDomainEventTypes::prompt_admitted() {
	return "session.next.prompt.admitted";
}

String AIDomainEventTypes::prompt_promoted() {
	return "session.next.prompt.promoted";
}

String AIDomainEventTypes::step_started() {
	return "session.next.step.started";
}

String AIDomainEventTypes::step_ended() {
	return "session.next.step.ended";
}

String AIDomainEventTypes::step_failed() {
	return "session.next.step.failed";
}

String AIDomainEventTypes::text_started() {
	return "session.next.text.started";
}

String AIDomainEventTypes::text_delta() {
	return "session.next.text.delta";
}

String AIDomainEventTypes::text_ended() {
	return "session.next.text.ended";
}

String AIDomainEventTypes::reasoning_started() {
	return "session.next.reasoning.started";
}

String AIDomainEventTypes::reasoning_delta() {
	return "session.next.reasoning.delta";
}

String AIDomainEventTypes::reasoning_ended() {
	return "session.next.reasoning.ended";
}

String AIDomainEventTypes::tool_input_started() {
	return "session.next.tool.input.started";
}

String AIDomainEventTypes::tool_input_delta() {
	return "session.next.tool.input.delta";
}

String AIDomainEventTypes::tool_input_ended() {
	return "session.next.tool.input.ended";
}

String AIDomainEventTypes::tool_called() {
	return "session.next.tool.called";
}

String AIDomainEventTypes::tool_progress() {
	return "session.next.tool.progress";
}

String AIDomainEventTypes::tool_success() {
	return "session.next.tool.success";
}

String AIDomainEventTypes::tool_failed() {
	return "session.next.tool.failed";
}

String AIDomainEventTypes::context_updated() {
	return "session.next.context.updated";
}

String AIDomainEventTypes::compaction_started() {
	return "session.next.compaction.started";
}

String AIDomainEventTypes::compaction_delta() {
	return "session.next.compaction.delta";
}

String AIDomainEventTypes::compaction_ended() {
	return "session.next.compaction.ended";
}

String AIDomainEventTypes::interrupt_requested() {
	return "session.next.interrupt.requested";
}

String AIDomainEventTypes::permission_asked() {
	return "permission.v2.asked";
}

String AIDomainEventTypes::permission_replied() {
	return "permission.v2.replied";
}

bool AIDomainEventTypes::is_live_only_event(const String &p_type) {
	const String type = p_type.strip_edges();
	return type == text_delta() ||
			type == reasoning_delta() ||
			type == tool_input_delta() ||
			type == compaction_delta();
}

bool AIDomainEventTypes::is_durable_event(const String &p_type) {
	return !p_type.strip_edges().is_empty() && !is_live_only_event(p_type);
}

bool AIDomainEventTypes::is_session_event(const String &p_type) {
	return p_type.begins_with("session.next.");
}

bool AIDomainEventTypes::is_permission_event(const String &p_type) {
	return p_type == permission_asked() || p_type == permission_replied();
}

Dictionary AIEventRow::to_dictionary() const {
	Dictionary result;
	result["id"] = id;
	result["aggregate_id"] = aggregate_id;
	result["seq"] = seq;
	result["type"] = type;
	result["data"] = data;
	result["timestamp"] = timestamp;
	result["live_only"] = live_only;
	return result;
}

AIEventRow AIEventRow::from_dictionary(const Dictionary &p_dict) {
	AIEventRow result;
	result.id = p_dict.get("id", String());
	result.aggregate_id = p_dict.get("aggregate_id", p_dict.get("aggregateID", String()));
	result.seq = int64_t(p_dict.get("seq", 0));
	result.type = p_dict.get("type", String());
	if (p_dict.get("data", Variant()).get_type() == Variant::DICTIONARY) {
		result.data = Dictionary(p_dict.get("data", Dictionary())).duplicate(true);
	}
	result.timestamp = uint64_t(p_dict.get("timestamp", 0));
	result.live_only = bool(p_dict.get("live_only", p_dict.get("liveOnly", AIDomainEventTypes::is_live_only_event(result.type))));
	return result;
}
