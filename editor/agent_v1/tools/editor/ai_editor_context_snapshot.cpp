/**************************************************************************/
/*  ai_editor_context_snapshot.cpp                                         */
/**************************************************************************/

#include "ai_editor_context_snapshot.h"

#include "core/config/project_settings.h"
#include "core/version.h"
#include "editor/editor_node.h"
#include "scene/2d/node_2d.h"
#include "scene/3d/node_3d.h"
#include "scene/gui/control.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"

namespace {

Dictionary _make_size_dict(const Size2i &p_size) {
	Dictionary dict;
	dict["width"] = p_size.width;
	dict["height"] = p_size.height;
	return dict;
}

Dictionary _make_size_dict(const Size2 &p_size) {
	Dictionary dict;
	dict["width"] = p_size.x;
	dict["height"] = p_size.y;
	return dict;
}

Dictionary _make_rect_dict(const Rect2 &p_rect) {
	Dictionary dict;
	dict["x"] = p_rect.position.x;
	dict["y"] = p_rect.position.y;
	dict["width"] = p_rect.size.x;
	dict["height"] = p_rect.size.y;
	return dict;
}

String _format_size(const Size2i &p_size) {
	return vformat("%d x %d px", p_size.width, p_size.height);
}

String _format_size(const Size2 &p_size) {
	return vformat("%.0f x %.0f px", p_size.x, p_size.y);
}

String _scene_root_kind(Node *p_scene_root) {
	if (!p_scene_root) {
		return "none";
	}
	if (Object::cast_to<Control>(p_scene_root)) {
		return "ui_control";
	}
	if (Object::cast_to<Node2D>(p_scene_root)) {
		return "node_2d";
	}
	if (Object::cast_to<Node3D>(p_scene_root)) {
		return "node_3d";
	}
	return "node";
}

} // namespace

void AIV1EditorContextSnapshotService::_bind_methods() {
}

void AIV1EditorContextSnapshotService::_execute_request(uint64_t p_request_ptr) {
	_execute_request_ptr(reinterpret_cast<MainThreadRequest *>(p_request_ptr));
}

void AIV1EditorContextSnapshotService::_execute_request_ptr(MainThreadRequest *p_request) {
	ERR_FAIL_NULL(p_request);
	p_request->result = _collect_main_thread(p_request->capabilities_id, p_request->capabilities_summary);
	p_request->done.post();
}

AIEditorContextSnapshotResult AIV1EditorContextSnapshotService::_dispatch_to_main_thread(MainThreadRequest &r_request) {
	return _dispatch_main_thread_request<AIEditorContextSnapshotResult>(r_request, this, &AIV1EditorContextSnapshotService::_execute_request, request_mutex, "Failed to schedule editor context collection on the main thread.");
}

AIEditorContextSnapshotResult AIV1EditorContextSnapshotService::_collect_main_thread(const String &p_capabilities_id, const String &p_capabilities_summary) const {
	AIEditorContextSnapshotResult result;
	Dictionary context_metadata;
	const String capabilities_id = p_capabilities_id.is_empty() ? String("agent_v1") : p_capabilities_id;
	const String capabilities_summary = p_capabilities_summary.is_empty() ? String("agent_v1: use registered tool schemas and permission policy for this request.") : p_capabilities_summary;

	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	String resource_path;
	String application_name;
	if (project_settings) {
		resource_path = project_settings->get_resource_path();
		application_name = GLOBAL_GET("application/config/name");
	}

	const Size2i project_viewport_size(
			int(GLOBAL_GET("display/window/size/viewport_width")),
			int(GLOBAL_GET("display/window/size/viewport_height")));
	const Size2i window_override_size(
			int(GLOBAL_GET("display/window/size/window_width_override")),
			int(GLOBAL_GET("display/window/size/window_height_override")));

	context_metadata["resource_path"] = resource_path;
	context_metadata["application_name"] = application_name;
	context_metadata["engine_name"] = String(VERSION_NAME);
	context_metadata["engine_version"] = String(VERSION_FULL_CONFIG);
	context_metadata["capabilities"] = capabilities_id;
	context_metadata["capabilities_summary"] = capabilities_summary;

	Dictionary display;
	display["project_viewport_size"] = _make_size_dict(project_viewport_size);
	display["project_viewport_setting"] = "display/window/size/viewport_width,height";
	display["window_override_size"] = _make_size_dict(window_override_size);
	display["window_override_enabled"] = window_override_size.width > 0 || window_override_size.height > 0;

	EditorNode *editor = EditorNode::get_singleton();
	Node *scene_root = nullptr;
	if (editor) {
		EditorData &editor_data = EditorNode::get_editor_data();
		const int edited_scene_index = editor_data.get_edited_scene();
		if (edited_scene_index >= 0 && edited_scene_index < editor_data.get_edited_scene_count()) {
			scene_root = editor_data.get_edited_scene_root(edited_scene_index);
		}

		Control *base_control = editor->get_gui_base();
		if (base_control) {
			display["editor_base_control_size"] = _make_size_dict(base_control->get_size());
			Window *window = base_control->get_window();
			if (window) {
				display["editor_window_size"] = _make_size_dict(window->get_size());
			}
		}

		SubViewport *viewport_2d = editor->get_scene_root();
		if (viewport_2d) {
			display["editor_2d_viewport_size"] = _make_size_dict(viewport_2d->get_size());
			display["editor_2d_visible_rect"] = _make_rect_dict(viewport_2d->get_visible_rect());
		}
	}

	Dictionary scene;
	if (scene_root) {
		scene["has_current_scene"] = true;
		scene["root_name"] = scene_root->get_name();
		scene["root_type"] = scene_root->get_class();
		scene["root_kind"] = _scene_root_kind(scene_root);
		scene["scene_file_path"] = scene_root->get_scene_file_path();
	} else {
		scene["has_current_scene"] = false;
		scene["root_kind"] = "none";
	}

	Dictionary placement_guidance;
	placement_guidance["primary_screen_rect"] = _make_rect_dict(Rect2(Vector2(), project_viewport_size));
	placement_guidance["primary_canvas_size_source"] = "project_viewport_size";
	placement_guidance["control_origin"] = "top_left";
	placement_guidance["control_center_formula"] = "Vector2(project_viewport_size.width / 2, project_viewport_size.height / 2)";
	placement_guidance["node2d_units"] = "pixels in the active viewport; Camera2D can shift the visible world.";
	placement_guidance["note"] = "Use project_viewport_size as the runtime game/window coordinate frame for UI layout and screen-space 2D placement unless the task specifies a custom camera or container size.";

	context_metadata["display"] = display;
	context_metadata["current_scene"] = scene;
	context_metadata["placement_guidance"] = placement_guidance;

	String content;
	content += "Editor Context\n";
	content += "Engine: " + String(VERSION_NAME) + " " + String(VERSION_FULL_CONFIG) + "\n";
	content += "Project name: " + (application_name.is_empty() ? String("<unnamed>") : application_name) + "\n";
	content += "Project path: " + (resource_path.is_empty() ? String("<unknown>") : resource_path) + "\n";
	content += "Agent capabilities: " + capabilities_summary + "\n";
	content += "Scene/window sizing:\n";
	content += "- Project viewport size: " + _format_size(project_viewport_size) + " from display/window/size/viewport_width,height. Use this as the runtime game/window coordinate frame for Control UI and screen-space 2D placement.\n";
	if (window_override_size.width > 0 || window_override_size.height > 0) {
		content += "- Window override size: " + _format_size(window_override_size) + " from display/window/size/window_width_override,height_override.\n";
	} else {
		content += "- Window override size: disabled.\n";
	}
	if (display.has("editor_2d_viewport_size")) {
		Dictionary viewport_size = display["editor_2d_viewport_size"];
		content += vformat("- Current editor 2D viewport: %s x %s px. This is the editor panel's visible viewport, not necessarily the runtime window size.\n", String::num_real(double(viewport_size["width"])), String::num_real(double(viewport_size["height"])));
	}
	if (display.has("editor_base_control_size")) {
		Dictionary base_size = display["editor_base_control_size"];
		content += vformat("- Editor base control size: %s x %s px.\n", String::num_real(double(base_size["width"])), String::num_real(double(base_size["height"])));
	}
	if (scene_root) {
		content += vformat("Current scene root: `%s` (%s), kind=%s, path=%s\n", scene_root->get_name(), scene_root->get_class(), String(scene["root_kind"]), String(scene["scene_file_path"]).is_empty() ? String("<unsaved>") : String(scene["scene_file_path"]));
	} else {
		content += "Current scene root: <none>\n";
	}
	content += "Placement guidance: Control nodes use a top-left origin and parent/container rects; the project viewport center is approximately (width/2, height/2). Node2D positions are pixel coordinates in the active viewport unless a Camera2D changes the visible world.\n";

	result.success = true;
	result.content = content.strip_edges();
	result.metadata = context_metadata;
	return result;
}

AIEditorContextSnapshotResult AIV1EditorContextSnapshotService::collect(const String &p_capabilities_id, const String &p_capabilities_summary) {
	MainThreadRequest request;
	request.capabilities_id = p_capabilities_id;
	request.capabilities_summary = p_capabilities_summary;
	return _dispatch_to_main_thread(request);
}
