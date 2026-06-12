/**************************************************************************/
/*  ai_message_list.cpp                                                    */
/**************************************************************************/

#include "ai_message_list.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

namespace {

bool _message_has_tool_calls(const Dictionary &p_message) {
	if (!p_message.has("metadata") || Variant(p_message["metadata"]).get_type() != Variant::DICTIONARY) {
		return false;
	}

	Dictionary metadata = p_message["metadata"];
	if (!metadata.has("tool_calls") || Variant(metadata["tool_calls"]).get_type() != Variant::ARRAY) {
		return false;
	}

	Array tool_calls = metadata["tool_calls"];
	return !tool_calls.is_empty();
}

bool _is_tool_like_message(const Dictionary &p_message) {
	const String role = String(p_message.get("role", String()));
	if (role == "tool") {
		return true;
	}

	if (role == "assistant" && String(p_message.get("content", String())).strip_edges().is_empty()) {
		return _message_has_tool_calls(p_message);
	}

	return false;
}

Dictionary _make_tool_group_message(const Vector<Dictionary> &p_messages, int p_from, int p_to) {
	Array grouped_messages;
	for (int i = p_from; i < p_to; i++) {
		grouped_messages.push_back(p_messages[i]);
	}

	Dictionary metadata;
	metadata["messages"] = grouped_messages;

	Dictionary message;
	message["role"] = "tool_group";
	message["content"] = String();
	message["metadata"] = metadata;
	return message;
}

} // namespace

void AIMessageList::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear_messages"), &AIMessageList::clear_messages);
	ClassDB::bind_method(D_METHOD("set_messages", "messages"), &AIMessageList::set_messages);
	ClassDB::bind_method(D_METHOD("add_message", "message"), &AIMessageList::add_message);
	ClassDB::bind_method(D_METHOD("update_message", "index", "message"), &AIMessageList::update_message);
	ClassDB::bind_method(D_METHOD("remove_message", "index"), &AIMessageList::remove_message);
	ClassDB::bind_method(D_METHOD("scroll_to_bottom"), &AIMessageList::scroll_to_bottom);
}

AIMessageList::AIMessageList() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_EXPAND_FILL);
	set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);

	message_box = memnew(VBoxContainer);
	message_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	message_box->add_theme_constant_override("separation", 8);
	message_box->connect(SceneStringName(resized), callable_mp(this, &AIMessageList::_content_layout_changed), CONNECT_DEFERRED);
	message_box->connect(SceneStringName(minimum_size_changed), callable_mp(this, &AIMessageList::_content_layout_changed), CONNECT_DEFERRED);
	add_child(message_box);

	bottom_spacer = memnew(Control);
	bottom_spacer->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	bottom_spacer->set_custom_minimum_size(Size2(0, 8));
	message_box->add_child(bottom_spacer);

	connect(SceneStringName(resized), callable_mp(this, &AIMessageList::_content_layout_changed), CONNECT_DEFERRED);
	get_v_scroll_bar()->connect("changed", callable_mp(this, &AIMessageList::_scroll_range_changed), CONNECT_DEFERRED);
	get_v_scroll_bar()->connect(SceneStringName(value_changed), callable_mp(this, &AIMessageList::_scroll_value_changed));
}

void AIMessageList::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_RESIZED:
		case NOTIFICATION_THEME_CHANGED: {
			_content_layout_changed();
		} break;
	}
}

bool AIMessageList::_is_at_bottom() const {
	VScrollBar *scroll_bar = const_cast<AIMessageList *>(this)->get_v_scroll_bar();
	if (!scroll_bar || !scroll_bar->is_visible()) {
		return true;
	}

	const double bottom = scroll_bar->get_value() + scroll_bar->get_page();
	return bottom >= scroll_bar->get_max() - 2.0;
}

void AIMessageList::_request_scroll_to_bottom_if_needed() {
	if (!should_scroll_to_bottom) {
		return;
	}

	_queue_scroll_to_bottom(6);
}

void AIMessageList::_queue_scroll_to_bottom(int p_passes) {
	pending_scroll_to_bottom_passes = MAX(pending_scroll_to_bottom_passes, p_passes);
	if (scroll_to_bottom_queued) {
		return;
	}

	scroll_to_bottom_queued = true;
	callable_mp(this, &AIMessageList::_scroll_to_bottom).call_deferred();
}

void AIMessageList::_scroll_to_bottom() {
	scroll_to_bottom_queued = false;
	VScrollBar *scroll_bar = get_v_scroll_bar();
	if (!scroll_bar) {
		return;
	}

	scrolling_to_bottom = true;
	set_v_scroll(scroll_bar->get_max());
	if (bottom_spacer && bottom_spacer->is_inside_tree()) {
		ensure_control_visible(bottom_spacer);
	}
	scrolling_to_bottom = false;
	should_scroll_to_bottom = true;
	if (pending_scroll_to_bottom_passes > 1) {
		pending_scroll_to_bottom_passes--;
		_queue_scroll_to_bottom(pending_scroll_to_bottom_passes);
	} else {
		pending_scroll_to_bottom_passes = 0;
	}
}

void AIMessageList::_content_layout_changed() {
	_request_scroll_to_bottom_if_needed();
}

void AIMessageList::_scroll_range_changed() {
	if (should_scroll_to_bottom) {
		_queue_scroll_to_bottom(1);
	}
}

void AIMessageList::_scroll_value_changed(double p_value) {
	if (!scroll_to_bottom_queued && !scrolling_to_bottom) {
		should_scroll_to_bottom = _is_at_bottom();
	}
}

void AIMessageList::_clear_bubbles() {
	for (int i = 0; i < bubbles.size(); i++) {
		AIMessageBubble *bubble = bubbles[i].bubble;
		if (!bubble) {
			continue;
		}
		if (bubble->get_parent()) {
			bubble->get_parent()->remove_child(bubble);
		}
		memdelete(bubble);
	}
	bubbles.clear();
	message_to_bubble_indices.clear();
}

int AIMessageList::_add_bubble(const Dictionary &p_message, int p_first_message_index, int p_message_count) {
	AIMessageBubble *bubble = memnew(AIMessageBubble);
	BubbleEntry entry;
	entry.bubble = bubble;
	entry.first_message_index = p_first_message_index;
	entry.message_count = p_message_count;
	bubbles.push_back(entry);
	message_box->add_child(bubble);
	if (bottom_spacer) {
		message_box->move_child(bottom_spacer, message_box->get_child_count() - 1);
	}
	bubble->set_message(p_message);
	return bubbles.size() - 1;
}

void AIMessageList::_update_bubble(int p_bubble_index) {
	if (p_bubble_index < 0 || p_bubble_index >= bubbles.size()) {
		return;
	}

	BubbleEntry &entry = bubbles.write[p_bubble_index];
	if (!entry.bubble || entry.first_message_index < 0 || entry.first_message_index >= messages.size()) {
		return;
	}

	if (entry.message_count > 1) {
		entry.bubble->set_message(_make_tool_group_message(messages, entry.first_message_index, MIN(entry.first_message_index + entry.message_count, messages.size())));
	} else {
		entry.bubble->set_message(messages[entry.first_message_index]);
	}
}

void AIMessageList::_rebuild_bubbles() {
	_clear_bubbles();
	message_to_bubble_indices.resize(messages.size());

	for (int i = 0; i < messages.size();) {
		if (_is_tool_like_message(messages[i])) {
			int group_end = i + 1;
			while (group_end < messages.size() && _is_tool_like_message(messages[group_end])) {
				group_end++;
			}

			if (group_end - i > 1) {
				const int bubble_index = _add_bubble(_make_tool_group_message(messages, i, group_end), i, group_end - i);
				for (int j = i; j < group_end; j++) {
					message_to_bubble_indices.write[j] = bubble_index;
				}
				i = group_end;
				continue;
			}
		}

		const int bubble_index = _add_bubble(messages[i], i, 1);
		message_to_bubble_indices.write[i] = bubble_index;
		i++;
	}

	if (bottom_spacer) {
		message_box->move_child(bottom_spacer, message_box->get_child_count() - 1);
	}
}

void AIMessageList::clear_messages() {
	should_scroll_to_bottom = true;
	pending_scroll_to_bottom_passes = 0;
	scroll_to_bottom_queued = false;
	scrolling_to_bottom = false;
	messages.clear();
	_clear_bubbles();
	set_v_scroll(0);
}

void AIMessageList::set_messages(const Array &p_messages) {
	should_scroll_to_bottom = true;
	pending_scroll_to_bottom_passes = 0;
	scroll_to_bottom_queued = false;
	scrolling_to_bottom = false;
	messages.clear();
	_clear_bubbles();

	for (int i = 0; i < p_messages.size(); i++) {
		if (Variant(p_messages[i]).get_type() == Variant::DICTIONARY) {
			messages.push_back(p_messages[i]);
		}
	}

	_rebuild_bubbles();
	set_v_scroll(0);
	_request_scroll_to_bottom_if_needed();
}

void AIMessageList::add_message(const Dictionary &p_message) {
	should_scroll_to_bottom = scroll_to_bottom_queued || _is_at_bottom();
	const int new_message_index = messages.size();
	const bool append_to_previous_tool_group = new_message_index > 0 && _is_tool_like_message(messages[new_message_index - 1]) && _is_tool_like_message(p_message);
	messages.push_back(p_message);

	if (append_to_previous_tool_group && message_to_bubble_indices.size() == new_message_index && !bubbles.is_empty()) {
		const int bubble_index = message_to_bubble_indices[new_message_index - 1];
		if (bubble_index >= 0 && bubble_index < bubbles.size() && bubbles[bubble_index].first_message_index + bubbles[bubble_index].message_count == new_message_index) {
			bubbles.write[bubble_index].message_count++;
			message_to_bubble_indices.push_back(bubble_index);
			_update_bubble(bubble_index);
		} else {
			_rebuild_bubbles();
		}
	} else if (message_to_bubble_indices.size() == new_message_index) {
		const int bubble_index = _add_bubble(p_message, new_message_index, 1);
		message_to_bubble_indices.push_back(bubble_index);
	} else {
		_rebuild_bubbles();
	}
	_request_scroll_to_bottom_if_needed();
}

void AIMessageList::update_message(int p_index, const Dictionary &p_message) {
	if (p_index < 0 || p_index >= messages.size()) {
		return;
	}
	should_scroll_to_bottom = scroll_to_bottom_queued || _is_at_bottom();
	const Dictionary old_message = messages[p_index];
	const bool grouping_changed = _is_tool_like_message(old_message) != _is_tool_like_message(p_message);
	messages.write[p_index] = p_message;
	if (grouping_changed || p_index >= message_to_bubble_indices.size()) {
		_rebuild_bubbles();
	} else {
		const int bubble_index = message_to_bubble_indices[p_index];
		if (bubble_index >= 0 && bubble_index < bubbles.size()) {
			_update_bubble(bubble_index);
		} else {
			_rebuild_bubbles();
		}
	}
	_request_scroll_to_bottom_if_needed();
}

void AIMessageList::remove_message(int p_index) {
	if (p_index < 0 || p_index >= messages.size()) {
		return;
	}

	should_scroll_to_bottom = scroll_to_bottom_queued || _is_at_bottom();
	messages.remove_at(p_index);
	_rebuild_bubbles();
	_request_scroll_to_bottom_if_needed();
}

void AIMessageList::scroll_to_bottom() {
	should_scroll_to_bottom = true;
	_queue_scroll_to_bottom(6);
}
