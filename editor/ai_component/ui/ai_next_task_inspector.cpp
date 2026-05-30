/**************************************************************************/
/*  ai_next_task_inspector.cpp                                            */
/**************************************************************************/

#include "ai_next_task_inspector.h"

#include "core/object/class_db.h"
#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/label.h"
#include "servers/text/text_server.h"

void AINextTaskInspector::_bind_methods() {
	ClassDB::bind_method(D_METHOD("refresh"), &AINextTaskInspector::refresh);
}

AINextTaskInspector::AINextTaskInspector() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 3 * EDSCALE);

	title_label = memnew(Label);
	title_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	add_child(title_label);

	agent_label = memnew(Label);
	agent_label->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
	add_child(agent_label);

	depends_label = memnew(Label);
	depends_label->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
	depends_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	add_child(depends_label);

	outputs_label = memnew(Label);
	outputs_label->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
	outputs_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	add_child(outputs_label);
}

void AINextTaskInspector::set_next_session(AIAgentNextSession *p_session) {
	next_session = p_session;
	refresh();
}

void AINextTaskInspector::refresh() {
	Dictionary task;
	if (next_session && next_session->get_project_state().is_valid()) {
		const String milestone_id = next_session->get_project_state()->get_active_milestone_id();
		Dictionary milestone = next_session->get_project_state()->get_milestone(milestone_id);
		Array tasks = milestone.get("tasks", Array());
		if (!tasks.is_empty() && Variant(tasks[0]).get_type() == Variant::DICTIONARY) {
			task = tasks[0];
		}
	}

	if (task.is_empty()) {
		title_label->set_text(TTR("No task selected"));
		agent_label->set_text(String());
		depends_label->set_text(String());
		outputs_label->set_text(String());
		return;
	}

	title_label->set_text(String(task.get("title", TTR("Task"))));
	agent_label->set_text(vformat(TTR("Agent: %s"), String(task.get("assigned_agent_id", String()))));
	depends_label->set_text(vformat(TTR("Depends: %s"), String(", ").join(task.get("depends_on", Array()))));
	outputs_label->set_text(vformat(TTR("Outputs: %s"), String(", ").join(task.get("output_paths", Array()))));
}
