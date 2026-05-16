/**************************************************************************/
/*  ai_message_list.cpp                                                    */
/**************************************************************************/

#include "ai_message_list.h"

#include "core/object/class_db.h"

void AIMessageList::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear_messages"), &AIMessageList::clear_messages);
	ClassDB::bind_method(D_METHOD("add_message", "message"), &AIMessageList::add_message);
	ClassDB::bind_method(D_METHOD("update_message", "index", "message"), &AIMessageList::update_message);
}

AIMessageList::AIMessageList() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_EXPAND_FILL);

	message_box = memnew(VBoxContainer);
	message_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	message_box->add_theme_constant_override("separation", 8);
	add_child(message_box);
}

void AIMessageList::clear_messages() {
	for (int i = 0; i < bubbles.size(); i++) {
		bubbles[i]->queue_free();
	}
	bubbles.clear();
}

void AIMessageList::add_message(const Dictionary &p_message) {
	AIMessageBubble *bubble = memnew(AIMessageBubble);
	bubble->set_message(p_message);
	bubbles.push_back(bubble);
	message_box->add_child(bubble);
}

void AIMessageList::update_message(int p_index, const Dictionary &p_message) {
	if (p_index < 0 || p_index >= bubbles.size()) {
		return;
	}
	bubbles[p_index]->set_message(p_message);
}
