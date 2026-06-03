/**************************************************************************/
/*  test_ai_next_project_memory.cpp                                       */
/**************************************************************************/

#include "tests/test_macros.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "editor/ai_component/next/ai_next_project_memory_store.h"
#include "tests/test_utils.h"

TEST_FORCE_LINK(test_ai_next_project_memory);

namespace TestAINextProjectMemory {

static void cleanup_memory_file(const Ref<AINextProjectMemoryStore> &p_store) {
	const String memory_path = p_store->get_memory_path_for_test();
	if (FileAccess::exists(memory_path)) {
		DirAccess::remove_absolute(memory_path);
	}
}

TEST_CASE("[Editor][AI][NEXT] project memory is persisted per project scope") {
	const String first_scope = "test_next_memory_first_scope";
	const String second_scope = "test_next_memory_second_scope";

	Ref<AINextProjectMemoryStore> first_store;
	first_store.instantiate();
	first_store->set_base_dir_for_test(TestUtils::get_temp_path(first_scope));
	cleanup_memory_file(first_store);

	AINextProjectMemory memory;
	memory.language = "Chinese";
	memory.renderer = "Forward Plus";
	memory.architecture_notes.push_back("Use res://scripts for gameplay scripts.");
	memory.coding_conventions.push_back("Prefer typed GDScript exports for tunable values.");
	memory.scene_conventions.push_back("Player scenes attach movement scripts at the root node.");
	memory.user_preferences.push_back("Keep generated summaries concise.");

	CHECK(first_store->save_memory(memory) == OK);

	AINextProjectMemory loaded;
	CHECK(first_store->load_memory(loaded));
	CHECK(loaded.language == "Chinese");
	CHECK(loaded.renderer == "Forward Plus");
	CHECK(loaded.architecture_notes.size() == 1);
	CHECK(loaded.architecture_notes[0] == "Use res://scripts for gameplay scripts.");
	CHECK(loaded.user_preferences.size() == 1);
	CHECK(loaded.user_preferences[0] == "Keep generated summaries concise.");

	Ref<AINextProjectMemoryStore> second_store;
	second_store.instantiate();
	second_store->set_base_dir_for_test(TestUtils::get_temp_path(second_scope));
	cleanup_memory_file(second_store);

	AINextProjectMemory missing;
	CHECK_FALSE(second_store->load_memory(missing));

	cleanup_memory_file(first_store);
	cleanup_memory_file(second_store);
}

TEST_CASE("[Editor][AI][NEXT] project memory load replaces previous values") {
	AINextProjectMemory memory;
	memory.language = "Chinese";
	memory.renderer = "Forward Plus";
	memory.architecture_notes.push_back("Old architecture note.");
	memory.coding_conventions.push_back("Old coding convention.");
	memory.scene_conventions.push_back("Old scene convention.");
	memory.user_preferences.push_back("Old user preference.");

	Dictionary replacement;
	replacement["language"] = "English";
	memory.load_from_dict(replacement);

	CHECK(memory.language == "English");
	CHECK(memory.renderer.is_empty());
	CHECK(memory.architecture_notes.is_empty());
	CHECK(memory.coding_conventions.is_empty());
	CHECK(memory.scene_conventions.is_empty());
	CHECK(memory.user_preferences.is_empty());
}

} // namespace TestAINextProjectMemory
