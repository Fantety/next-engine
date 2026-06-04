/**************************************************************************/
/*  ai_next_feedback_panel.cpp                                            */
/**************************************************************************/

#include "ai_next_feedback_panel.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/ai_component/ui/ai_attachment_bar.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/text_edit.h"
#include "servers/text/text_server.h"

void AINextFeedbackPanel::_bind_methods() {
}

AINextFeedbackPanel::AINextFeedbackPanel() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 4 * EDSCALE);

	feedback_input = memnew(TextEdit);
	feedback_input->set_custom_minimum_size(Size2(0, 58) * EDSCALE);
	feedback_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	feedback_input->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	feedback_input->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	feedback_input->set_placeholder(TTR("Playtest feedback"));
	feedback_input->connect(SNAME("text_changed"), callable_mp(this, &AINextFeedbackPanel::refresh));
	add_child(feedback_input);

	attachment_bar = memnew(AIAttachmentBar);
	add_child(attachment_bar);

	HBoxContainer *actions = memnew(HBoxContainer);
	actions->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	actions->add_theme_constant_override("separation", 4 * EDSCALE);
	add_child(actions);

	generate_button = memnew(Button);
	generate_button->set_text(TTR("Generate Fix Tasks"));
	generate_button->connect(SceneStringName(pressed), callable_mp(this, &AINextFeedbackPanel::_generate_pressed));
	actions->add_child(generate_button);

	lock_button = memnew(Button);
	lock_button->set_text(TTR("Accept & Lock"));
	lock_button->connect(SceneStringName(pressed), callable_mp(this, &AINextFeedbackPanel::_lock_pressed));
	actions->add_child(lock_button);
}

void AINextFeedbackPanel::set_next_session(AIAgentNextSession *p_session) {
	next_session = p_session;
	refresh();
}

void AINextFeedbackPanel::_generate_pressed() {
	if (!next_session || !feedback_input) {
		return;
	}
	const Array attachments = attachment_bar ? attachment_bar->get_attachments() : Array();
	next_session->generate_feedback_tasks(feedback_input->get_text(), attachments);
	if (attachment_bar) {
		attachment_bar->clear_attachments();
	}
}

void AINextFeedbackPanel::_lock_pressed() {
	if (!next_session) {
		return;
	}
	next_session->accept_and_lock_active_milestone();
}

void AINextFeedbackPanel::refresh() {
	const bool running = next_session && next_session->is_workflow_active();
	if (generate_button) {
		const bool has_feedback = feedback_input && !feedback_input->get_text().strip_edges().is_empty();
		generate_button->set_disabled(!next_session || running || !has_feedback);
	}
	if (lock_button) {
		lock_button->set_disabled(!next_session || running || !next_session->can_lock_active_milestone());
	}
}
