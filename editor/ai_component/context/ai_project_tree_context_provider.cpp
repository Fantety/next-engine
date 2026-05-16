/**************************************************************************/
/*  ai_project_tree_context_provider.cpp                                  */
/**************************************************************************/

#include "ai_project_tree_context_provider.h"

#include "core/object/class_db.h"
#include "core/string/string_builder.h"

#include "editor/ai_component/context/ai_context_document.h"

void AIProjectTreeContextProvider::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_limits", "max_depth", "max_entries", "max_chars"), &AIProjectTreeContextProvider::set_limits);
}

void AIProjectTreeContextProvider::set_limits(int p_max_depth, int p_max_entries, int p_max_chars) {
	max_depth = MAX(0, p_max_depth);
	max_entries = MAX(1, p_max_entries);
	max_chars = MAX(1000, p_max_chars);
}

Array AIProjectTreeContextProvider::collect_context() {
	truncated = false;
	entry_count = 0;

	StringBuilder builder;
	builder.append("Project tree under res://\n");
	_append_tree(builder, "res://", 0);

	String content = builder.as_string();
	if (content.length() > max_chars) {
		content = content.substr(0, max_chars);
		truncated = true;
	}
	if (truncated) {
		content += "\n[truncated]";
	}

	AIContextDocument doc;
	doc.title = "Project Tree";
	doc.source = "res://";
	doc.content = content;
	doc.truncated = truncated;

	Array result;
	result.push_back(doc.to_dict());
	return result;
}

void AIProjectTreeContextProvider::_append_tree(StringBuilder &r_builder, const String &p_path, int p_depth) {
	if (p_depth > max_depth || entry_count >= max_entries || r_builder.as_string().length() >= max_chars) {
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
		if (entry != "." && entry != "..") {
			bool hidden = entry.begins_with(".");
			if (include_hidden || !hidden) {
				bool is_dir = dir->current_is_dir();
				for (int i = 0; i < p_depth; i++) {
					r_builder.append("  ");
				}
				r_builder.append(is_dir ? "[D] " : "[F] ");
				r_builder.append(entry);
				r_builder.append("\n");
				entry_count++;

				if (entry_count >= max_entries || r_builder.as_string().length() >= max_chars) {
					truncated = true;
					break;
				}

				if (is_dir) {
					_append_tree(r_builder, p_path.path_join(entry), p_depth + 1);
				}
			}
		}
		entry = dir->get_next();
	}
	dir->list_dir_end();
}
