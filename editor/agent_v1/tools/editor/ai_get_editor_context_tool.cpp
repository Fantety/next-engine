/**************************************************************************/
/*  ai_get_editor_context_tool.cpp                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "ai_get_editor_context_tool.h"

#include "core/variant/variant.h"
#include "editor/agent_v1/tools/ai_editor_tools_v1.h"
#include "editor/agent_v1/tools/editor/ai_editor_context_snapshot.h"
#include "editor/next_file_logger.h"

namespace {

String _capabilities_id_for_agent(const String &p_agent_id) {
	const String agent_id = p_agent_id.strip_edges();
	if (agent_id.is_empty()) {
		return "agent_v1";
	}
	return "agent_v1-" + agent_id;
}

String _capabilities_summary_for_agent(const String &p_agent_id, bool p_review_changes) {
	const String agent_id = p_agent_id.strip_edges();
	String summary = agent_id.is_empty() ? String("agent_v1") : String("agent_v1 agent `") + agent_id + "`";
	summary += ": use registered tool schemas and permission policy for this request.";
	if (p_review_changes) {
		summary += " Mutating editor/project operations are recorded for user review when supported.";
	}
	return summary;
}

} // namespace

String AIV1GetEditorContextTool::get_name() const {
	return "editor.get_context";
}

String AIV1GetEditorContextTool::get_description() const {
	return "Returns safe, read-only metadata about the current Godot editor project, scene root, and available viewport/window sizes.";
}

Dictionary AIV1GetEditorContextTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	schema["properties"] = properties;
	return schema;
}

AIV1EditorToolResult AIV1GetEditorContextTool::execute_tool(const Dictionary &p_arguments) {
	(void)p_arguments;
	NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:editor.get_context] Start.");
	AIV1EditorToolResult result;

	Ref<AIV1EditorContextSnapshotService> snapshot_service;
	snapshot_service.instantiate();
	String capabilities_id = "agent_v1";
	String capabilities_summary = _capabilities_summary_for_agent(String(), false);
	Ref<AIV1ToolExecutionState> execution_context = AIV1ToolExecutionState::get_current();
	if (execution_context.is_valid()) {
		capabilities_id = _capabilities_id_for_agent(execution_context->get_agent_id());
		capabilities_summary = _capabilities_summary_for_agent(execution_context->get_agent_id(), execution_context->should_review_changes());
	}
	AIEditorContextSnapshotResult snapshot = snapshot_service->collect(capabilities_id, capabilities_summary);
	if (!snapshot.success) {
		result.error = snapshot.error.is_empty() ? String("Failed to collect editor context.") : snapshot.error;
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:editor.get_context] Failed: %s", result.error));
		return result;
	}

	result.content = snapshot.content;
	result.metadata = snapshot.metadata;
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:editor.get_context] Completed. project=%s path=%s", String(result.metadata.get("application_name", "")), String(result.metadata.get("resource_path", ""))));
	return result;
}
