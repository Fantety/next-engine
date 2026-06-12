/**************************************************************************/
/*  ai_message_list.h                                                      */
/**************************************************************************/

#pragma once

#include "core/variant/array.h"
#include "core/variant/dictionary.h"

#include "scene/gui/box_container.h"
#include "scene/gui/scroll_container.h"

#include "editor/agent_ui/component/ai_message_bubble.h"

class AIMessageList : public ScrollContainer {
	GDCLASS(AIMessageList, ScrollContainer);

	struct BubbleEntry {
		AIMessageBubble *bubble = nullptr;
		int first_message_index = 0;
		int message_count = 0;
	};

	VBoxContainer *message_box = nullptr;
	Control *bottom_spacer = nullptr;
	Vector<Dictionary> messages;
	Vector<BubbleEntry> bubbles;
	Vector<int> message_to_bubble_indices;
	bool should_scroll_to_bottom = true;
	bool scroll_to_bottom_queued = false;
	bool scrolling_to_bottom = false;
	int pending_scroll_to_bottom_passes = 0;

	bool _is_at_bottom() const;
	void _request_scroll_to_bottom_if_needed();
	void _queue_scroll_to_bottom(int p_passes);
	void _scroll_to_bottom();
	void _content_layout_changed();
	void _scroll_range_changed();
	void _scroll_value_changed(double p_value);
	void _clear_bubbles();
	int _add_bubble(const Dictionary &p_message, int p_first_message_index, int p_message_count);
	void _update_bubble(int p_bubble_index);
	void _rebuild_bubbles();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	AIMessageList();
	void clear_messages();
	void set_messages(const Array &p_messages);
	void add_message(const Dictionary &p_message);
	void update_message(int p_index, const Dictionary &p_message);
	void remove_message(int p_index);
	void scroll_to_bottom();
};
