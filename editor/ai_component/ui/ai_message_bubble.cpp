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
	label->set_use_bbcode(false);
	add_child(label);
}

void AIMessageBubble::set_message(const Dictionary &p_message) {
	String role = p_message.get("role", "assistant");
	String content = p_message.get("content", "");
	Dictionary message_metadata;
	if (p_message.has("metadata") && Variant(p_message["metadata"]).get_type() == Variant::DICTIONARY) {
		message_metadata = p_message["metadata"];
	}

	String title;
	if (role == "user") {
		title = "You";
	} else if (role == "tool") {
		String tool_name = String(message_metadata.get("tool_name", "tool"));
		String status = String(message_metadata.get("status", ""));
		title = "Tool: " + tool_name;
		if (!status.is_empty()) {
			title += " (" + status + ")";
		}
	} else if (role == "error") {
		title = "Error";
	} else {
		title = "Assistant";
	}

	label->clear();
	label->push_bold();
	label->add_text(title);
	label->pop();
	label->add_text("\n");
	label->add_text(content);
}
