/**************************************************************************/
/*  ai_domain_types.cpp                                                   */
/**************************************************************************/

#include "ai_domain_types.h"

#include "core/variant/variant.h"

static Dictionary _ai_dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return p_value;
	}
	return Dictionary();
}

static Array _ai_array_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::ARRAY) {
		return p_value;
	}
	return Array();
}

static PackedStringArray _ai_packed_string_array_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::PACKED_STRING_ARRAY) {
		return p_value;
	}

	PackedStringArray result;
	const Array array = _ai_array_from_variant(p_value);
	for (int i = 0; i < array.size(); i++) {
		result.push_back(String(array[i]));
	}
	return result;
}

static AIError _ai_error_from_dictionary(const Dictionary &p_dict) {
	const String kind = p_dict.get("kind", "unknown");
	const String message = p_dict.get("message", String());
	const Dictionary details = _ai_dictionary_from_variant(p_dict.get("details", Dictionary()));
	return AIError::make(AIError::string_to_kind(kind), message, details);
}

String ai_session_input_delivery_to_string(AISessionInputDelivery p_delivery) {
	switch (p_delivery) {
		case AI_SESSION_INPUT_DELIVERY_STEER:
			return "steer";
		case AI_SESSION_INPUT_DELIVERY_QUEUE:
			return "queue";
	}
	return "queue";
}

AISessionInputDelivery ai_session_input_delivery_from_string(const String &p_delivery) {
	const String delivery = p_delivery.strip_edges().to_lower();
	if (delivery == "steer") {
		return AI_SESSION_INPUT_DELIVERY_STEER;
	}
	return AI_SESSION_INPUT_DELIVERY_QUEUE;
}

String ai_session_message_type_to_string(AISessionMessageType p_type) {
	switch (p_type) {
		case AI_SESSION_MESSAGE_USER:
			return "user";
		case AI_SESSION_MESSAGE_ASSISTANT:
			return "assistant";
		case AI_SESSION_MESSAGE_SYSTEM:
			return "system";
		case AI_SESSION_MESSAGE_COMPACTION:
			return "compaction";
	}
	return "user";
}

AISessionMessageType ai_session_message_type_from_string(const String &p_type) {
	const String type = p_type.strip_edges().to_lower();
	if (type == "assistant") {
		return AI_SESSION_MESSAGE_ASSISTANT;
	}
	if (type == "system") {
		return AI_SESSION_MESSAGE_SYSTEM;
	}
	if (type == "compaction") {
		return AI_SESSION_MESSAGE_COMPACTION;
	}
	return AI_SESSION_MESSAGE_USER;
}

String ai_tool_status_to_string(AIToolStatus p_status) {
	switch (p_status) {
		case AI_TOOL_STATUS_PENDING:
			return "pending";
		case AI_TOOL_STATUS_RUNNING:
			return "running";
		case AI_TOOL_STATUS_SUCCESS:
			return "success";
		case AI_TOOL_STATUS_FAILED:
			return "failed";
	}
	return "pending";
}

AIToolStatus ai_tool_status_from_string(const String &p_status) {
	const String status = p_status.strip_edges().to_lower();
	if (status == "running") {
		return AI_TOOL_STATUS_RUNNING;
	}
	if (status == "success") {
		return AI_TOOL_STATUS_SUCCESS;
	}
	if (status == "failed") {
		return AI_TOOL_STATUS_FAILED;
	}
	return AI_TOOL_STATUS_PENDING;
}

Dictionary AILocationRef::to_dictionary() const {
	Dictionary result;
	result["directory"] = directory;
	result["workspace_id"] = workspace_id;
	return result;
}

AILocationRef AILocationRef::from_dictionary(const Dictionary &p_dict) {
	AILocationRef result;
	result.directory = p_dict.get("directory", String());
	result.workspace_id = p_dict.get("workspace_id", p_dict.get("workspaceID", String()));
	return result;
}

Dictionary AIModelRef::to_dictionary() const {
	Dictionary result;
	result["provider"] = provider;
	result["model"] = model;
	result["metadata"] = metadata;
	return result;
}

AIModelRef AIModelRef::from_dictionary(const Dictionary &p_dict) {
	AIModelRef result;
	result.provider = p_dict.get("provider", String());
	result.model = p_dict.get("model", String());
	result.metadata = _ai_dictionary_from_variant(p_dict.get("metadata", Dictionary())).duplicate(true);
	return result;
}

int64_t AITokenUsage::get_total_tokens() const {
	return input_tokens + output_tokens + cache_read_tokens + cache_write_tokens;
}

Dictionary AITokenUsage::to_dictionary() const {
	Dictionary result;
	result["input_tokens"] = input_tokens;
	result["output_tokens"] = output_tokens;
	result["cache_read_tokens"] = cache_read_tokens;
	result["cache_write_tokens"] = cache_write_tokens;
	result["total_tokens"] = get_total_tokens();
	return result;
}

AITokenUsage AITokenUsage::from_dictionary(const Dictionary &p_dict) {
	AITokenUsage result;
	result.input_tokens = int64_t(p_dict.get("input_tokens", p_dict.get("input", 0)));
	result.output_tokens = int64_t(p_dict.get("output_tokens", p_dict.get("output", 0)));
	result.cache_read_tokens = int64_t(p_dict.get("cache_read_tokens", p_dict.get("cache_read", 0)));
	result.cache_write_tokens = int64_t(p_dict.get("cache_write_tokens", p_dict.get("cache_write", 0)));
	return result;
}

Dictionary AIFileAttachment::to_dictionary() const {
	Dictionary result;
	result["id"] = id;
	result["path"] = path;
	result["name"] = name;
	result["mime"] = mime;
	result["size_bytes"] = size_bytes;
	result["metadata"] = metadata;
	return result;
}

AIFileAttachment AIFileAttachment::from_dictionary(const Dictionary &p_dict) {
	AIFileAttachment result;
	result.id = p_dict.get("id", String());
	result.path = p_dict.get("path", String());
	result.name = p_dict.get("name", String());
	result.mime = p_dict.get("mime", p_dict.get("mime_type", String()));
	result.size_bytes = int64_t(p_dict.get("size_bytes", p_dict.get("size", 0)));
	result.metadata = _ai_dictionary_from_variant(p_dict.get("metadata", Dictionary())).duplicate(true);
	return result;
}

Dictionary AIAgentReference::to_dictionary() const {
	Dictionary result;
	result["id"] = id;
	result["name"] = name;
	result["metadata"] = metadata;
	return result;
}

AIAgentReference AIAgentReference::from_dictionary(const Dictionary &p_dict) {
	AIAgentReference result;
	result.id = p_dict.get("id", String());
	result.name = p_dict.get("name", String());
	result.metadata = _ai_dictionary_from_variant(p_dict.get("metadata", Dictionary())).duplicate(true);
	return result;
}

Dictionary AIPromptReference::to_dictionary() const {
	Dictionary result;
	result["id"] = id;
	result["kind"] = kind;
	result["label"] = label;
	result["value"] = value;
	result["metadata"] = metadata;
	return result;
}

AIPromptReference AIPromptReference::from_dictionary(const Dictionary &p_dict) {
	AIPromptReference result;
	result.id = p_dict.get("id", String());
	result.kind = p_dict.get("kind", p_dict.get("type", String()));
	result.label = p_dict.get("label", String());
	result.value = p_dict.get("value", Variant());
	result.metadata = _ai_dictionary_from_variant(p_dict.get("metadata", Dictionary())).duplicate(true);
	return result;
}

Dictionary AIPrompt::to_dictionary() const {
	Dictionary result;
	result["text"] = text;

	Array file_array;
	for (int i = 0; i < files.size(); i++) {
		file_array.push_back(files[i].to_dictionary());
	}
	result["files"] = file_array;

	Array agent_array;
	for (int i = 0; i < agents.size(); i++) {
		agent_array.push_back(agents[i].to_dictionary());
	}
	result["agents"] = agent_array;

	Array reference_array;
	for (int i = 0; i < references.size(); i++) {
		reference_array.push_back(references[i].to_dictionary());
	}
	result["references"] = reference_array;
	return result;
}

AIPrompt AIPrompt::from_dictionary(const Dictionary &p_dict) {
	AIPrompt result;
	result.text = p_dict.get("text", String());

	const Array file_array = _ai_array_from_variant(p_dict.get("files", Array()));
	for (int i = 0; i < file_array.size(); i++) {
		const Dictionary item = _ai_dictionary_from_variant(file_array[i]);
		if (!item.is_empty()) {
			result.files.push_back(AIFileAttachment::from_dictionary(item));
		}
	}

	const Array agent_array = _ai_array_from_variant(p_dict.get("agents", Array()));
	for (int i = 0; i < agent_array.size(); i++) {
		const Dictionary item = _ai_dictionary_from_variant(agent_array[i]);
		if (!item.is_empty()) {
			result.agents.push_back(AIAgentReference::from_dictionary(item));
		}
	}

	const Array reference_array = _ai_array_from_variant(p_dict.get("references", Array()));
	for (int i = 0; i < reference_array.size(); i++) {
		const Dictionary item = _ai_dictionary_from_variant(reference_array[i]);
		if (!item.is_empty()) {
			result.references.push_back(AIPromptReference::from_dictionary(item));
		}
	}
	return result;
}

Dictionary AISessionInfo::to_dictionary() const {
	Dictionary result;
	result["id"] = id;
	result["project_id"] = project_id;
	result["directory"] = location.directory;
	result["path"] = path;
	result["workspace_id"] = location.workspace_id;
	result["title"] = title;
	result["agent"] = agent_id;
	result["model"] = model.to_dictionary();
	result["cost"] = cost;
	result["tokens"] = tokens.to_dictionary();

	Dictionary time;
	time["created"] = time_created;
	time["updated"] = time_updated;
	result["time"] = time;
	return result;
}

AISessionInfo AISessionInfo::from_dictionary(const Dictionary &p_dict) {
	AISessionInfo result;
	result.id = p_dict.get("id", String());
	result.project_id = p_dict.get("project_id", p_dict.get("projectID", String()));
	result.location.directory = p_dict.get("directory", String());
	result.location.workspace_id = p_dict.get("workspace_id", p_dict.get("workspaceID", String()));
	result.path = p_dict.get("path", String());
	result.title = p_dict.get("title", String());
	result.agent_id = p_dict.get("agent", p_dict.get("agent_id", String()));
	result.model = AIModelRef::from_dictionary(_ai_dictionary_from_variant(p_dict.get("model", Dictionary())));
	result.cost = double(p_dict.get("cost", 0.0));
	result.tokens = AITokenUsage::from_dictionary(_ai_dictionary_from_variant(p_dict.get("tokens", Dictionary())));

	const Dictionary time = _ai_dictionary_from_variant(p_dict.get("time", Dictionary()));
	result.time_created = uint64_t(time.get("created", p_dict.get("time_created", 0)));
	result.time_updated = uint64_t(time.get("updated", p_dict.get("time_updated", 0)));
	return result;
}

bool AISessionInput::is_promoted() const {
	return promoted_seq > 0;
}

Dictionary AISessionInput::to_dictionary() const {
	Dictionary result;
	result["admitted_seq"] = admitted_seq;
	result["id"] = id;
	result["message_id"] = message_id;
	result["session_id"] = session_id;
	result["prompt"] = prompt.to_dictionary();
	result["delivery"] = ai_session_input_delivery_to_string(delivery);
	result["time_created"] = time_created;
	if (promoted_seq > 0) {
		result["promoted_seq"] = promoted_seq;
	}
	return result;
}

AISessionInput AISessionInput::from_dictionary(const Dictionary &p_dict) {
	AISessionInput result;
	result.admitted_seq = int64_t(p_dict.get("admitted_seq", p_dict.get("admittedSeq", 0)));
	result.id = p_dict.get("id", String());
	result.message_id = p_dict.get("message_id", p_dict.get("messageID", String()));
	result.session_id = p_dict.get("session_id", p_dict.get("sessionID", String()));
	result.prompt = AIPrompt::from_dictionary(_ai_dictionary_from_variant(p_dict.get("prompt", Dictionary())));
	result.delivery = ai_session_input_delivery_from_string(p_dict.get("delivery", "queue"));
	result.time_created = uint64_t(p_dict.get("time_created", p_dict.get("timeCreated", 0)));
	result.promoted_seq = int64_t(p_dict.get("promoted_seq", p_dict.get("promotedSeq", 0)));
	return result;
}

Dictionary AIToolState::to_dictionary() const {
	Dictionary dict;
	dict["status"] = ai_tool_status_to_string(status);
	dict["input"] = input;
	dict["progress"] = progress;
	dict["output"] = output;
	dict["output_paths"] = output_paths;
	dict["error"] = error.to_dictionary();
	dict["result"] = this->result;
	dict["provider"] = provider;
	dict["metadata"] = metadata;
	return dict;
}

AIToolState AIToolState::from_dictionary(const Dictionary &p_dict) {
	AIToolState state;
	state.status = ai_tool_status_from_string(p_dict.get("status", "pending"));
	state.input = p_dict.get("input", Variant());
	state.progress = p_dict.get("progress", Variant());
	state.output = p_dict.get("output", Variant());
	state.output_paths = _ai_packed_string_array_from_variant(p_dict.get("output_paths", p_dict.get("outputPaths", PackedStringArray())));
	const Dictionary error_dict = _ai_dictionary_from_variant(p_dict.get("error", Dictionary()));
	if (!error_dict.is_empty()) {
		state.error = _ai_error_from_dictionary(error_dict);
	}
	state.result = p_dict.get("result", Variant());
	state.provider = _ai_dictionary_from_variant(p_dict.get("provider", Dictionary())).duplicate(true);
	state.metadata = _ai_dictionary_from_variant(p_dict.get("metadata", Dictionary())).duplicate(true);
	return state;
}

AIToolState AIToolState::pending(const Variant &p_input, const Dictionary &p_provider) {
	AIToolState state;
	state.status = AI_TOOL_STATUS_PENDING;
	state.input = p_input;
	state.provider = p_provider.duplicate(true);
	return state;
}

AIToolState AIToolState::running(const Variant &p_input, const Dictionary &p_provider) {
	AIToolState state;
	state.status = AI_TOOL_STATUS_RUNNING;
	state.input = p_input;
	state.provider = p_provider.duplicate(true);
	return state;
}

AIToolState AIToolState::success(const Variant &p_input, const Variant &p_output, const PackedStringArray &p_output_paths, const Dictionary &p_provider) {
	AIToolState state;
	state.status = AI_TOOL_STATUS_SUCCESS;
	state.input = p_input;
	state.output = p_output;
	state.output_paths = p_output_paths;
	state.provider = p_provider.duplicate(true);
	return state;
}

AIToolState AIToolState::failed(const AIError &p_error, const Variant &p_input, const Dictionary &p_provider) {
	AIToolState state;
	state.status = AI_TOOL_STATUS_FAILED;
	state.input = p_input;
	state.error = p_error;
	state.provider = p_provider.duplicate(true);
	return state;
}

Dictionary AIAssistantContent::to_dictionary() const {
	Dictionary result;
	result["type"] = type;
	result["id"] = id;
	result["name"] = name;
	result["text"] = text;
	if (type == "tool") {
		result["state"] = tool_state.to_dictionary();
	}
	result["provider_metadata"] = provider_metadata;
	result["metadata"] = metadata;
	return result;
}

AIAssistantContent AIAssistantContent::from_dictionary(const Dictionary &p_dict) {
	AIAssistantContent result;
	result.type = p_dict.get("type", String());
	result.id = p_dict.get("id", String());
	result.name = p_dict.get("name", String());
	result.text = p_dict.get("text", String());
	result.tool_state = AIToolState::from_dictionary(_ai_dictionary_from_variant(p_dict.get("state", Dictionary())));
	result.provider_metadata = _ai_dictionary_from_variant(p_dict.get("provider_metadata", p_dict.get("providerMetadata", Dictionary()))).duplicate(true);
	result.metadata = _ai_dictionary_from_variant(p_dict.get("metadata", Dictionary())).duplicate(true);
	return result;
}

AIAssistantContent AIAssistantContent::text_content(const String &p_text, const String &p_id) {
	AIAssistantContent content;
	content.type = "text";
	content.id = p_id;
	content.text = p_text;
	return content;
}

AIAssistantContent AIAssistantContent::reasoning_content(const String &p_text, const Dictionary &p_provider_metadata, const String &p_id) {
	AIAssistantContent content;
	content.type = "reasoning";
	content.id = p_id;
	content.text = p_text;
	content.provider_metadata = p_provider_metadata.duplicate(true);
	return content;
}

AIAssistantContent AIAssistantContent::tool_content(const String &p_call_id, const String &p_name, const AIToolState &p_state, const Dictionary &p_provider_metadata) {
	AIAssistantContent content;
	content.type = "tool";
	content.id = p_call_id;
	content.name = p_name;
	content.tool_state = p_state;
	content.provider_metadata = p_provider_metadata.duplicate(true);
	return content;
}

bool AISessionMessage::is_runner_visible_system_message(int64_t p_baseline_seq) const {
	return type != AI_SESSION_MESSAGE_SYSTEM || seq > p_baseline_seq;
}

Dictionary AISessionMessage::to_dictionary() const {
	Dictionary result;
	result["seq"] = seq;
	result["id"] = id;
	result["session_id"] = session_id;
	result["type"] = ai_session_message_type_to_string(type);
	result["text"] = text;

	Array file_array;
	for (int i = 0; i < files.size(); i++) {
		file_array.push_back(files[i].to_dictionary());
	}
	result["files"] = file_array;

	Array agent_array;
	for (int i = 0; i < agents.size(); i++) {
		agent_array.push_back(agents[i].to_dictionary());
	}
	result["agents"] = agent_array;

	Array reference_array;
	for (int i = 0; i < references.size(); i++) {
		reference_array.push_back(references[i].to_dictionary());
	}
	result["references"] = reference_array;

	Array content_array;
	for (int i = 0; i < content.size(); i++) {
		content_array.push_back(content[i].to_dictionary());
	}
	result["content"] = content_array;
	result["metadata"] = metadata;
	result["time_created"] = time_created;
	return result;
}

AISessionMessage AISessionMessage::from_dictionary(const Dictionary &p_dict) {
	AISessionMessage result;
	result.seq = int64_t(p_dict.get("seq", 0));
	result.id = p_dict.get("id", String());
	result.session_id = p_dict.get("session_id", p_dict.get("sessionID", String()));
	result.type = ai_session_message_type_from_string(p_dict.get("type", "user"));
	result.text = p_dict.get("text", String());

	const Array file_array = _ai_array_from_variant(p_dict.get("files", Array()));
	for (int i = 0; i < file_array.size(); i++) {
		const Dictionary item = _ai_dictionary_from_variant(file_array[i]);
		if (!item.is_empty()) {
			result.files.push_back(AIFileAttachment::from_dictionary(item));
		}
	}

	const Array agent_array = _ai_array_from_variant(p_dict.get("agents", Array()));
	for (int i = 0; i < agent_array.size(); i++) {
		const Dictionary item = _ai_dictionary_from_variant(agent_array[i]);
		if (!item.is_empty()) {
			result.agents.push_back(AIAgentReference::from_dictionary(item));
		}
	}

	const Array reference_array = _ai_array_from_variant(p_dict.get("references", Array()));
	for (int i = 0; i < reference_array.size(); i++) {
		const Dictionary item = _ai_dictionary_from_variant(reference_array[i]);
		if (!item.is_empty()) {
			result.references.push_back(AIPromptReference::from_dictionary(item));
		}
	}

	const Array content_array = _ai_array_from_variant(p_dict.get("content", Array()));
	for (int i = 0; i < content_array.size(); i++) {
		const Dictionary item = _ai_dictionary_from_variant(content_array[i]);
		if (!item.is_empty()) {
			result.content.push_back(AIAssistantContent::from_dictionary(item));
		}
	}

	result.metadata = _ai_dictionary_from_variant(p_dict.get("metadata", Dictionary())).duplicate(true);
	result.time_created = uint64_t(p_dict.get("time_created", p_dict.get("timeCreated", 0)));
	return result;
}

AISessionMessage AISessionMessage::user_message(const String &p_session_id, int64_t p_seq, const String &p_id, const AIPrompt &p_prompt, uint64_t p_time_created) {
	AISessionMessage message;
	message.seq = p_seq;
	message.id = p_id;
	message.session_id = p_session_id;
	message.type = AI_SESSION_MESSAGE_USER;
	message.text = p_prompt.text;
	message.files = p_prompt.files;
	message.agents = p_prompt.agents;
	message.references = p_prompt.references;
	message.time_created = p_time_created;
	return message;
}

AISessionMessage AISessionMessage::assistant_shell(const String &p_session_id, int64_t p_seq, const String &p_id, const Dictionary &p_metadata, uint64_t p_time_created) {
	AISessionMessage message;
	message.seq = p_seq;
	message.id = p_id;
	message.session_id = p_session_id;
	message.type = AI_SESSION_MESSAGE_ASSISTANT;
	message.metadata = p_metadata.duplicate(true);
	message.time_created = p_time_created;
	return message;
}

AISessionMessage AISessionMessage::system_message(const String &p_session_id, int64_t p_seq, const String &p_id, const String &p_text, const Dictionary &p_metadata, uint64_t p_time_created) {
	AISessionMessage message;
	message.seq = p_seq;
	message.id = p_id;
	message.session_id = p_session_id;
	message.type = AI_SESSION_MESSAGE_SYSTEM;
	message.text = p_text;
	message.metadata = p_metadata.duplicate(true);
	message.time_created = p_time_created;
	return message;
}

AISessionMessage AISessionMessage::compaction_message(const String &p_session_id, int64_t p_seq, const String &p_id, const String &p_text, const Dictionary &p_metadata, uint64_t p_time_created) {
	AISessionMessage message;
	message.seq = p_seq;
	message.id = p_id;
	message.session_id = p_session_id;
	message.type = AI_SESSION_MESSAGE_COMPACTION;
	message.text = p_text;
	message.metadata = p_metadata.duplicate(true);
	message.time_created = p_time_created;
	return message;
}

bool AIContextEpoch::has_replacement() const {
	return replacement_seq > 0;
}

Dictionary AIContextEpoch::to_dictionary() const {
	Dictionary result;
	result["session_id"] = session_id;
	result["baseline"] = baseline;
	result["snapshot"] = snapshot;
	result["agent"] = agent_id;
	result["baseline_seq"] = baseline_seq;
	if (replacement_seq > 0) {
		result["replacement_seq"] = replacement_seq;
	}
	result["revision"] = revision;
	return result;
}

AIContextEpoch AIContextEpoch::from_dictionary(const Dictionary &p_dict) {
	AIContextEpoch result;
	result.session_id = p_dict.get("session_id", p_dict.get("sessionID", String()));
	result.baseline = p_dict.get("baseline", String());
	result.snapshot = _ai_dictionary_from_variant(p_dict.get("snapshot", Dictionary())).duplicate(true);
	result.agent_id = p_dict.get("agent", p_dict.get("agent_id", String()));
	result.baseline_seq = int64_t(p_dict.get("baseline_seq", p_dict.get("baselineSeq", 0)));
	result.replacement_seq = int64_t(p_dict.get("replacement_seq", p_dict.get("replacementSeq", 0)));
	result.revision = int(p_dict.get("revision", 0));
	return result;
}
