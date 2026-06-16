/**************************************************************************/
/*  ai_delete_file_tool.h                                                 */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"

class AIV1ProjectDeleteFileTool : public AIV1EditorTool {
	GDCLASS(AIV1ProjectDeleteFileTool, AIV1EditorTool);

	static bool _is_scene_file(const String &p_path);
	static String _normalize_resource_path(const String &p_path);
	static bool _dependency_matches_path(const String &p_dependency, const String &p_target_path);
	static void _collect_scene_files(const String &p_dir_path, Vector<String> &r_scene_paths);
	static bool _find_open_file(const String &p_path, String &r_open_path);
	static void _find_scene_references(const String &p_path, Vector<String> &r_referencing_scenes);
	static Array _vector_to_array(const Vector<String> &p_values);
	static String _format_path_list(const Vector<String> &p_paths, int p_max_count = 8);

public:
	String get_name() const override;
	String get_description() const override;
	Dictionary get_parameters_schema() const override;
	AIV1EditorToolResult execute_tool(const Dictionary &p_arguments) override;
};
