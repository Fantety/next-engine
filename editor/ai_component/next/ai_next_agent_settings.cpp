/**************************************************************************/
/*  ai_next_agent_settings.cpp                                            */
/**************************************************************************/

#include "ai_next_agent_settings.h"

#include "editor/ai_component/next/ai_next_agent_registry.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/settings/editor_settings.h"

String AINextAgentSettings::_get_agent_model_path(const String &p_agent_id) {
	return "ai_agent/next/agents/" + p_agent_id + "/model_profile_id";
}

bool AINextAgentSettings::_is_valid_agent_id(const String &p_agent_id) {
	return AINextAgentRegistry::is_valid_agent_id(p_agent_id);
}

String AINextAgentSettings::_get_first_enabled_model_profile_id() {
	Vector<AIModelDescriptor> models = AIModelSettings::get_enabled_models();
	if (models.is_empty()) {
		return String();
	}
	return models[0].id;
}

Vector<String> AINextAgentSettings::get_agent_ids() {
	return AINextAgentRegistry::get_agent_ids();
}

String AINextAgentSettings::get_agent_display_name(const String &p_agent_id) {
	return AINextAgentRegistry::get_display_name(p_agent_id);
}

String AINextAgentSettings::get_model_profile_id(const String &p_agent_id) {
	if (!_is_valid_agent_id(p_agent_id)) {
		return String();
	}

	EditorSettings *settings = EditorSettings::get_singleton();
	if (!settings) {
		return String();
	}

	const String path = _get_agent_model_path(p_agent_id);
	if (!settings->has_setting(path)) {
		return String();
	}
	return String(settings->get(path));
}

String AINextAgentSettings::get_effective_model_profile_id(const String &p_agent_id) {
	const String configured_profile_id = get_model_profile_id(p_agent_id);
	if (!configured_profile_id.is_empty()) {
		AIModelDescriptor model = AIModelSettings::get_model(configured_profile_id);
		if (!model.id.is_empty() && model.enabled) {
			return configured_profile_id;
		}
	}
	return _get_first_enabled_model_profile_id();
}

bool AINextAgentSettings::set_model_profile_id(const String &p_agent_id, const String &p_model_profile_id) {
	if (!_is_valid_agent_id(p_agent_id)) {
		return false;
	}
	if (!p_model_profile_id.is_empty()) {
		AIModelDescriptor model = AIModelSettings::get_model(p_model_profile_id);
		if (model.id.is_empty() || !model.enabled) {
			return false;
		}
	}

	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL_V(settings, false);
	settings->set(_get_agent_model_path(p_agent_id), p_model_profile_id);
	return true;
}

Dictionary AINextAgentSettings::get_agent_model_storage_for_test() {
	Dictionary storage;
	Vector<String> agent_ids = get_agent_ids();
	for (int i = 0; i < agent_ids.size(); i++) {
		storage[agent_ids[i]] = get_model_profile_id(agent_ids[i]);
	}
	return storage;
}

void AINextAgentSettings::set_agent_model_storage_for_test(const Dictionary &p_storage) {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);

	Vector<String> agent_ids = get_agent_ids();
	for (int i = 0; i < agent_ids.size(); i++) {
		const String agent_id = agent_ids[i];
		settings->set(_get_agent_model_path(agent_id), String(p_storage.get(agent_id, String())));
	}
}

void AINextAgentSettings::clear_agent_models_for_test() {
	Vector<String> agent_ids = get_agent_ids();
	for (int i = 0; i < agent_ids.size(); i++) {
		(void)set_model_profile_id(agent_ids[i], String());
	}
}
