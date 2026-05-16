/**************************************************************************/
/*  ai_agent_runner.cpp                                                    */
/**************************************************************************/

#include "ai_agent_runner.h"

#include "core/object/class_db.h"

#include "editor/ai_component/prompts/agent_system_prompt.h"

void AIAgentRunner::_bind_methods() {
}

void AIAgentRunner::set_provider(AIAgentProvider *p_provider) {
	provider = p_provider;
}

bool AIAgentRunner::start(const Vector<AIAgentMessage> &p_messages, const Array &p_context_documents) {
	ERR_FAIL_NULL_V(provider, false);
	return provider->start_chat(_build_provider_messages(p_messages, p_context_documents));
}

void AIAgentRunner::cancel() {
	if (provider) {
		provider->cancel();
	}
}

Array AIAgentRunner::_build_provider_messages(const Vector<AIAgentMessage> &p_messages, const Array &p_context_documents) const {
	Array messages;

	Dictionary system_message;
	system_message["role"] = "system";
	system_message["content"] = String(AIAgentPrompts::SYSTEM_PROMPT);
	messages.push_back(system_message);

	if (!p_context_documents.is_empty()) {
		String context_text = "Read-only project context:\n";
		for (int i = 0; i < p_context_documents.size(); i++) {
			if (Variant(p_context_documents[i]).get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary doc = p_context_documents[i];
			context_text += "\n## " + String(doc.get("title", "Context")) + "\n";
			context_text += "Source: " + String(doc.get("source", "")) + "\n";
			context_text += String(doc.get("content", "")) + "\n";
		}
		Dictionary context_message;
		context_message["role"] = "system";
		context_message["content"] = context_text;
		messages.push_back(context_message);
	}

	for (int i = 0; i < p_messages.size(); i++) {
		const AIAgentMessage &message = p_messages[i];
		if (message.role != AI_AGENT_ROLE_USER && message.role != AI_AGENT_ROLE_ASSISTANT) {
			continue;
		}
		Dictionary api_message;
		api_message["role"] = AIAgentMessage::role_to_string(message.role);
		api_message["content"] = message.content;
		messages.push_back(api_message);
	}

	return messages;
}
