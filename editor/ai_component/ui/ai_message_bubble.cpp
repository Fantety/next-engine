/**************************************************************************/
/*  ai_message_bubble.cpp                                                  */
/**************************************************************************/

#include "ai_message_bubble.h"

#include "ai_markdown_label.h"

#include "core/io/json.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/label.h"
#include "scene/resources/style_box_flat.h"
#include "servers/text/text_server.h"

void AIMessageBubble::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_message", "message"), &AIMessageBubble::set_message);
}

namespace {

static const int TOOL_SUMMARY_LIMIT = 72;

String _variant_to_text(const Variant &p_value) {
	if (p_value.get_type() == Variant::NIL) {
		return String();
	}
	if (p_value.get_type() == Variant::DICTIONARY || p_value.get_type() == Variant::ARRAY) {
		return JSON::stringify(p_value);
	}
	return String(p_value);
}

String _single_line_summary(const String &p_text, int p_limit = TOOL_SUMMARY_LIMIT) {
	String summary = p_text.strip_edges();
	summary = summary.replace("\r\n", " ");
	summary = summary.replace("\n", " ");
	summary = summary.replace("\t", " ");

	while (summary.contains("  ")) {
		summary = summary.replace("  ", " ");
	}

	if (summary.length() > p_limit) {
		summary = summary.substr(0, MAX(0, p_limit - 3)) + "...";
	}

	return summary;
}

String _build_tool_call_summary(const Array &p_tool_calls) {
	PackedStringArray names;
	String first_arguments;
	for (int i = 0; i < p_tool_calls.size(); i++) {
		if (Variant(p_tool_calls[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary call = p_tool_calls[i];
		names.push_back(String(call.get("tool_name", "tool")));
		if (first_arguments.is_empty() && call.has("arguments")) {
			first_arguments = _variant_to_text(call["arguments"]);
		}
	}

	String summary = names.is_empty() ? "tool" : String(", ").join(names);
	if (!first_arguments.is_empty()) {
		summary += " " + first_arguments;
	}
	return _single_line_summary(summary);
}

String _build_tool_call_details(const Array &p_tool_calls) {
	PackedStringArray lines;
	for (int i = 0; i < p_tool_calls.size(); i++) {
		if (Variant(p_tool_calls[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary call = p_tool_calls[i];
		const String tool_name = String(call.get("tool_name", "tool"));
		lines.push_back(tool_name);
		if (call.has("arguments")) {
			lines.push_back(_variant_to_text(call["arguments"]));
		}
	}

	return String("\n").join(lines);
}

bool _is_mcp_tool_metadata(const Dictionary &p_metadata) {
	return String(p_metadata.get("tool_origin", String())) == "mcp" || p_metadata.has("mcp_server_name") || p_metadata.has("mcp_tool_name");
}

String _build_mcp_tool_title(const Dictionary &p_metadata) {
	const String server_name = String(p_metadata.get("mcp_server_name", p_metadata.get("mcp_server_id", String()))).strip_edges();
	const String mcp_tool_name = String(p_metadata.get("mcp_tool_name", p_metadata.get("tool_name", "tool"))).strip_edges();
	String title = "MCP";
	if (!server_name.is_empty() || !mcp_tool_name.is_empty()) {
		title += ": ";
		if (!server_name.is_empty()) {
			title += server_name;
		}
		if (!server_name.is_empty() && !mcp_tool_name.is_empty()) {
			title += " / ";
		}
		if (!mcp_tool_name.is_empty()) {
			title += mcp_tool_name;
		}
	}
	return title;
}

String _build_mcp_tool_details(const Dictionary &p_metadata, const String &p_content) {
	PackedStringArray lines;
	const String server_name = String(p_metadata.get("mcp_server_name", String())).strip_edges();
	const String server_id = String(p_metadata.get("mcp_server_id", String())).strip_edges();
	const String mcp_tool_name = String(p_metadata.get("mcp_tool_name", String())).strip_edges();
	const String transport = String(p_metadata.get("mcp_transport", String())).strip_edges();
	const String agent_tool_name = String(p_metadata.get("mcp_agent_tool_name", p_metadata.get("tool_name", String()))).strip_edges();

	if (!server_name.is_empty()) {
		lines.push_back("MCP Server: " + server_name);
	}
	if (!server_id.is_empty()) {
		lines.push_back("Server ID: " + server_id);
	}
	if (!mcp_tool_name.is_empty()) {
		lines.push_back("MCP Tool: " + mcp_tool_name);
	}
	if (!transport.is_empty()) {
		lines.push_back("Transport: " + transport);
	}
	if (!agent_tool_name.is_empty()) {
		lines.push_back("Agent Tool: " + agent_tool_name);
	}
	if (!p_content.is_empty()) {
		if (!lines.is_empty()) {
			lines.push_back(String());
		}
		lines.push_back(p_content);
	}
	return String("\n").join(lines);
}

bool _tool_details_need_toggle(const String &p_summary, const String &p_details, int p_call_count = 1) {
	return p_call_count > 1 || p_details.contains("\n") || p_details.length() > p_summary.length();
}

bool _looks_like_markdown(const String &p_text) {
	return p_text.contains("\n") ||
			p_text.contains("**") ||
			p_text.contains("`") ||
			p_text.contains("](") ||
			p_text.begins_with("#") ||
			p_text.begins_with("- ") ||
			p_text.begins_with("* ") ||
			p_text.contains("\n#") ||
			p_text.contains("\n- ") ||
			p_text.contains("\n* ") ||
			p_text.contains("|");
}

String _get_tool_message_title(const Dictionary &p_message) {
	Dictionary metadata;
	if (p_message.has("metadata") && Variant(p_message["metadata"]).get_type() == Variant::DICTIONARY) {
		metadata = p_message["metadata"];
	}

	const String role = String(p_message.get("role", String()));
	if (role == "assistant" && metadata.has("tool_calls") && Variant(metadata["tool_calls"]).get_type() == Variant::ARRAY) {
		Array tool_calls = metadata["tool_calls"];
		if (tool_calls.size() == 1 && Variant(tool_calls[0]).get_type() == Variant::DICTIONARY) {
			Dictionary call = tool_calls[0];
			return String(call.get("tool_name", "tool"));
		}
		return vformat("%d tool request(s)", tool_calls.size());
	}

	if (_is_mcp_tool_metadata(metadata)) {
		return _build_mcp_tool_title(metadata);
	}

	return String(metadata.get("tool_name", "tool"));
}

String _build_tool_group_summary(const Array &p_messages) {
	PackedStringArray tool_names;
	for (int i = 0; i < p_messages.size(); i++) {
		if (Variant(p_messages[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary message = p_messages[i];
		String status;
		if (message.has("metadata") && Variant(message["metadata"]).get_type() == Variant::DICTIONARY) {
			Dictionary metadata = message["metadata"];
			status = String(metadata.get("status", String()));
		}
		String title = _get_tool_message_title(message);
		if (!status.is_empty()) {
			title += " " + status;
		}
		tool_names.push_back(title);
	}

	String summary = vformat("%d tool event(s)", p_messages.size());
	if (!tool_names.is_empty()) {
		summary += ": " + String(", ").join(tool_names);
	}
	return _single_line_summary(summary, 120);
}

String _build_tool_group_details(const Array &p_messages) {
	PackedStringArray sections;
	for (int i = 0; i < p_messages.size(); i++) {
		if (Variant(p_messages[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary message = p_messages[i];
		const String role = String(message.get("role", String()));
		const String content = String(message.get("content", String()));

		Dictionary metadata;
		if (message.has("metadata") && Variant(message["metadata"]).get_type() == Variant::DICTIONARY) {
			metadata = message["metadata"];
		}

		String section = vformat("%d. %s", sections.size() + 1, _get_tool_message_title(message));
		if (role == "assistant" && metadata.has("tool_calls") && Variant(metadata["tool_calls"]).get_type() == Variant::ARRAY) {
			const String call_details = _build_tool_call_details(metadata["tool_calls"]);
			if (!call_details.is_empty()) {
				section += "\n" + call_details;
			}
		} else if (_is_mcp_tool_metadata(metadata)) {
			const String mcp_details = _build_mcp_tool_details(metadata, content);
			if (!mcp_details.is_empty()) {
				section += "\n" + mcp_details;
			}
		} else if (!content.is_empty()) {
			section += "\n" + content;
		}
		sections.push_back(section);
	}

	return String("\n\n").join(sections);
}

String _attachment_label(const Dictionary &p_attachment) {
	const String path = String(p_attachment.get("path", String()));
	if (path.is_empty()) {
		return TTR("Image");
	}
	return path.get_file();
}

Ref<StyleBoxFlat> _make_bubble_style(const Color &p_bg_color, const Color &p_border_color) {
	Ref<StyleBoxFlat> style;
	style.instantiate();
	style->set_bg_color(p_bg_color);
	style->set_border_color(p_border_color);
	style->set_border_width_all(MAX(1, int(EDSCALE)));
	style->set_border_width(SIDE_LEFT, MAX(1, int(3 * EDSCALE)));
	style->set_border_width(SIDE_BOTTOM, MAX(2, int(2 * EDSCALE)));
	style->set_content_margin_individual(10 * EDSCALE, 7 * EDSCALE, 10 * EDSCALE, 8 * EDSCALE);
	style->set_corner_radius_all(6 * EDSCALE);
	style->set_anti_aliased(false);
	return style;
}

void _apply_bubble_style(AIMessageBubble *p_bubble, Label *p_title_label, const String &p_role, bool p_is_tool_bubble) {
	ERR_FAIL_NULL(p_bubble);
	ERR_FAIL_NULL(p_title_label);

	Color bg_color(0.15, 0.16, 0.18, 0.38);
	Color border_color(0.68, 0.72, 0.78, 0.22);
	Color title_color(0.84, 0.86, 0.9, 1.0);

	if (p_role == "user") {
		bg_color = Color(0.08, 0.20, 0.26, 0.48);
		border_color = Color(0.32, 0.75, 0.92, 0.72);
		title_color = Color(0.58, 0.86, 1.0, 1.0);
	} else if (p_is_tool_bubble) {
		bg_color = Color(0.20, 0.17, 0.10, 0.50);
		border_color = Color(0.95, 0.68, 0.28, 0.74);
		title_color = Color(1.0, 0.78, 0.44, 1.0);
	} else if (p_role == "error") {
		bg_color = Color(0.28, 0.08, 0.08, 0.52);
		border_color = Color(1.0, 0.34, 0.30, 0.78);
		title_color = Color(1.0, 0.58, 0.54, 1.0);
	}

	p_bubble->add_theme_style_override(SceneStringName(panel), _make_bubble_style(bg_color, border_color));
	p_title_label->add_theme_color_override(SceneStringName(font_color), title_color);
}

} // namespace

AIMessageBubble::AIMessageBubble() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);

	content_box = memnew(VBoxContainer);
	content_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content_box->add_theme_constant_override("separation", 2 * EDSCALE);
	add_child(content_box);

	header_box = memnew(HBoxContainer);
	header_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header_box->add_theme_constant_override("separation", 4 * EDSCALE);
	content_box->add_child(header_box);

	title_label = memnew(Label);
	title_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title_label->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	header_box->add_child(title_label);

	details_button = memnew(LinkButton);
	details_button->set_h_size_flags(Control::SIZE_SHRINK_END);
	details_button->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	details_button->set_underline_mode(LinkButton::UNDERLINE_MODE_ON_HOVER);
	details_button->connect(SceneStringName(pressed), callable_mp(this, &AIMessageBubble::_toggle_details));
	details_button->hide();
	header_box->add_child(details_button);

	label = memnew(AIMarkdownLabel);
	label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	label->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	content_box->add_child(label);

	attachments_box = memnew(HFlowContainer);
	attachments_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	attachments_box->add_theme_constant_override("h_separation", 4 * EDSCALE);
	attachments_box->add_theme_constant_override("v_separation", 3 * EDSCALE);
	attachments_box->hide();
	content_box->add_child(attachments_box);
}

void AIMessageBubble::set_message(const Dictionary &p_message) {
	current_message = p_message;
	details_expanded = false;
	_render_message();
}

void AIMessageBubble::_render_message() {
	String role = current_message.get("role", "assistant");
	String content;
	if (current_message.has("content") && Variant(current_message["content"]).get_type() != Variant::NIL) {
		content = String(current_message["content"]);
	}
	Dictionary message_metadata;
	if (current_message.has("metadata") && Variant(current_message["metadata"]).get_type() == Variant::DICTIONARY) {
		message_metadata = current_message["metadata"];
	}

	Array tool_calls;
	if (message_metadata.has("tool_calls") && Variant(message_metadata["tool_calls"]).get_type() == Variant::ARRAY) {
		tool_calls = message_metadata["tool_calls"];
	}
	Array grouped_tool_messages;
	if (message_metadata.has("messages") && Variant(message_metadata["messages"]).get_type() == Variant::ARRAY) {
		grouped_tool_messages = message_metadata["messages"];
	}

	String title;
	if (role == "user") {
		title = "You";
	} else if (role == "tool_group") {
		title = vformat("Tool Calls (%d)", grouped_tool_messages.size());
	} else if (role == "assistant" && content.strip_edges().is_empty() && !tool_calls.is_empty()) {
		title = "Tool Call";
	} else if (role == "tool") {
		String tool_name = _is_mcp_tool_metadata(message_metadata) ? _build_mcp_tool_title(message_metadata) : String("Tool: ") + String(message_metadata.get("tool_name", "tool"));
		String status = String(message_metadata.get("status", ""));
		title = tool_name;
		if (!status.is_empty()) {
			title += " (" + status + ")";
		}
	} else if (role == "error") {
		title = "Error";
	} else {
		title = "Assistant";
	}

	const bool is_pure_assistant_tool_call = role == "assistant" && content.strip_edges().is_empty() && !tool_calls.is_empty();
	const bool is_assistant_with_tool_calls = role == "assistant" && !content.strip_edges().is_empty() && !tool_calls.is_empty();
	const bool is_tool_group = role == "tool_group";
	const bool is_tool_bubble = (role == "tool") || is_pure_assistant_tool_call || is_tool_group;
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	_apply_bubble_style(this, title_label, role, is_tool_bubble);
	title_label->remove_theme_font_size_override(SceneStringName(font_size));
	label->remove_theme_font_size_override("normal_font_size");
	label->remove_theme_font_size_override("bold_font_size");
	details_button->remove_theme_font_size_override(SceneStringName(font_size));
	if (is_tool_bubble) {
		title_label->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
		label->add_theme_font_size_override("normal_font_size", int(11 * EDSCALE));
		label->add_theme_font_size_override("bold_font_size", int(11 * EDSCALE));
		details_button->add_theme_font_size_override(SceneStringName(font_size), int(10 * EDSCALE));
	}

	String summary = content;
	String details = content;
	int tool_call_count = 0;
	if (is_tool_group) {
		tool_call_count = grouped_tool_messages.size();
		summary = _build_tool_group_summary(grouped_tool_messages);
		details = _build_tool_group_details(grouped_tool_messages);
	} else if (is_pure_assistant_tool_call) {
		tool_call_count = tool_calls.size();
		summary = _build_tool_call_summary(tool_calls);
		details = _build_tool_call_details(tool_calls);
	} else if (role == "tool") {
		summary = _single_line_summary(content);
		details = _is_mcp_tool_metadata(message_metadata) ? _build_mcp_tool_details(message_metadata, content) : content;
	}

	details_available = is_tool_bubble && _tool_details_need_toggle(summary, details, tool_call_count);

	title_label->set_text(title);
	label->clear();

	if (is_tool_bubble) {
		label->add_text(details_expanded ? details : summary);
		details_button->set_text(details_expanded ? TTR("Hide") : TTR("Details"));
		details_button->set_visible(details_available);
	} else if (role == "assistant") {
		if (is_assistant_with_tool_calls) {
			const String tool_summary = _build_tool_call_summary(tool_calls);
			label->set_markdown(content + "\n\n`" + tool_summary + "`");
		} else if (_looks_like_markdown(content)) {
			label->set_markdown(content);
		} else {
			label->add_text(content);
		}
		details_button->hide();
	} else {
		label->add_text(content);
		details_button->hide();
	}

	_render_attachments(message_metadata);
}

void AIMessageBubble::_render_attachments(const Dictionary &p_metadata) {
	if (!attachments_box) {
		return;
	}
	while (attachments_box->get_child_count() > 0) {
		Node *child = attachments_box->get_child(0);
		attachments_box->remove_child(child);
		memdelete(child);
	}

	if (!p_metadata.has("attachments") || Variant(p_metadata["attachments"]).get_type() != Variant::ARRAY) {
		attachments_box->hide();
		return;
	}

	Array attachments = p_metadata["attachments"];
	for (int i = 0; i < attachments.size(); i++) {
		if (Variant(attachments[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary attachment = attachments[i];
		const String label_text = _attachment_label(attachment);

		HBoxContainer *chip = memnew(HBoxContainer);
		chip->set_name(SNAME("AIMessageAttachmentChip"));
		chip->add_theme_constant_override("separation", 3 * EDSCALE);

		Button *icon = memnew(Button);
		icon->set_button_icon(get_editor_theme_icon(SNAME("Attachment")));
		icon->set_disabled(true);
		chip->add_child(icon);

		Label *attachment_label = memnew(Label);
		attachment_label->set_text(label_text);
		attachment_label->set_tooltip_text(String(attachment.get("path", String())));
		attachment_label->set_custom_minimum_size(Size2(72, 0) * EDSCALE);
		attachment_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		attachment_label->set_clip_text(true);
		chip->add_child(attachment_label);
		attachments_box->add_child(chip);
	}

	attachments_box->set_visible(attachments_box->get_child_count() > 0);
}

void AIMessageBubble::_toggle_details() {
	if (!details_available) {
		return;
	}

	details_expanded = !details_expanded;
	_render_message();
}
