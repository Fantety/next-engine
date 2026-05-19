/**************************************************************************/
/*  ai_context_manager.cpp                                                */
/**************************************************************************/

#include "ai_context_manager.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

namespace {

String _role_to_provider_role(AIAgentRole p_role) {
	switch (p_role) {
		case AI_AGENT_ROLE_SYSTEM:
			return "system";
		case AI_AGENT_ROLE_ASSISTANT:
			return "assistant";
		case AI_AGENT_ROLE_TOOL:
			return "tool";
		case AI_AGENT_ROLE_CONTEXT:
			return "system";
		case AI_AGENT_ROLE_ERROR:
			return "user";
		case AI_AGENT_ROLE_USER:
			return "user";
	}
	return "user";
}

bool _message_has_tool_calls(const Dictionary &p_message) {
	if (String(p_message.get("role", "")) != "assistant" || !p_message.has("metadata") || Variant(p_message["metadata"]).get_type() != Variant::DICTIONARY) {
		return false;
	}

	Dictionary metadata = p_message["metadata"];
	if (!metadata.has("tool_calls") || Variant(metadata["tool_calls"]).get_type() != Variant::ARRAY) {
		return false;
	}

	Array tool_calls = metadata["tool_calls"];
	return !tool_calls.is_empty();
}

bool _assistant_has_tool_call_id(const Dictionary &p_assistant_message, const String &p_tool_call_id) {
	if (p_tool_call_id.is_empty() || !_message_has_tool_calls(p_assistant_message)) {
		return false;
	}

	Dictionary metadata = p_assistant_message["metadata"];
	Array tool_calls = metadata["tool_calls"];
	for (int i = 0; i < tool_calls.size(); i++) {
		if (Variant(tool_calls[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary tool_call = tool_calls[i];
		if (String(tool_call.get("id", "")) == p_tool_call_id) {
			return true;
		}
	}
	return false;
}

bool _tool_group_has_all_results(const Array &p_group) {
	if (p_group.is_empty() || Variant(p_group[0]).get_type() != Variant::DICTIONARY) {
		return false;
	}

	Dictionary assistant_message = p_group[0];
	if (!_message_has_tool_calls(assistant_message)) {
		return true;
	}

	Dictionary metadata = assistant_message["metadata"];
	Array tool_calls = metadata["tool_calls"];
	for (int i = 0; i < tool_calls.size(); i++) {
		if (Variant(tool_calls[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary tool_call = tool_calls[i];
		const String expected_id = String(tool_call.get("id", ""));
		bool found_result = false;
		for (int j = 1; j < p_group.size(); j++) {
			if (Variant(p_group[j]).get_type() != Variant::DICTIONARY) {
				continue;
			}

			Dictionary result_message = p_group[j];
			if (String(result_message.get("role", "")) != "tool" || !result_message.has("metadata") || Variant(result_message["metadata"]).get_type() != Variant::DICTIONARY) {
				continue;
			}

			Dictionary result_metadata = result_message["metadata"];
			if (String(result_metadata.get("tool_call_id", "")) == expected_id) {
				found_result = true;
				break;
			}
		}

		if (!found_result) {
			return false;
		}
	}
	return true;
}

} // namespace

void AIContextManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_input_chars", "max_input_chars"), &AIContextManager::set_max_input_chars);
	ClassDB::bind_method(D_METHOD("get_max_input_chars"), &AIContextManager::get_max_input_chars);
	ClassDB::bind_method(D_METHOD("set_max_context_chars", "max_context_chars"), &AIContextManager::set_max_context_chars);
	ClassDB::bind_method(D_METHOD("get_max_context_chars"), &AIContextManager::get_max_context_chars);
	ClassDB::bind_method(D_METHOD("set_max_history_chars", "max_history_chars"), &AIContextManager::set_max_history_chars);
	ClassDB::bind_method(D_METHOD("get_max_history_chars"), &AIContextManager::get_max_history_chars);
	ClassDB::bind_method(D_METHOD("set_max_tool_result_chars", "max_tool_result_chars"), &AIContextManager::set_max_tool_result_chars);
	ClassDB::bind_method(D_METHOD("get_max_tool_result_chars"), &AIContextManager::get_max_tool_result_chars);
	ClassDB::bind_method(D_METHOD("set_min_recent_messages", "min_recent_messages"), &AIContextManager::set_min_recent_messages);
	ClassDB::bind_method(D_METHOD("get_min_recent_messages"), &AIContextManager::get_min_recent_messages);
}

void AIContextManager::set_max_input_chars(int p_max_input_chars) {
	max_input_chars = MAX(256, p_max_input_chars);
}

int AIContextManager::get_max_input_chars() const {
	return max_input_chars;
}

void AIContextManager::set_max_context_chars(int p_max_context_chars) {
	max_context_chars = MAX(128, p_max_context_chars);
}

int AIContextManager::get_max_context_chars() const {
	return max_context_chars;
}

void AIContextManager::set_max_history_chars(int p_max_history_chars) {
	max_history_chars = MAX(128, p_max_history_chars);
}

int AIContextManager::get_max_history_chars() const {
	return max_history_chars;
}

void AIContextManager::set_max_tool_result_chars(int p_max_tool_result_chars) {
	max_tool_result_chars = MAX(64, p_max_tool_result_chars);
}

int AIContextManager::get_max_tool_result_chars() const {
	return max_tool_result_chars;
}

void AIContextManager::set_min_recent_messages(int p_min_recent_messages) {
	min_recent_messages = MAX(1, p_min_recent_messages);
}

int AIContextManager::get_min_recent_messages() const {
	return min_recent_messages;
}

String AIContextManager::_truncate_text(const String &p_text, int p_max_chars, bool &r_truncated) const {
	r_truncated = false;
	if (p_text.length() <= p_max_chars) {
		return p_text;
	}

	r_truncated = true;
	const String suffix = "\n[truncated]";
	const int content_limit = MAX(0, p_max_chars - suffix.length());
	return p_text.substr(0, content_limit) + suffix;
}

String AIContextManager::_build_context_text(const Array &p_context_documents, int &r_truncated_documents) const {
	r_truncated_documents = 0;
	if (p_context_documents.is_empty()) {
		return String();
	}

	String context_text = "Read-only project context:\n";
	for (int i = 0; i < p_context_documents.size(); i++) {
		if (Variant(p_context_documents[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary doc = p_context_documents[i];
		context_text += "\n## " + String(doc.get("title", "Context")) + "\n";
		context_text += "Source: " + String(doc.get("source", "")) + "\n";
		context_text += String(doc.get("content", "")) + "\n";
		if (bool(doc.get("truncated", false))) {
			r_truncated_documents++;
		}
	}

	bool context_truncated = false;
	context_text = _truncate_text(context_text, max_context_chars, context_truncated);
	if (context_truncated) {
		r_truncated_documents++;
	}
	return context_text;
}

Dictionary AIContextManager::_message_to_provider_dict(const AIAgentMessage &p_message) const {
	Dictionary message;
	message["role"] = _role_to_provider_role(p_message.role);

	bool content_truncated = false;
	String content = p_message.content;
	if (p_message.role == AI_AGENT_ROLE_TOOL) {
		content = _truncate_text(content, max_tool_result_chars, content_truncated);
	}
	message["content"] = content;

	Dictionary metadata = p_message.metadata.duplicate(true);
	if (content_truncated) {
		metadata["context_truncated"] = true;
		metadata["original_chars"] = p_message.content.length();
		metadata["kept_chars"] = content.length();
	}
	if (!metadata.is_empty()) {
		message["metadata"] = metadata;
	}
	if (p_message.created_at != 0) {
		message["created_at"] = p_message.created_at;
	}

	return message;
}

int AIContextManager::_estimate_message_chars(const Dictionary &p_message) const {
	int chars = 0;
	chars += String(p_message.get("role", "")).length();
	chars += String(p_message.get("content", "")).length();
	if (p_message.has("metadata")) {
		chars += Variant(p_message["metadata"]).stringify().length();
	}
	return chars;
}

int AIContextManager::_estimate_messages_chars(const Array &p_messages) const {
	int chars = 0;
	for (int i = 0; i < p_messages.size(); i++) {
		if (Variant(p_messages[i]).get_type() == Variant::DICTIONARY) {
			chars += _estimate_message_chars(p_messages[i]);
		}
	}
	return chars;
}

int AIContextManager::_count_truncated_tool_results(const Array &p_messages) const {
	int count = 0;
	for (int i = 0; i < p_messages.size(); i++) {
		if (Variant(p_messages[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary message = p_messages[i];
		if (String(message.get("role", "")) != "tool") {
			continue;
		}
		if (message.has("metadata") && Variant(message["metadata"]).get_type() == Variant::DICTIONARY) {
			Dictionary metadata = message["metadata"];
			if (bool(metadata.get("context_truncated", false))) {
				count++;
			}
		}
	}
	return count;
}

AIContextBuildResult AIContextManager::build_messages(const String &p_system_prompt, const Vector<AIAgentMessage> &p_messages, const Array &p_context_documents) const {
	AIContextBuildResult result;

	Dictionary system_message;
	system_message["role"] = "system";
	system_message["content"] = p_system_prompt;
	result.messages.push_back(system_message);

	int truncated_context_documents = 0;
	String context_text = _build_context_text(p_context_documents, truncated_context_documents);
	if (!context_text.is_empty()) {
		Dictionary context_message;
		context_message["role"] = "system";
		context_message["content"] = context_text;
		result.messages.push_back(context_message);
	}

	Array history_messages;
	for (int i = 0; i < p_messages.size(); i++) {
		history_messages.push_back(_message_to_provider_dict(p_messages[i]));
	}

	Array history_groups;
	int omitted_history_messages = 0;
	for (int i = 0; i < history_messages.size(); i++) {
		if (Variant(history_messages[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary message = history_messages[i];
		const String role = String(message.get("role", ""));
		if (role == "tool") {
			omitted_history_messages++;
			continue;
		}

		Array group;
		group.push_back(message);
		if (_message_has_tool_calls(message)) {
			while (i + 1 < history_messages.size() && Variant(history_messages[i + 1]).get_type() == Variant::DICTIONARY) {
				Dictionary next_message = history_messages[i + 1];
				if (String(next_message.get("role", "")) != "tool") {
					break;
				}

				Dictionary next_metadata;
				if (next_message.has("metadata") && Variant(next_message["metadata"]).get_type() == Variant::DICTIONARY) {
					next_metadata = next_message["metadata"];
				}
				if (!_assistant_has_tool_call_id(message, String(next_metadata.get("tool_call_id", "")))) {
					break;
				}

				group.push_back(next_message);
				i++;
			}

			if (!_tool_group_has_all_results(group)) {
				omitted_history_messages += group.size();
				continue;
			}
		}

		history_groups.push_back(group);
	}

	int kept_history_chars = 0;
	int recent_messages = 0;
	Array kept_groups_reversed;
	for (int i = history_groups.size() - 1; i >= 0; i--) {
		Array group = history_groups[i];
		const int group_chars = _estimate_messages_chars(group);
		const int group_message_count = group.size();
		const bool keep_for_recency = recent_messages < min_recent_messages;
		if (!keep_for_recency && kept_history_chars + group_chars > max_history_chars) {
			omitted_history_messages += group_message_count;
			continue;
		}

		kept_groups_reversed.push_back(group);
		kept_history_chars += group_chars;
		recent_messages += group_message_count;
	}

	Array kept_groups;
	for (int i = kept_groups_reversed.size() - 1; i >= 0; i--) {
		kept_groups.push_back(kept_groups_reversed[i]);
	}

	while (true) {
		Array candidate_messages = result.messages.duplicate();
		int candidate_history_count = 0;
		for (int i = 0; i < kept_groups.size(); i++) {
			Array group = kept_groups[i];
			candidate_history_count += group.size();
			for (int j = 0; j < group.size(); j++) {
				candidate_messages.push_back(group[j]);
			}
		}

		if (_estimate_messages_chars(candidate_messages) <= max_input_chars || kept_groups.is_empty() || candidate_history_count <= min_recent_messages) {
			result.messages = candidate_messages;
			break;
		}

		Array oldest_group = kept_groups[0];
		omitted_history_messages += oldest_group.size();
		kept_groups.remove_at(0);
	}

	result.metadata["estimated_input_chars"] = _estimate_messages_chars(result.messages);
	result.metadata["max_input_chars"] = max_input_chars;
	result.metadata["max_context_chars"] = max_context_chars;
	result.metadata["max_history_chars"] = max_history_chars;
	result.metadata["max_tool_result_chars"] = max_tool_result_chars;
	result.metadata["input_messages"] = p_messages.size();
	result.metadata["output_messages"] = result.messages.size();
	result.metadata["omitted_history_messages"] = omitted_history_messages;
	result.metadata["truncated_tool_results"] = _count_truncated_tool_results(result.messages);
	result.metadata["truncated_context_documents"] = truncated_context_documents;
	return result;
}
