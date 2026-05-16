/**************************************************************************/
/*  ai_conversation_serializer.h                                           */
/**************************************************************************/

#pragma once

#include "core/variant/array.h"
#include "core/variant/dictionary.h"

#include "editor/ai_component/agent/ai_agent_message.h"

class AIConversationSerializer {
public:
	static Dictionary message_to_dict(const AIAgentMessage &p_message);
	static AIAgentMessage message_from_dict(const Dictionary &p_dict);
	static Array messages_to_array(const Vector<AIAgentMessage> &p_messages);
	static Vector<AIAgentMessage> messages_from_array(const Array &p_array);
};
