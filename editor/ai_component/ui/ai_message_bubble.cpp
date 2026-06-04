/**************************************************************************/
/*  ai_message_bubble.cpp                                                  */
/**************************************************************************/

#include "ai_message_bubble.h"

#include "ai_markdown_label.h"

#include "core/io/json.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/rich_text_label.h"
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

	String title;
	if (role == "user") {
		title = "You";
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
	const bool is_tool_bubble = (role == "tool") || is_pure_assistant_tool_call;
	set_h_size_flags(is_tool_bubble && !details_expanded ? Control::SIZE_SHRINK_BEGIN : Control::SIZE_EXPAND_FILL);
	label->set_autowrap_mode(is_tool_bubble && !details_expanded ? TextServer::AUTOWRAP_OFF : TextServer::AUTOWRAP_WORD_SMART);
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
	if (is_pure_assistant_tool_call) {
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
}

void AIMessageBubble::_toggle_details() {
	if (!details_available) {
		return;
	}

	details_expanded = !details_expanded;
	_render_message();
}
