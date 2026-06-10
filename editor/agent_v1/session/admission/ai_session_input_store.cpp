/**************************************************************************/
/*  ai_session_input_store.cpp                                            */
/**************************************************************************/

#include "ai_session_input_store.h"

#include "editor/agent_v1/core/base/ai_id.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/os/time.h"
#include "core/variant/variant.h"

void AISessionInputStore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_base_dir", "base_dir"), &AISessionInputStore::set_base_dir);
	ClassDB::bind_method(D_METHOD("get_base_dir"), &AISessionInputStore::get_base_dir);
	ClassDB::bind_method(D_METHOD("set_event_store", "event_store"), &AISessionInputStore::set_event_store);
	ClassDB::bind_method(D_METHOD("get_event_store"), &AISessionInputStore::get_event_store);
	ClassDB::bind_method(D_METHOD("set_projector", "projector"), &AISessionInputStore::set_projector);
	ClassDB::bind_method(D_METHOD("get_projector"), &AISessionInputStore::get_projector);
	ClassDB::bind_method(D_METHOD("admit_prompt", "input"), &AISessionInputStore::admit_prompt);
	ClassDB::bind_method(D_METHOD("mark_promoted_inputs", "session_id", "prompt_ids"), &AISessionInputStore::mark_promoted_inputs);
	ClassDB::bind_method(D_METHOD("list_admitted", "session_id"), &AISessionInputStore::list_admitted);
	ClassDB::bind_method(D_METHOD("list_inputs", "session_id"), &AISessionInputStore::list_inputs);
	ClassDB::bind_method(D_METHOD("clear_memory"), &AISessionInputStore::clear_memory);
}

AISessionInputStore::AISessionInputStore() {
	base_dir = "user://agent_v1/session_inputs";
}

uint64_t AISessionInputStore::_now_unix_time() {
	return Time::get_singleton() ? Time::get_singleton()->get_unix_time_from_system() : 0;
}

bool AISessionInputStore::_same_prompt_content(const AIPrompt &p_left, const AIPrompt &p_right) {
	const Dictionary left = p_left.to_dictionary();
	const Dictionary right = p_right.to_dictionary();
	return left.recursive_equal(right, 0);
}

bool AISessionInputStore::_same_retry_shape(const AISessionInputRecord &p_existing, const AISessionInputRecord &p_request, bool p_compare_message_id) {
	if (p_existing.session_id != p_request.session_id) {
		return false;
	}
	if (p_compare_message_id && p_existing.message_id != p_request.message_id) {
		return false;
	}
	if (p_existing.delivery != p_request.delivery || p_existing.resume != p_request.resume) {
		return false;
	}
	if (!p_existing.parts.recursive_equal(p_request.parts, 0)) {
		return false;
	}
	return _same_prompt_content(p_existing.prompt, p_request.prompt);
}

AIPrompt AISessionInputStore::_prompt_from_message(const AISessionMessage &p_message) {
	AIPrompt prompt;
	prompt.text = p_message.text;
	prompt.files = p_message.files;
	prompt.agents = p_message.agents;
	prompt.references = p_message.references;
	return prompt;
}

String AISessionInputStore::_get_log_path() const {
	return base_dir.path_join("inputs.jsonl");
}

bool AISessionInputStore::_ensure_base_dir_locked(AIError &r_error) const {
	if (base_dir.is_empty()) {
		return true;
	}

	const Error err = DirAccess::make_dir_recursive_absolute(base_dir);
	if (err != OK) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to create AI session input directory: " + base_dir);
		return false;
	}
	return true;
}

bool AISessionInputStore::_ensure_loaded_locked(AIError &r_error) {
	if (loaded) {
		return true;
	}

	if (base_dir.is_empty()) {
		loaded = true;
		return true;
	}

	const String path = _get_log_path();
	if (!FileAccess::exists(path)) {
		loaded = true;
		return true;
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to open AI session input store: " + path);
		return false;
	}

	Vector<AISessionInputRecord> loaded_inputs;
	while (!file->eof_reached()) {
		const String line = file->get_line().strip_edges();
		if (line.is_empty()) {
			continue;
		}

		Ref<JSON> json;
		json.instantiate();
		const Error parse_err = json->parse(line);
		if (parse_err != OK || json->get_data().get_type() != Variant::DICTIONARY) {
			r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to parse AI session input row: " + path);
			return false;
		}

		const AISessionInputRecord input = AISessionInputRecord::from_dictionary(json->get_data());
		if (input.id.is_empty()) {
			continue;
		}

		bool replaced = false;
		for (int i = 0; i < loaded_inputs.size(); i++) {
			if (loaded_inputs[i].id == input.id) {
				loaded_inputs.write[i] = input;
				replaced = true;
				break;
			}
		}
		if (!replaced) {
			loaded_inputs.push_back(input);
		}
	}

	inputs = loaded_inputs;
	loaded = true;
	return true;
}

bool AISessionInputStore::_append_snapshot_locked(const AISessionInputRecord &p_input, AIError &r_error) const {
	if (base_dir.is_empty()) {
		return true;
	}
	if (!_ensure_base_dir_locked(r_error)) {
		return false;
	}

	const String path = _get_log_path();
	const bool exists = FileAccess::exists(path);
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(path, exists ? FileAccess::READ_WRITE : FileAccess::WRITE_READ, &err);
	if (file.is_null() || err != OK) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to open AI session input store for append: " + path);
		return false;
	}

	file->seek_end();
	file->store_string(JSON::stringify(p_input.to_dictionary()) + "\n");
	file->flush();
	return true;
}

int AISessionInputStore::_find_by_id_locked(const String &p_prompt_id) const {
	for (int i = 0; i < inputs.size(); i++) {
		if (inputs[i].id == p_prompt_id) {
			return i;
		}
	}
	return -1;
}

int AISessionInputStore::_find_by_message_id_locked(const String &p_message_id) const {
	if (p_message_id.is_empty()) {
		return -1;
	}
	for (int i = 0; i < inputs.size(); i++) {
		if (inputs[i].message_id == p_message_id) {
			return i;
		}
	}
	return -1;
}

int AISessionInputStore::_find_by_idempotency_key_locked(const String &p_idempotency_key) const {
	if (p_idempotency_key.is_empty()) {
		return -1;
	}
	for (int i = 0; i < inputs.size(); i++) {
		if (inputs[i].idempotency_key == p_idempotency_key) {
			return i;
		}
	}
	return -1;
}

bool AISessionInputStore::_try_synthesize_from_projected_locked(const AISessionInputRecord &p_request, AISessionInputAdmission &r_admission, AIError &r_error) {
	if (projector.is_null() || p_request.message_id.is_empty()) {
		return false;
	}

	const Vector<AISessionMessage> messages = projector->get_messages_struct(p_request.session_id);
	for (int i = 0; i < messages.size(); i++) {
		const AISessionMessage &message = messages[i];
		if (message.type != AI_SESSION_MESSAGE_USER || message.id != p_request.message_id) {
			continue;
		}

		AISessionInputRecord synthesized = p_request;
		synthesized.id = synthesized.id.is_empty() ? AIId::make("prompt") : synthesized.id;
		synthesized.prompt = _prompt_from_message(message);
		synthesized.status = AI_SESSION_INPUT_STATUS_PROMOTED;
		synthesized.created_at = message.time_created;
		synthesized.promoted_at = message.time_created;
		synthesized.admitted_seq = message.seq;
		synthesized.promoted_seq = message.seq;

		if (!_same_prompt_content(synthesized.prompt, p_request.prompt)) {
			Dictionary details;
			details["message_id"] = p_request.message_id;
			details["session_id"] = p_request.session_id;
			r_error = AIError::make(AI_ERROR_CONFLICT, "Message id was reused with different prompt content.", details);
			return true;
		}

		if (!_append_snapshot_locked(synthesized, r_error)) {
			return true;
		}

		inputs.push_back(synthesized);
		r_admission.input = synthesized;
		r_admission.created = false;
		r_admission.retry = true;
		r_admission.synthesized = true;
		return true;
	}
	return false;
}

void AISessionInputStore::set_base_dir(const String &p_base_dir) {
	MutexLock lock(mutex);
	base_dir = p_base_dir.strip_edges();
	inputs.clear();
	loaded = false;
}

String AISessionInputStore::get_base_dir() const {
	MutexLock lock(mutex);
	return base_dir;
}

void AISessionInputStore::set_event_store(const Ref<AIEventStore> &p_event_store) {
	MutexLock lock(mutex);
	event_store = p_event_store;
}

Ref<AIEventStore> AISessionInputStore::get_event_store() const {
	MutexLock lock(mutex);
	return event_store;
}

void AISessionInputStore::set_projector(const Ref<AISessionProjector> &p_projector) {
	MutexLock lock(mutex);
	projector = p_projector;
}

Ref<AISessionProjector> AISessionInputStore::get_projector() const {
	MutexLock lock(mutex);
	return projector;
}

bool AISessionInputStore::admit(const AISessionInputRecord &p_request, AISessionInputAdmission &r_admission, AIError &r_error) {
	AISessionInputRecord request = p_request;
	request.session_id = request.session_id.strip_edges();
	if (request.session_id.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Session id is required to admit a prompt.");
		return false;
	}
	const bool request_had_message_id = !request.message_id.strip_edges().is_empty();
	if (request.message_id.strip_edges().is_empty()) {
		request.message_id = AIId::make("msg");
	}
	if (request.id.strip_edges().is_empty()) {
		request.id = AIId::make("prompt");
	}
	if (request.created_at == 0) {
		request.created_at = _now_unix_time();
	}
	request.role = request.role.is_empty() ? String("user") : request.role;
	request.status = AI_SESSION_INPUT_STATUS_ADMITTED;

	MutexLock lock(mutex);
	if (!_ensure_loaded_locked(r_error)) {
		return false;
	}

	const int message_index = _find_by_message_id_locked(request.message_id);
	if (message_index >= 0) {
		const AISessionInputRecord &existing = inputs[message_index];
		if (!_same_retry_shape(existing, request, true)) {
			Dictionary details;
			details["message_id"] = request.message_id;
			details["existing_session_id"] = existing.session_id;
			details["request_session_id"] = request.session_id;
			r_error = AIError::make(AI_ERROR_CONFLICT, "Message id was reused with different prompt content.", details);
			return false;
		}

		r_admission.input = existing;
		r_admission.created = false;
		r_admission.retry = true;
		r_admission.synthesized = false;
		return true;
	}

	const int idempotency_index = _find_by_idempotency_key_locked(request.idempotency_key);
	if (idempotency_index >= 0) {
		const AISessionInputRecord &existing = inputs[idempotency_index];
		if (!_same_retry_shape(existing, request, request_had_message_id)) {
			Dictionary details;
			details["idempotency_key"] = request.idempotency_key;
			details["existing_session_id"] = existing.session_id;
			details["request_session_id"] = request.session_id;
			r_error = AIError::make(AI_ERROR_CONFLICT, "Idempotency key was reused with different prompt content.", details);
			return false;
		}

		r_admission.input = existing;
		r_admission.created = false;
		r_admission.retry = true;
		r_admission.synthesized = false;
		return true;
	}

	if (_try_synthesize_from_projected_locked(request, r_admission, r_error)) {
		return !r_error.is_error();
	}

	if (!_append_snapshot_locked(request, r_error)) {
		return false;
	}

	inputs.push_back(request);
	r_admission.input = request;
	r_admission.created = true;
	r_admission.retry = false;
	r_admission.synthesized = false;
	return true;
}

bool AISessionInputStore::set_admitted_seq(const String &p_session_id, const String &p_prompt_id, int64_t p_admitted_seq, AISessionInputRecord &r_input, AIError &r_error) {
	MutexLock lock(mutex);
	if (!_ensure_loaded_locked(r_error)) {
		return false;
	}

	const int index = _find_by_id_locked(p_prompt_id);
	if (index < 0 || inputs[index].session_id != p_session_id) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Prompt input was not found.");
		return false;
	}

	inputs.write[index].admitted_seq = p_admitted_seq;
	if (!_append_snapshot_locked(inputs[index], r_error)) {
		return false;
	}

	r_input = inputs[index];
	return true;
}

bool AISessionInputStore::mark_promoted(const String &p_session_id, const Vector<String> &p_prompt_ids, Vector<AISessionInputRecord> &r_promoted, AIError &r_error) {
	if (p_session_id.strip_edges().is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Session id is required to promote prompts.");
		return false;
	}

	for (int i = 0; i < p_prompt_ids.size(); i++) {
		AIEventRow event_row;
		bool has_event = false;
		AISessionInputRecord promoted_input;
		{
			MutexLock lock(mutex);
			if (!_ensure_loaded_locked(r_error)) {
				return false;
			}

			const int index = _find_by_id_locked(p_prompt_ids[i]);
			if (index < 0 || inputs[index].session_id != p_session_id) {
				r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Prompt input was not found.");
				return false;
			}

			if (inputs[index].is_promoted()) {
				r_promoted.push_back(inputs[index]);
				continue;
			}
			if (inputs[index].status == AI_SESSION_INPUT_STATUS_CANCELED) {
				r_error = AIError::make(AI_ERROR_CONFLICT, "Canceled prompt input cannot be promoted.");
				return false;
			}

			promoted_input = inputs[index];
		}

		if (event_store.is_valid()) {
			Dictionary data;
			data["id"] = promoted_input.id;
			data["session_id"] = promoted_input.session_id;
			data["message_id"] = promoted_input.message_id;
			data["prompt"] = promoted_input.prompt.to_dictionary();
			data["parts"] = promoted_input.parts;
			data["delivery"] = ai_session_input_delivery_to_string(promoted_input.delivery);
			data["time_created"] = promoted_input.created_at;

			String event_error;
			const String idempotency_key = "prompt.promoted:" + promoted_input.session_id + ":" + promoted_input.id;
			if (!event_store->append_idempotent(promoted_input.session_id, AIDomainEventTypes::prompt_promoted(), data, false, idempotency_key, event_row, event_error)) {
				r_error = AIError::make(AI_ERROR_INTERNAL, event_error);
				return false;
			}
			has_event = true;
		}

		{
			MutexLock lock(mutex);
			if (!_ensure_loaded_locked(r_error)) {
				return false;
			}
			const int index = _find_by_id_locked(promoted_input.id);
			if (index < 0) {
				r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Prompt input was not found after promotion event.");
				return false;
			}

			inputs.write[index].status = AI_SESSION_INPUT_STATUS_PROMOTED;
			inputs.write[index].promoted_at = _now_unix_time();
			if (has_event) {
				inputs.write[index].promoted_seq = event_row.seq;
			}
			if (!_append_snapshot_locked(inputs[index], r_error)) {
				return false;
			}
			promoted_input = inputs[index];
		}

		if (has_event && projector.is_valid()) {
			projector->project(event_row);
		}
		r_promoted.push_back(promoted_input);
	}
	return true;
}

bool AISessionInputStore::cancel(const String &p_session_id, const String &p_prompt_id, const String &p_reason, AISessionInputRecord &r_input, AIError &r_error) {
	MutexLock lock(mutex);
	if (!_ensure_loaded_locked(r_error)) {
		return false;
	}

	const int index = _find_by_id_locked(p_prompt_id);
	if (index < 0 || inputs[index].session_id != p_session_id) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Prompt input was not found.");
		return false;
	}
	if (inputs[index].is_promoted()) {
		r_error = AIError::make(AI_ERROR_CONFLICT, "Promoted prompt input cannot be canceled.");
		return false;
	}

	inputs.write[index].status = AI_SESSION_INPUT_STATUS_CANCELED;
	inputs.write[index].cancel_reason = p_reason;
	if (!_append_snapshot_locked(inputs[index], r_error)) {
		return false;
	}
	r_input = inputs[index];
	return true;
}

Vector<AISessionInputRecord> AISessionInputStore::list_admitted_struct(const String &p_session_id) {
	Vector<AISessionInputRecord> result;
	MutexLock lock(mutex);
	AIError error;
	if (!_ensure_loaded_locked(error)) {
		return result;
	}

	for (int i = 0; i < inputs.size(); i++) {
		if (inputs[i].session_id == p_session_id && inputs[i].status == AI_SESSION_INPUT_STATUS_ADMITTED) {
			result.push_back(inputs[i]);
		}
	}
	return result;
}

Vector<AISessionInputRecord> AISessionInputStore::list_inputs_struct(const String &p_session_id) {
	Vector<AISessionInputRecord> result;
	MutexLock lock(mutex);
	AIError error;
	if (!_ensure_loaded_locked(error)) {
		return result;
	}

	for (int i = 0; i < inputs.size(); i++) {
		if (inputs[i].session_id == p_session_id) {
			result.push_back(inputs[i]);
		}
	}
	return result;
}

bool AISessionInputStore::find_by_message_id_struct(const String &p_session_id, const String &p_message_id, AISessionInputRecord &r_input) {
	MutexLock lock(mutex);
	AIError error;
	if (!_ensure_loaded_locked(error)) {
		return false;
	}

	const int index = _find_by_message_id_locked(p_message_id);
	if (index < 0 || inputs[index].session_id != p_session_id) {
		return false;
	}
	r_input = inputs[index];
	return true;
}

Dictionary AISessionInputStore::admit_prompt(const Dictionary &p_input) {
	AISessionInputRecord request = AISessionInputRecord::from_dictionary(p_input);
	AISessionInputAdmission admission;
	AIError error;
	if (!admit(request, admission, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	Dictionary result = admission.to_dictionary();
	result["success"] = true;
	return result;
}

Dictionary AISessionInputStore::mark_promoted_inputs(const String &p_session_id, const Array &p_prompt_ids) {
	Vector<String> prompt_ids;
	for (int i = 0; i < p_prompt_ids.size(); i++) {
		prompt_ids.push_back(String(p_prompt_ids[i]));
	}

	Vector<AISessionInputRecord> promoted;
	AIError error;
	if (!mark_promoted(p_session_id, prompt_ids, promoted, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	Array items;
	for (int i = 0; i < promoted.size(); i++) {
		items.push_back(promoted[i].to_dictionary());
	}
	Dictionary result;
	result["success"] = true;
	result["promoted"] = items;
	return result;
}

Array AISessionInputStore::list_admitted(const String &p_session_id) {
	Array result;
	const Vector<AISessionInputRecord> admitted = list_admitted_struct(p_session_id);
	for (int i = 0; i < admitted.size(); i++) {
		result.push_back(admitted[i].to_dictionary());
	}
	return result;
}

Array AISessionInputStore::list_inputs(const String &p_session_id) {
	Array result;
	const Vector<AISessionInputRecord> session_inputs = list_inputs_struct(p_session_id);
	for (int i = 0; i < session_inputs.size(); i++) {
		result.push_back(session_inputs[i].to_dictionary());
	}
	return result;
}

void AISessionInputStore::clear_memory() {
	MutexLock lock(mutex);
	inputs.clear();
	loaded = false;
}
