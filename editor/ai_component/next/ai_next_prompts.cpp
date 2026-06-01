/**************************************************************************/
/*  ai_next_prompts.cpp                                                   */
/**************************************************************************/

#include "ai_next_prompts.h"

#include "editor/ai_component/prompts/ai_prompt_registry.h"

namespace AINextPrompts {

const char *get_planning_prompt() {
	return AIPrompts::get_planning_prompt();
}

const char *get_script_prompt() {
	return AIPrompts::get_script_prompt();
}

const char *get_scene_prompt() {
	return AIPrompts::get_scene_prompt();
}

const char *get_shader_prompt() {
	return AIPrompts::get_shader_prompt();
}

const char *get_review_prompt() {
	return AIPrompts::get_review_prompt();
}

} // namespace AINextPrompts
