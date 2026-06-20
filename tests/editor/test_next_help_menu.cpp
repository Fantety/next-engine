/**************************************************************************/
/*  test_next_help_menu.cpp                                               */
/**************************************************************************/

#include "tests/test_macros.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"

TEST_FORCE_LINK(test_next_help_menu);

namespace TestNextHelpMenu {

static String find_repo_file(const String &p_relative_path) {
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	REQUIRE(dir.is_valid());

	String current_dir = dir->get_current_dir();
	for (int i = 0; i < 8; i++) {
		const String candidate = current_dir.path_join(p_relative_path);
		if (FileAccess::exists(candidate)) {
			return candidate;
		}

		const String parent = current_dir.get_base_dir();
		if (parent == current_dir) {
			break;
		}
		current_dir = parent;
	}

	REQUIRE_MESSAGE(false, vformat("Could not locate repository file: %s", p_relative_path));
	return String();
}

static String read_repo_file(const String &p_relative_path) {
	const String path = find_repo_file(p_relative_path);
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ, &err);
	REQUIRE_MESSAGE(file.is_valid(), vformat("Could not read repository file: %s", path));
	REQUIRE(err == OK);
	return file->get_as_text();
}

TEST_CASE("[Editor][HelpMenu] NEXT help menu does not expose upstream Godot support channels") {
	const String editor_node = read_repo_file("editor/editor_node.cpp");

	CHECK(editor_node.contains("ED_SHORTCUT_AND_COMMAND(\"editor/online_docs\", TTRC(\"Godot Documentation\"))"));
	CHECK(editor_node.contains("ED_SHORTCUT_AND_COMMAND(\"editor/about\", TTRC(\"About NEXT Engine...\"))"));
	CHECK(editor_node.contains("get_editor_theme_native_menu_icon(SNAME(\"NextIcon\")"));

	CHECK_FALSE(editor_node.contains("ED_SHORTCUT_AND_COMMAND(\"editor/forum\""));
	CHECK_FALSE(editor_node.contains("ED_SHORTCUT_AND_COMMAND(\"editor/community\""));
	CHECK_FALSE(editor_node.contains("ED_SHORTCUT_AND_COMMAND(\"editor/support_development\""));
	CHECK_FALSE(editor_node.contains("ED_SHORTCUT_AND_COMMAND(\"editor/report_a_bug\""));
	CHECK_FALSE(editor_node.contains("ED_SHORTCUT_AND_COMMAND(\"editor/suggest_a_feature\""));
	CHECK_FALSE(editor_node.contains("ED_SHORTCUT_AND_COMMAND(\"editor/send_docs_feedback\""));

	CHECK_FALSE(editor_node.contains("https://forum.godotengine.org/"));
	CHECK_FALSE(editor_node.contains("https://godotengine.org/community"));
	CHECK_FALSE(editor_node.contains("https://fund.godotengine.org/"));
	CHECK_FALSE(editor_node.contains("https://github.com/godotengine/godot/issues"));
	CHECK_FALSE(editor_node.contains("https://github.com/godotengine/godot-proposals"));
	CHECK_FALSE(editor_node.contains("https://github.com/godotengine/godot-docs/issues"));
}

TEST_CASE("[Editor][HelpMenu] About dialog keeps NEXT branding and Godot MIT attribution") {
	const String editor_about = read_repo_file("editor/gui/editor_about.cpp");

	CHECK(editor_about.contains("set_title(TTRC(\"About NEXT Engine\"))"));
	CHECK(editor_about.contains("NEXT Engine contributors"));
	CHECK(editor_about.contains("Godot Engine contributors"));
	CHECK(editor_about.contains("Godot-derived editor"));
	CHECK(editor_about.contains("MIT license"));
}

} // namespace TestNextHelpMenu
