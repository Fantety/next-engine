/**************************************************************************/
/*  ai_message_bubble.cpp                                                  */
/**************************************************************************/

#include "ai_message_bubble.h"

#include "core/io/json.h"
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
	String content;
	if (p_message.has("content") && Variant(p_message["content"]).get_type() != Variant::NIL) {
		content = String(p_message["content"]);
	}
	Dictionary message_metadata;
	if (p_message.has("metadata") && Variant(p_message["metadata"]).get_type() == Variant::DICTIONARY) {
		message_metadata = p_message["metadata"];
	}

	Array tool_calls;
	if (message_metadata.has("tool_calls") && Variant(message_metadata["tool_calls"]).get_type() == Variant::ARRAY) {
		tool_calls = message_metadata["tool_calls"];
	}

	String title;
	if (role == "user") {
		title = "You";
	} else if (role == "assistant" && !tool_calls.is_empty()) {
		title = "Tool Call";
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
	if (role == "assistant" && !tool_calls.is_empty()) {
		for (int i = 0; i < tool_calls.size(); i++) {
			if (Variant(tool_calls[i]).get_type() != Variant::DICTIONARY) {
				continue;
			}

			Dictionary call = tool_calls[i];
			const String tool_name = String(call.get("tool_name", "tool"));
			label->add_text(tool_name);
			if (call.has("arguments") && Variant(call["arguments"]).get_type() == Variant::DICTIONARY) {
				label->add_text("\n");
				label->add_text(JSON::stringify(call["arguments"]));
			}
			if (i < tool_calls.size() - 1) {
				label->add_text("\n");
			}
		}
	} else {
		label->add_text(content);
	}
}
