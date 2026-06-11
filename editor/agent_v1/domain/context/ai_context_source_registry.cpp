/**************************************************************************/
/*  ai_context_source_registry.cpp                                        */
/**************************************************************************/

#include "ai_context_source_registry.h"

#include "core/object/class_db.h"
#include "core/os/time.h"
#include "core/variant/variant.h"

static String _ai_context_source_hash(const String &p_domain, const String &p_text, const Dictionary &p_metadata, bool p_required, bool p_available) {
	String signature = p_domain + "\n" + p_text + "\n" + Variant(p_metadata).stringify();
	signature += p_required ? "\nrequired" : "\noptional";
	signature += p_available ? "\navailable" : "\nunavailable";
	return signature.md5_text();
}

void AIContextSourceRegistry::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_config_service", "config_service"), &AIContextSourceRegistry::set_config_service);
	ClassDB::bind_method(D_METHOD("get_config_service"), &AIContextSourceRegistry::get_config_service);
	ClassDB::bind_method(D_METHOD("add_source", "source"), &AIContextSourceRegistry::add_source);
	ClassDB::bind_method(D_METHOD("add_session_source", "session_id", "source"), &AIContextSourceRegistry::add_session_source);
	ClassDB::bind_method(D_METHOD("clear_sources"), &AIContextSourceRegistry::clear_sources);
	ClassDB::bind_method(D_METHOD("set_blocked", "blocked", "reason"), &AIContextSourceRegistry::set_blocked, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("load", "agent_id", "location", "provider", "model"), &AIContextSourceRegistry::load);
}

AIContextSourceRegistry::AIContextSourceRegistry() {
	config_service.instantiate();
}

AISystemContextSource AIContextSourceRegistry::_make_source(const String &p_domain, const String &p_text, bool p_required, int p_priority, const Dictionary &p_metadata, bool p_available) {
	AISystemContextSource source;
	source.domain = p_domain.strip_edges();
	source.text = p_text;
	source.required = p_required;
	source.available = p_available;
	source.priority = p_priority;
	source.metadata = p_metadata.duplicate(true);
	source.content_hash = _ai_context_source_hash(source.domain, source.text, source.metadata, source.required, source.available);
	return source;
}

Dictionary AIContextSourceRegistry::_dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return Dictionary(p_value).duplicate(true);
	}
	return Dictionary();
}

Array AIContextSourceRegistry::_array_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::ARRAY) {
		return Array(p_value).duplicate(true);
	}
	return Array();
}

void AIContextSourceRegistry::_append_text_sources(Vector<AISystemContextSource> &r_sources, const String &p_domain_prefix, const Variant &p_value, bool p_required, int p_priority, const Dictionary &p_metadata) {
	if (p_value.get_type() == Variant::STRING) {
		const String text = String(p_value).strip_edges();
		if (!text.is_empty()) {
			r_sources.push_back(_make_source(p_domain_prefix, text, p_required, p_priority, p_metadata));
		}
		return;
	}

	const Array values = _array_from_variant(p_value);
	for (int i = 0; i < values.size(); i++) {
		const Variant value = values[i];
		if (value.get_type() == Variant::STRING) {
			const String text = String(value).strip_edges();
			if (!text.is_empty()) {
				r_sources.push_back(_make_source(p_domain_prefix + "/" + itos(i), text, p_required, p_priority + i, p_metadata));
			}
		} else if (value.get_type() == Variant::DICTIONARY) {
			const Dictionary dict = value;
			const String text = String(dict.get("text", dict.get("content", dict.get("guidance", String())))).strip_edges();
			if (!text.is_empty() && bool(dict.get("enabled", true))) {
				Dictionary source_metadata = p_metadata.duplicate(true);
				source_metadata["source"] = dict.duplicate(true);
				r_sources.push_back(_make_source(p_domain_prefix + "/" + itos(i), text, p_required, p_priority + i, source_metadata));
			}
		}
	}
}

String AIContextSourceRegistry::_host_date() {
	if (!Time::get_singleton()) {
		return String();
	}
	const Dictionary date = Time::get_singleton()->get_date_dict_from_system(false);
	const int year = int(date.get("year", 0));
	const int month = int(date.get("month", 0));
	const int day = int(date.get("day", 0));
	if (year <= 0 || month <= 0 || day <= 0) {
		return String();
	}
	return vformat("%04d-%02d-%02d", year, month, day);
}

void AIContextSourceRegistry::set_config_service(const Ref<AIConfigService> &p_config_service) {
	config_service = p_config_service;
}

Ref<AIConfigService> AIContextSourceRegistry::get_config_service() const {
	return config_service;
}

void AIContextSourceRegistry::add_source(const Dictionary &p_source) {
	add_source_struct(AISystemContextSource::from_dictionary(p_source));
}

void AIContextSourceRegistry::add_source_struct(const AISystemContextSource &p_source) {
	MutexLock lock(mutex);
	manual_sources.push_back(p_source);
}

void AIContextSourceRegistry::add_session_source(const String &p_session_id, const Dictionary &p_source) {
	add_session_source_struct(p_session_id, AISystemContextSource::from_dictionary(p_source));
}

void AIContextSourceRegistry::add_session_source_struct(const String &p_session_id, const AISystemContextSource &p_source) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		add_source_struct(p_source);
		return;
	}

	MutexLock lock(mutex);
	manual_sources_by_session[session_id].push_back(p_source);
}

void AIContextSourceRegistry::clear_session_sources_with_domain_prefix_struct(const String &p_session_id, const String &p_domain_prefix) {
	const String session_id = p_session_id.strip_edges();
	const String domain_prefix = p_domain_prefix.strip_edges();
	if (session_id.is_empty() || domain_prefix.is_empty()) {
		return;
	}

	MutexLock lock(mutex);
	HashMap<String, Vector<AISystemContextSource>>::Iterator session_sources = manual_sources_by_session.find(session_id);
	if (!session_sources) {
		return;
	}

	Vector<AISystemContextSource> retained_sources;
	for (int i = 0; i < session_sources->value.size(); i++) {
		const String domain = session_sources->value[i].domain;
		if (domain == domain_prefix || domain.begins_with(domain_prefix + ":") || domain.begins_with(domain_prefix + "/")) {
			continue;
		}
		retained_sources.push_back(session_sources->value[i]);
	}

	if (retained_sources.is_empty()) {
		manual_sources_by_session.erase(session_id);
	} else {
		session_sources->value = retained_sources;
	}
}

void AIContextSourceRegistry::clear_sources() {
	MutexLock lock(mutex);
	manual_sources.clear();
	manual_sources_by_session.clear();
}

void AIContextSourceRegistry::set_blocked(bool p_blocked, const String &p_reason) {
	MutexLock lock(mutex);
	blocked = p_blocked;
	blocked_reason = p_reason;
}

bool AIContextSourceRegistry::load_struct(const String &p_agent_id, const AILocationRef &p_location, const String &p_provider, const String &p_model, AISystemContext &r_context, AIError &r_error) const {
	return load_session_struct(String(), p_agent_id, p_location, p_provider, p_model, r_context, r_error);
}

bool AIContextSourceRegistry::load_session_struct(const String &p_session_id, const String &p_agent_id, const AILocationRef &p_location, const String &p_provider, const String &p_model, AISystemContext &r_context, AIError &r_error) const {
	Vector<AISystemContextSource> sources;
	{
		MutexLock lock(mutex);
		if (blocked) {
			AISystemContextSource source = _make_source("system.blocked", String(), true, -1000, Dictionary(), false);
			source.metadata["reason"] = blocked_reason;
			sources.push_back(source);
			r_context = AISystemContext::combine(sources);
			r_context.blocked_reason = blocked_reason.strip_edges().is_empty() ? r_context.blocked_reason : blocked_reason;
			r_error = AIError::none();
			return true;
		}
		for (int i = 0; i < manual_sources.size(); i++) {
			sources.push_back(manual_sources[i]);
		}
		const String session_id = p_session_id.strip_edges();
		if (!session_id.is_empty()) {
			HashMap<String, Vector<AISystemContextSource>>::ConstIterator session_sources = manual_sources_by_session.find(session_id);
			if (session_sources) {
				for (int i = 0; i < session_sources->value.size(); i++) {
					sources.push_back(session_sources->value[i]);
				}
			}
		}
	}

	const String agent_id = p_agent_id.strip_edges().is_empty() ? String("main") : p_agent_id.strip_edges();
	Dictionary environment_metadata;
	environment_metadata["agent"] = agent_id;
	environment_metadata["directory"] = p_location.directory;
	environment_metadata["workspace_id"] = p_location.workspace_id;
	environment_metadata["provider"] = p_provider;
	environment_metadata["model"] = p_model;

	const String date = _host_date();
	String environment_text = "You are running inside NextEngine editor agent infrastructure.";
	if (!p_location.directory.strip_edges().is_empty()) {
		environment_text += "\nCurrent workspace directory: " + p_location.directory.strip_edges();
	}
	if (!date.is_empty()) {
		environment_text += "\nHost-local date: " + date + ".";
	}
	sources.push_back(_make_source("environment", environment_text, true, -100, environment_metadata));

	Dictionary model_metadata;
	model_metadata["provider"] = p_provider;
	model_metadata["model"] = p_model;
	sources.push_back(_make_source("model.selection", String(), true, -90, model_metadata));

	if (config_service.is_null()) {
		sources.push_back(_make_source("config", String(), true, -80, Dictionary(), false));
		r_context = AISystemContext::combine(sources);
		r_error = AIError::none();
		return true;
	}

	const Dictionary config = config_service->get_config();
	if (!bool(config.get("success", true))) {
		Dictionary source_metadata;
		source_metadata["error"] = config.get("error", Dictionary());
		sources.push_back(_make_source("config", String(), true, -80, source_metadata, false));
		r_context = AISystemContext::combine(sources);
		r_error = AIError::none();
		return true;
	}

	const Array system_prompt = config_service->get_system_prompt(agent_id);
	Dictionary agent_metadata;
	agent_metadata["agent"] = agent_id;
	_append_text_sources(sources, "agent.system/" + agent_id, system_prompt, true, 0, agent_metadata);

	const Dictionary provider_config = config_service->get_provider_config(p_provider);
	Dictionary provider_metadata;
	provider_metadata["provider"] = p_provider;
	provider_metadata["model"] = p_model;
	_append_text_sources(sources, "provider.system/" + p_provider, provider_config.get("system", provider_config.get("instructions", Variant())), false, 100, provider_metadata);

	const Dictionary skills = _dictionary_from_variant(config.get("skills", Dictionary()));
	Dictionary skill_metadata;
	skill_metadata["agent"] = agent_id;
	_append_text_sources(sources, "skills.guidance/" + agent_id, skills.get("guidance", Variant()), false, 200, skill_metadata);

	r_context = AISystemContext::combine(sources);
	r_error = AIError::none();
	return true;
}

Dictionary AIContextSourceRegistry::load(const String &p_agent_id, const Dictionary &p_location, const String &p_provider, const String &p_model) const {
	AISystemContext context;
	AIError error;
	const bool ok = load_struct(p_agent_id, AILocationRef::from_dictionary(p_location), p_provider, p_model, context, error);
	Dictionary result;
	result["success"] = ok && !error.is_error();
	if (ok && !error.is_error()) {
		result["context"] = context.to_dictionary();
	} else {
		result["error"] = error.to_dictionary();
	}
	return result;
}
