/**************************************************************************/
/*  ai_list_project_tool.cpp                                              */
/**************************************************************************/

#include "ai_list_project_tool.h"

#include "core/io/dir_access.h"
#include "core/variant/variant.h"

#include "editor/ai_component/tools/project/ai_project_tool_utils.h"

String AIListProjectTool::get_name() const {
	return "project.list_tree";
}

String AIListProjectTool::get_description() const {
	return "Lists files and directories under res:// with depth and entry limits.";
}

Dictionary AIListProjectTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary path_property;
	path_property["type"] = "string";
	path_property["description"] = "Project path under res:// to list.";
	properties["path"] = path_property;

	Dictionary depth_property;
	depth_property["type"] = "integer";
	depth_property["description"] = "Maximum directory depth.";
	properties["max_depth"] = depth_property;

	Dictionary entries_property;
	entries_property["type"] = "integer";
	entries_property["description"] = "Maximum number of entries to return.";
	properties["max_entries"] = entries_property;

	schema["properties"] = properties;
	return schema;
}

AIToolResult AIListProjectTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	String path = AIProjectToolUtils::get_path_argument(p_arguments);
	if (!AIProjectToolUtils::is_allowed_path(path)) {
		result.error = "Only res:// project paths without traversal are allowed.";
		return result;
	}

	max_depth = AIProjectToolUtils::get_int_argument(p_arguments, "max_depth", 4, 0, 8);
	max_entries = AIProjectToolUtils::get_int_argument(p_arguments, "max_entries", 400, 1, 2000);
	max_chars = AIProjectToolUtils::get_int_argument(p_arguments, "max_chars", 16000, 1000, 64000);
	entry_count = 0;
	truncated = false;

	String output = "Project tree under " + path + "\n";
	_append_tree(output, path, 0);
	if (truncated) {
		output += "[truncated]\n";
	}

	result.content = output;
	result.truncated = truncated;
	result.metadata["path"] = path;
	result.metadata["entry_count"] = entry_count;
	return result;
}

void AIListProjectTool::_append_tree(String &r_output, const String &p_path, int p_depth) {
	if (p_depth > max_depth || entry_count >= max_entries || r_output.length() >= max_chars) {
		truncated = true;
		return;
	}

	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	String entry = dir->get_next();
	while (!entry.is_empty()) {
		if (entry != "." && entry != ".." && !entry.begins_with(".")) {
			bool is_dir = dir->current_is_dir();
			for (int i = 0; i < p_depth; i++) {
				r_output += "  ";
			}
			r_output += is_dir ? "[D] " : "[F] ";
			r_output += entry + "\n";
			entry_count++;

			if (entry_count >= max_entries || r_output.length() >= max_chars) {
				truncated = true;
				break;
			}

			if (is_dir) {
				_append_tree(r_output, p_path.path_join(entry), p_depth + 1);
			}
		}
		entry = dir->get_next();
	}
	dir->list_dir_end();
}
