/**************************************************************************/
/*  ai_next_task_inspector.cpp                                            */
/**************************************************************************/

#include "ai_next_task_inspector.h"

#include "core/object/class_db.h"
#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/markdown_viewer.h"

namespace {

String _join_or_none(const Array &p_values) {
	String joined;
	for (int i = 0; i < p_values.size(); i++) {
		const String value = String(p_values[i]).strip_edges();
		if (value.is_empty()) {
			continue;
		}
		if (!joined.is_empty()) {
			joined += ", ";
		}
		joined += value;
	}
	return joined.is_empty() ? TTR("None") : joined;
}

String _format_task_inspector_markdown(const Dictionary &p_task) {
	if (p_task.is_empty()) {
		return TTR("No task selected");
	}

	String markdown = vformat("### %s\n\n", String(p_task.get("title", TTR("Task"))));
	markdown += vformat("- **Status:** %s\n", String(p_task.get("status", "pending")).capitalize());
	markdown += vformat("- **Agent:** %s\n", String(p_task.get("assigned_agent_id", String())));
	markdown += vformat("- **Depends:** %s\n", _join_or_none(p_task.get("depends_on", Array())));
	markdown += vformat("- **Outputs:** %s\n", _join_or_none(p_task.get("output_paths", Array())));

	const String description = String(p_task.get("description", String())).strip_edges();
	if (!description.is_empty()) {
		markdown += "\n#### Description\n\n";
		markdown += description;
		markdown += "\n";
	}

	const String error = String(p_task.get("error", String())).strip_edges();
	if (!error.is_empty()) {
		markdown += "\n#### Error\n\n";
		markdown += error;
		markdown += "\n";
		return markdown;
	}

	const String result = String(p_task.get("result_summary", String())).strip_edges();
	if (!result.is_empty()) {
		markdown += "\n#### Result\n\n";
		markdown += result;
		markdown += "\n";
	}
	return markdown;
}

} // namespace

void AINextTaskInspector::_bind_methods() {
	ClassDB::bind_method(D_METHOD("refresh"), &AINextTaskInspector::refresh);
}

AINextTaskInspector::AINextTaskInspector() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 3 * EDSCALE);

	detail_viewer = memnew(MarkdownViewer);
	detail_viewer->set_name(SNAME("TaskInspectorMarkdown"));
	detail_viewer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	detail_viewer->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	detail_viewer->set_custom_minimum_size(Size2(0, 156) * EDSCALE);
	detail_viewer->set_remote_images_enabled(false);
	detail_viewer->set_open_links_enabled(false);
	detail_viewer->set_scroll_enabled(true);
	detail_viewer->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
	add_child(detail_viewer);
}

void AINextTaskInspector::set_next_session(AIAgentNextSession *p_session) {
	next_session = p_session;
	refresh();
}

void AINextTaskInspector::refresh() {
	Dictionary task;
	if (next_session && next_session->get_project_state().is_valid()) {
		const String selected_task_id = next_session->get_selected_task_id();
		if (!selected_task_id.is_empty()) {
			task = next_session->get_project_state()->get_task(selected_task_id);
		}
		if (task.is_empty()) {
			const String milestone_id = next_session->get_project_state()->get_active_milestone_id();
			Dictionary milestone = next_session->get_project_state()->get_milestone(milestone_id);
			Array tasks = milestone.get("tasks", Array());
			if (!tasks.is_empty() && Variant(tasks[0]).get_type() == Variant::DICTIONARY) {
				Dictionary first_task = tasks[0];
				task = next_session->get_project_state()->get_task(String(first_task.get("id", String())));
			}
		}
	}

	if (detail_viewer) {
		detail_viewer->set_markdown(_format_task_inspector_markdown(task));
	}
}
