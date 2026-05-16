/**************************************************************************/
/*  ai_conversation_serializer.cpp                                         */
/**************************************************************************/

#include "ai_conversation_serializer.h"

#include "core/variant/variant.h"

Dictionary AIConversationSerializer::message_to_dict(const AIAgentMessage &p_message) {
	return p_message.to_dict();
}

AIAgentMessage AIConversationSerializer::message_from_dict(const Dictionary &p_dict) {
	return AIAgentMessage::from_dict(p_dict);
}

Array AIConversationSerializer::messages_to_array(const Vector<AIAgentMessage> &p_messages) {
	Array array;
	for (int i = 0; i < p_messages.size(); i++) {
		array.push_back(Variant(message_to_dict(p_messages[i])));
	}
	return array;
}

Vector<AIAgentMessage> AIConversationSerializer::messages_from_array(const Array &p_array) {
	Vector<AIAgentMessage> messages;
	for (int i = 0; i < p_array.size(); i++) {
		Variant item = p_array[i];
		if (item.get_type() == Variant::DICTIONARY) {
			Dictionary dict = item;
			messages.push_back(message_from_dict(dict));
		}
	}
	return messages;
}
