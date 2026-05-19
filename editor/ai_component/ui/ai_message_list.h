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
	bool should_scroll_to_bottom = true;
	bool scroll_to_bottom_queued = false;
	int pending_scroll_to_bottom_passes = 0;

	bool _is_at_bottom() const;
	void _request_scroll_to_bottom_if_needed();
	void _queue_scroll_to_bottom(int p_passes);
	void _scroll_to_bottom();
	void _scroll_range_changed();
	void _scroll_value_changed(double p_value);

protected:
	static void _bind_methods();

public:
	AIMessageList();
	void clear_messages();
	void add_message(const Dictionary &p_message);
	void update_message(int p_index, const Dictionary &p_message);
	void remove_message(int p_index);
	void scroll_to_bottom();
};
