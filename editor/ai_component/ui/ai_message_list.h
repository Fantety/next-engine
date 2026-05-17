/**************************************************************************/
/*  ai_message_list.h                                                      */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"
#include "scene/gui/scroll_container.h"

#include "editor/ai_component/ui/ai_message_bubble.h"

class AIMessageList : public ScrollContainer {
	GDCLASS(AIMessageList, ScrollContainer);

	VBoxContainer *message_box = nullptr;
	Vector<AIMessageBubble *> bubbles;

protected:
	static void _bind_methods();

public:
	AIMessageList();
	void clear_messages();
	void add_message(const Dictionary &p_message);
	void update_message(int p_index, const Dictionary &p_message);
	void remove_message(int p_index);
};
