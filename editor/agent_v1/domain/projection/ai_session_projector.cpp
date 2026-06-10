/**************************************************************************/
/*  ai_session_projector.cpp                                              */
/**************************************************************************/

#include "ai_session_projector.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

static Dictionary _ai_projector_dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return p_value;
	}
	return Dictionary();
}

static Array _ai_projector_array_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::ARRAY) {
		return p_value;
	}
	return Array();
}

static String _ai_projector_get_string(const Dictionary &p_dict, const String &p_key_a, const String &p_key_b = String(), const String &p_key_c = String(), const String &p_fallback = String()) {
	if (!p_key_a.is_empty() && p_dict.has(p_key_a)) {
		return p_dict[p_key_a];
	}
	if (!p_key_b.is_empty() && p_dict.has(p_key_b)) {
		return p_dict[p_key_b];
	}
	if (!p_key_c.is_empty() && p_dict.has(p_key_c)) {
		return p_dict[p_key_c];
	}
	return p_fallback;
}

static uint64_t _ai_projector_get_time(const Dictionary &p_data, const AIEventRow &p_row) {
	if (p_data.has("time_created")) {
		return uint64_t(p_data["time_created"]);
	}
	if (p_data.has("timeCreated")) {
		return uint64_t(p_data["timeCreated"]);
	}
	if (p_data.has("timestamp")) {
		return uint64_t(p_data["timestamp"]);
	}
	return p_row.timestamp;
}

static String _ai_projector_assistant_message_id(const Dictionary &p_data) {
	return _ai_projector_get_string(p_data, "assistant_message_id", "assistantMessageID", "message_id", p_data.get("messageID", p_data.get("id", String())));
}

static String _ai_projector_call_id(const Dictionary &p_data) {
	return _ai_projector_get_string(p_data, "call_id", "callID", "id");
}

static Dictionary _ai_projector_provider_dict(const Dictionary &p_data) {
	return _ai_projector_dictionary_from_variant(p_data.get("provider", Dictionary())).duplicate(true);
}

static Dictionary _ai_projector_provider_metadata(const Dictionary &p_data) {
	const Dictionary provider = _ai_projector_provider_dict(p_data);
	if (provider.has("metadata") && provider["metadata"].get_type() == Variant::DICTIONARY) {
		return Dictionary(provider["metadata"]).duplicate(true);
	}
	return _ai_projector_dictionary_from_variant(p_data.get("provider_metadata", p_data.get("providerMetadata", Dictionary()))).duplicate(true);
}

static PackedStringArray _ai_projector_output_paths(const Dictionary &p_data) {
	if (p_data.get("output_paths", Variant()).get_type() == Variant::PACKED_STRING_ARRAY) {
		return p_data.get("output_paths", PackedStringArray());
	}
	if (p_data.get("outputPaths", Variant()).get_type() == Variant::PACKED_STRING_ARRAY) {
		return p_data.get("outputPaths", PackedStringArray());
	}

	PackedStringArray paths;
	const Array array = _ai_projector_array_from_variant(p_data.get("output_paths", p_data.get("outputPaths", Array())));
	for (int i = 0; i < array.size(); i++) {
		paths.push_back(String(array[i]));
	}
	return paths;
}

static AIError _ai_projector_error_from_data(const Dictionary &p_data, const String &p_default_message) {
	const Dictionary error_dict = _ai_projector_dictionary_from_variant(p_data.get("error", Dictionary()));
	if (error_dict.is_empty()) {
		return AIError::make(AI_ERROR_INTERNAL, p_default_message);
	}

	const String type = error_dict.get("type", error_dict.get("kind", "unknown"));
	const String message = error_dict.get("message", p_default_message);
	Dictionary details = error_dict.duplicate(true);
	details.erase("type");
	details.erase("kind");
	details.erase("message");
	return AIError::make(AIError::string_to_kind(type), message, details);
}

void AISessionProjector::_bind_methods() {
	ClassDB::bind_method(D_METHOD("project_event", "event"), &AISessionProjector::project_event);
	ClassDB::bind_method(D_METHOD("project_events", "events"), &AISessionProjector::project_events);
	ClassDB::bind_method(D_METHOD("project_from_store", "store", "session_id", "after_seq"), &AISessionProjector::project_from_store, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("rebuild_from_store", "store", "session_id"), &AISessionProjector::rebuild_from_store);
	ClassDB::bind_method(D_METHOD("get_inputs", "session_id"), &AISessionProjector::get_inputs);
	ClassDB::bind_method(D_METHOD("get_messages", "session_id"), &AISessionProjector::get_messages);
	ClassDB::bind_method(D_METHOD("get_context_epoch", "session_id"), &AISessionProjector::get_context_epoch);
	ClassDB::bind_method(D_METHOD("get_projected_seq", "session_id"), &AISessionProjector::get_projected_seq);
	ClassDB::bind_method(D_METHOD("mark_pending_tools_interrupted", "session_id", "reason"), &AISessionProjector::mark_pending_tools_interrupted, DEFVAL("Tool execution interrupted"));
	ClassDB::bind_method(D_METHOD("clear_session", "session_id"), &AISessionProjector::clear_session);
	ClassDB::bind_method(D_METHOD("clear"), &AISessionProjector::clear);
}

int AISessionProjector::_find_input_index(const Vector<AISessionInput> &p_inputs, const String &p_id) {
	for (int i = 0; i < p_inputs.size(); i++) {
		if (p_inputs[i].id == p_id) {
			return i;
		}
	}
	return -1;
}

int AISessionProjector::_find_message_index(const Vector<AISessionMessage> &p_messages, const String &p_id) {
	for (int i = 0; i < p_messages.size(); i++) {
		if (p_messages[i].id == p_id) {
			return i;
		}
	}
	return -1;
}

int AISessionProjector::_find_tool_content_index(const AISessionMessage &p_message, const String &p_call_id) {
	for (int i = 0; i < p_message.content.size(); i++) {
		const AIAssistantContent &content = p_message.content[i];
		if (content.type == "tool" && content.id == p_call_id) {
			return i;
		}
	}
	return -1;
}

String AISessionProjector::_fallback_message_id(const String &p_prefix, int64_t p_seq) {
	return p_prefix + "_" + String::num_int64(p_seq);
}

int AISessionProjector::_ensure_assistant_message_locked(const String &p_session_id, int64_t p_seq, const String &p_message_id, const Dictionary &p_metadata, uint64_t p_time_created) {
	Vector<AISessionMessage> &messages = messages_by_session[p_session_id];
	const int existing_index = _find_message_index(messages, p_message_id);
	if (existing_index >= 0) {
		if (!p_metadata.is_empty()) {
			messages.write[existing_index].metadata = p_metadata.duplicate(true);
		}
		return existing_index;
	}

	messages.push_back(AISessionMessage::assistant_shell(p_session_id, p_seq, p_message_id, p_metadata, p_time_created));
	return messages.size() - 1;
}

int AISessionProjector::_ensure_tool_content_locked(AISessionMessage &r_message, const String &p_call_id, const String &p_tool_name, const AIToolState &p_initial_state, const Dictionary &p_provider_metadata) {
	const int existing_index = _find_tool_content_index(r_message, p_call_id);
	if (existing_index >= 0) {
		return existing_index;
	}

	r_message.content.push_back(AIAssistantContent::tool_content(p_call_id, p_tool_name, p_initial_state, p_provider_metadata));
	return r_message.content.size() - 1;
}

void AISessionProjector::_project_row_locked(const AIEventRow &p_row) {
	if (p_row.live_only || !AIDomainEventTypes::is_durable_event(p_row.type)) {
		return;
	}

	const String session_id = p_row.aggregate_id;
	if (session_id.is_empty()) {
		return;
	}

	const Dictionary data = p_row.data;
	const String type = p_row.type;
	const uint64_t time_created = _ai_projector_get_time(data, p_row);

	if (type == AIDomainEventTypes::prompt_admitted()) {
		AISessionInput input;
		input.admitted_seq = p_row.seq;
		input.id = _ai_projector_get_string(data, "id", "message_id", "messageID", _fallback_message_id("user", p_row.seq));
		input.session_id = _ai_projector_get_string(data, "session_id", "sessionID", String(), session_id);
		input.prompt = AIPrompt::from_dictionary(_ai_projector_dictionary_from_variant(data.get("prompt", Dictionary())));
		input.delivery = ai_session_input_delivery_from_string(data.get("delivery", "queue"));
		input.time_created = time_created;

		Vector<AISessionInput> &inputs = inputs_by_session[session_id];
		if (_find_input_index(inputs, input.id) < 0) {
			inputs.push_back(input);
		}
	} else if (type == AIDomainEventTypes::prompt_promoted()) {
		const String input_id = _ai_projector_get_string(data, "id", "message_id", "messageID", _fallback_message_id("user", p_row.seq));
		Vector<AISessionInput> &inputs = inputs_by_session[session_id];
		const int input_index = _find_input_index(inputs, input_id);
		AIPrompt prompt = AIPrompt::from_dictionary(_ai_projector_dictionary_from_variant(data.get("prompt", Dictionary())));
		uint64_t promoted_time = time_created;
		if (input_index >= 0) {
			inputs.write[input_index].promoted_seq = p_row.seq;
			if (prompt.text.is_empty() && prompt.files.is_empty() && prompt.agents.is_empty() && prompt.references.is_empty()) {
				prompt = inputs[input_index].prompt;
			}
			if (promoted_time == 0) {
				promoted_time = inputs[input_index].time_created;
			}
		}

		Vector<AISessionMessage> &messages = messages_by_session[session_id];
		const String message_id = _ai_projector_get_string(data, "message_id", "messageID", "id", input_id);
		if (_find_message_index(messages, message_id) < 0) {
			messages.push_back(AISessionMessage::user_message(session_id, p_row.seq, message_id, prompt, promoted_time));
		}
	} else if (type == AIDomainEventTypes::step_started()) {
		Dictionary step_metadata = data.duplicate(true);
		step_metadata["status"] = "running";
		const String assistant_id = _ai_projector_assistant_message_id(data);
		_ensure_assistant_message_locked(session_id, p_row.seq, assistant_id.is_empty() ? _fallback_message_id("assistant", p_row.seq) : assistant_id, step_metadata, time_created);
	} else if (type == AIDomainEventTypes::step_ended() || type == AIDomainEventTypes::step_failed()) {
		Dictionary step_metadata = data.duplicate(true);
		step_metadata["status"] = type == AIDomainEventTypes::step_failed() ? "failed" : "ended";
		const String assistant_id = _ai_projector_assistant_message_id(data);
		if (!assistant_id.is_empty()) {
			const int message_index = _ensure_assistant_message_locked(session_id, p_row.seq, assistant_id, Dictionary(), time_created);
			messages_by_session[session_id].write[message_index].metadata = step_metadata;
		}
	} else if (type == AIDomainEventTypes::text_ended()) {
		const String assistant_id = _ai_projector_assistant_message_id(data);
		const String message_id = assistant_id.is_empty() ? _fallback_message_id("assistant", p_row.seq) : assistant_id;
		const int message_index = _ensure_assistant_message_locked(session_id, p_row.seq, message_id, Dictionary(), time_created);
		const String content_id = _ai_projector_get_string(data, "content_id", "contentID", "id");
		const String text = data.get("text", data.get("content", String()));
		messages_by_session[session_id].write[message_index].content.push_back(AIAssistantContent::text_content(text, content_id));
	} else if (type == AIDomainEventTypes::reasoning_ended()) {
		const String assistant_id = _ai_projector_assistant_message_id(data);
		const String message_id = assistant_id.is_empty() ? _fallback_message_id("assistant", p_row.seq) : assistant_id;
		const int message_index = _ensure_assistant_message_locked(session_id, p_row.seq, message_id, Dictionary(), time_created);
		const String content_id = _ai_projector_get_string(data, "content_id", "contentID", "id");
		const String text = data.get("text", data.get("content", String()));
		messages_by_session[session_id].write[message_index].content.push_back(AIAssistantContent::reasoning_content(text, _ai_projector_provider_metadata(data), content_id));
	} else if (type == AIDomainEventTypes::tool_input_ended() || type == AIDomainEventTypes::tool_called()) {
		const String call_id = _ai_projector_call_id(data);
		if (!call_id.is_empty()) {
			const String assistant_id = _ai_projector_assistant_message_id(data);
			const String message_id = assistant_id.is_empty() ? _fallback_message_id("assistant", p_row.seq) : assistant_id;
			const String tool_name = _ai_projector_get_string(data, "tool", "name");
			const Variant input = data.get("input", Variant());
			const Dictionary provider = _ai_projector_provider_dict(data);
			const AIToolState initial_state = type == AIDomainEventTypes::tool_called() ? AIToolState::running(input, provider) : AIToolState::pending(input, provider);
			const int message_index = _ensure_assistant_message_locked(session_id, p_row.seq, message_id, Dictionary(), time_created);
			AISessionMessage &message = messages_by_session[session_id].write[message_index];
			const int tool_index = _ensure_tool_content_locked(message, call_id, tool_name, initial_state, _ai_projector_provider_metadata(data));
			AIAssistantContent &tool_content = message.content.write[tool_index];
			if (!tool_name.is_empty()) {
				tool_content.name = tool_name;
			}
			tool_content.tool_state.input = input;
			tool_content.tool_state.provider = provider;
			tool_content.tool_state.status = type == AIDomainEventTypes::tool_called() ? AI_TOOL_STATUS_RUNNING : AI_TOOL_STATUS_PENDING;
		}
	} else if (type == AIDomainEventTypes::tool_progress()) {
		const String call_id = _ai_projector_call_id(data);
		const String assistant_id = _ai_projector_assistant_message_id(data);
		if (!call_id.is_empty() && !assistant_id.is_empty()) {
			const int message_index = _ensure_assistant_message_locked(session_id, p_row.seq, assistant_id, Dictionary(), time_created);
			AISessionMessage &message = messages_by_session[session_id].write[message_index];
			const int tool_index = _ensure_tool_content_locked(message, call_id, _ai_projector_get_string(data, "tool", "name"), AIToolState::running(data.get("input", Variant()), _ai_projector_provider_dict(data)), _ai_projector_provider_metadata(data));
			message.content.write[tool_index].tool_state.progress = data.get("progress", data.get("content", Variant()));
		}
	} else if (type == AIDomainEventTypes::tool_success() || type == AIDomainEventTypes::tool_failed()) {
		const String call_id = _ai_projector_call_id(data);
		if (!call_id.is_empty()) {
			const String assistant_id = _ai_projector_assistant_message_id(data);
			const String message_id = assistant_id.is_empty() ? _fallback_message_id("assistant", p_row.seq) : assistant_id;
			const int message_index = _ensure_assistant_message_locked(session_id, p_row.seq, message_id, Dictionary(), time_created);
			AISessionMessage &message = messages_by_session[session_id].write[message_index];
			const int tool_index = _ensure_tool_content_locked(message, call_id, _ai_projector_get_string(data, "tool", "name"), AIToolState::pending(), _ai_projector_provider_metadata(data));
			AIAssistantContent &tool_content = message.content.write[tool_index];
			const Variant input = data.has("input") ? data.get("input", Variant()) : tool_content.tool_state.input;
			const Dictionary provider = data.has("provider") ? _ai_projector_provider_dict(data) : tool_content.tool_state.provider;
			if (type == AIDomainEventTypes::tool_success()) {
				Variant output = data.get("structured", Variant());
				if (output.get_type() == Variant::NIL) {
					output = data.get("content", data.get("result", Variant()));
				}
				tool_content.tool_state = AIToolState::success(input, output, _ai_projector_output_paths(data), provider);
				tool_content.tool_state.result = data.get("result", Variant());
			} else {
				tool_content.tool_state = AIToolState::failed(_ai_projector_error_from_data(data, "Tool execution failed."), input, provider);
				tool_content.tool_state.result = data.get("result", Variant());
			}
			if (data.has("metadata") && data["metadata"].get_type() == Variant::DICTIONARY) {
				tool_content.tool_state.metadata = Dictionary(data["metadata"]).duplicate(true);
			}
			tool_content.provider_metadata = _ai_projector_provider_metadata(data);
		}
	} else if (type == AIDomainEventTypes::context_updated()) {
		const String message_id = _ai_projector_get_string(data, "message_id", "messageID", "id", _fallback_message_id("system", p_row.seq));
		const String text = data.get("text", data.get("baseline", data.get("message", String())));
		messages_by_session[session_id].push_back(AISessionMessage::system_message(session_id, p_row.seq, message_id, text, data, time_created));

		Dictionary epoch_dict = _ai_projector_dictionary_from_variant(data.get("epoch", Dictionary()));
		if (epoch_dict.is_empty() && (data.has("baseline") || data.has("snapshot"))) {
			epoch_dict = data.duplicate(true);
		}
		if (!epoch_dict.is_empty()) {
			AIContextEpoch epoch = AIContextEpoch::from_dictionary(epoch_dict);
			if (epoch.session_id.is_empty()) {
				epoch.session_id = session_id;
			}
			if (epoch.baseline_seq == 0) {
				epoch.baseline_seq = p_row.seq;
			}
			const int previous_revision = context_epochs_by_session.has(session_id) ? context_epochs_by_session[session_id].revision : 0;
			if (epoch.revision == 0) {
				epoch.revision = previous_revision + 1;
			}
			context_epochs_by_session[session_id] = epoch;
		}
	} else if (type == AIDomainEventTypes::compaction_ended()) {
		const String message_id = _ai_projector_get_string(data, "message_id", "messageID", "id", _fallback_message_id("compaction", p_row.seq));
		const String text = data.get("summary", data.get("text", String()));
		messages_by_session[session_id].push_back(AISessionMessage::compaction_message(session_id, p_row.seq, message_id, text, data, time_created));
	}

	const int64_t current_seq = projected_seq_by_session.has(session_id) ? projected_seq_by_session[session_id] : 0;
	if (p_row.seq > current_seq) {
		projected_seq_by_session[session_id] = p_row.seq;
	}
}

bool AISessionProjector::project(const AIEventRow &p_row) {
	if (p_row.live_only) {
		return false;
	}

	MutexLock lock(mutex);
	_project_row_locked(p_row);
	return true;
}

int AISessionProjector::project(const Vector<AIEventRow> &p_rows) {
	int count = 0;
	MutexLock lock(mutex);
	for (int i = 0; i < p_rows.size(); i++) {
		if (!p_rows[i].live_only) {
			_project_row_locked(p_rows[i]);
			count++;
		}
	}
	return count;
}

bool AISessionProjector::project_event(const Dictionary &p_event) {
	return project(AIEventRow::from_dictionary(p_event));
}

int AISessionProjector::project_events(const Array &p_events) {
	Vector<AIEventRow> rows;
	for (int i = 0; i < p_events.size(); i++) {
		if (p_events[i].get_type() == Variant::DICTIONARY) {
			rows.push_back(AIEventRow::from_dictionary(p_events[i]));
		}
	}
	return project(rows);
}

int AISessionProjector::project_from_store(const Ref<AIEventStore> &p_store, const String &p_session_id, int64_t p_after_seq) {
	if (p_store.is_null()) {
		return 0;
	}
	return project(p_store->list(p_session_id, p_after_seq, false));
}

int AISessionProjector::rebuild_from_store(const Ref<AIEventStore> &p_store, const String &p_session_id) {
	if (p_store.is_null()) {
		return 0;
	}
	clear_session(p_session_id);
	return project(p_store->list(p_session_id, 0, false));
}

Vector<AISessionInput> AISessionProjector::get_inputs_struct(const String &p_session_id) const {
	MutexLock lock(mutex);
	HashMap<String, Vector<AISessionInput>>::ConstIterator inputs = inputs_by_session.find(p_session_id);
	return inputs ? inputs->value : Vector<AISessionInput>();
}

Vector<AISessionMessage> AISessionProjector::get_messages_struct(const String &p_session_id) const {
	MutexLock lock(mutex);
	HashMap<String, Vector<AISessionMessage>>::ConstIterator messages = messages_by_session.find(p_session_id);
	return messages ? messages->value : Vector<AISessionMessage>();
}

bool AISessionProjector::get_context_epoch_struct(const String &p_session_id, AIContextEpoch &r_epoch) const {
	MutexLock lock(mutex);
	HashMap<String, AIContextEpoch>::ConstIterator epoch = context_epochs_by_session.find(p_session_id);
	if (!epoch) {
		return false;
	}
	r_epoch = epoch->value;
	return true;
}

Array AISessionProjector::get_inputs(const String &p_session_id) const {
	Array result;
	const Vector<AISessionInput> inputs = get_inputs_struct(p_session_id);
	for (int i = 0; i < inputs.size(); i++) {
		result.push_back(inputs[i].to_dictionary());
	}
	return result;
}

Array AISessionProjector::get_messages(const String &p_session_id) const {
	Array result;
	const Vector<AISessionMessage> messages = get_messages_struct(p_session_id);
	for (int i = 0; i < messages.size(); i++) {
		result.push_back(messages[i].to_dictionary());
	}
	return result;
}

Dictionary AISessionProjector::get_context_epoch(const String &p_session_id) const {
	AIContextEpoch epoch;
	if (!get_context_epoch_struct(p_session_id, epoch)) {
		return Dictionary();
	}
	return epoch.to_dictionary();
}

int64_t AISessionProjector::get_projected_seq(const String &p_session_id) const {
	MutexLock lock(mutex);
	return projected_seq_by_session.has(p_session_id) ? projected_seq_by_session[p_session_id] : 0;
}

int AISessionProjector::mark_pending_tools_interrupted(const String &p_session_id, const String &p_reason) {
	int changed = 0;
	MutexLock lock(mutex);
	HashMap<String, Vector<AISessionMessage>>::Iterator messages = messages_by_session.find(p_session_id);
	if (!messages) {
		return changed;
	}

	for (int i = 0; i < messages->value.size(); i++) {
		AISessionMessage &message = messages->value.write[i];
		if (message.type != AI_SESSION_MESSAGE_ASSISTANT) {
			continue;
		}
		for (int j = 0; j < message.content.size(); j++) {
			AIAssistantContent &content = message.content.write[j];
			if (content.type != "tool") {
				continue;
			}
			if (content.tool_state.status == AI_TOOL_STATUS_PENDING || content.tool_state.status == AI_TOOL_STATUS_RUNNING) {
				content.tool_state.status = AI_TOOL_STATUS_FAILED;
				content.tool_state.error = AIError::make(AI_ERROR_INTERRUPTED, p_reason);
				changed++;
			}
		}
	}
	return changed;
}

void AISessionProjector::clear_session(const String &p_session_id) {
	MutexLock lock(mutex);
	inputs_by_session.erase(p_session_id);
	messages_by_session.erase(p_session_id);
	context_epochs_by_session.erase(p_session_id);
	projected_seq_by_session.erase(p_session_id);
}

void AISessionProjector::clear() {
	MutexLock lock(mutex);
	inputs_by_session.clear();
	messages_by_session.clear();
	context_epochs_by_session.clear();
	projected_seq_by_session.clear();
}
