/**************************************************************************/
/*  ai_message_bubble.cpp                                                  */
/**************************************************************************/

#include "ai_message_bubble.h"

#include "core/object/class_db.h"

void AIMessageBubble::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_message", "message"), &AIMessageBubble::set_message);
}

AIMessageBubble::AIMessageBubble() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	label = memnew(RichTextLabel);
	label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	label->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	label->set_fit_content(true);
	label->set_selection_enabled(true);
	add_child(label);
}

void AIMessageBubble::set_message(const Dictionary &p_message) {
	String role = p_message.get("role", "assistant");
	String content = p_message.get("content", "");

	String prefix;
	if (role == "user") {
		prefix = "[b]You[/b]\n";
	} else if (role == "error") {
		prefix = "[b]Error[/b]\n";
	} else {
		prefix = "[b]Assistant[/b]\n";
	}
	label->set_text(prefix + content);
}
