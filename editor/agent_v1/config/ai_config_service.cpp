/**************************************************************************/
/*  ai_config_service.cpp                                                 */
/**************************************************************************/

#include "ai_config_service.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/variant/variant.h"

Dictionary AIConfigEntry::to_dictionary() const {
	Dictionary result;
	result["source"] = source;
	result["path"] = path;
	result["priority"] = priority;
	result["data"] = data;
	return result;
}

AIConfigEntry AIConfigEntry::from_dictionary(const Dictionary &p_dict) {
	AIConfigEntry result;
	result.source = p_dict.get("source", String());
	result.path = p_dict.get("path", String());
	result.priority = int(p_dict.get("priority", 0));
	if (p_dict.get("data", Variant()).get_type() == Variant::DICTIONARY) {
		result.data = Dictionary(p_dict["data"]).duplicate(true);
	}
	return result;
}

void AIConfigService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_global_config_path", "path"), &AIConfigService::set_global_config_path);
	ClassDB::bind_method(D_METHOD("get_global_config_path"), &AIConfigService::get_global_config_path);
	ClassDB::bind_method(D_METHOD("set_project_config_path", "path"), &AIConfigService::set_project_config_path);
	ClassDB::bind_method(D_METHOD("get_project_config_path"), &AIConfigService::get_project_config_path);
	ClassDB::bind_method(D_METHOD("set_opencode_config_path", "path"), &AIConfigService::set_opencode_config_path);
	ClassDB::bind_method(D_METHOD("get_opencode_config_path"), &AIConfigService::get_opencode_config_path);
	ClassDB::bind_method(D_METHOD("set_account_config_path", "path"), &AIConfigService::set_account_config_path);
	ClassDB::bind_method(D_METHOD("get_account_config_path"), &AIConfigService::get_account_config_path);
	ClassDB::bind_method(D_METHOD("set_managed_config_path", "path"), &AIConfigService::set_managed_config_path);
	ClassDB::bind_method(D_METHOD("get_managed_config_path"), &AIConfigService::get_managed_config_path);
	ClassDB::bind_method(D_METHOD("set_remote_config_path", "path"), &AIConfigService::set_remote_config_path);
	ClassDB::bind_method(D_METHOD("get_remote_config_path"), &AIConfigService::get_remote_config_path);
	ClassDB::bind_method(D_METHOD("get_config"), &AIConfigService::get_config);
	ClassDB::bind_method(D_METHOD("entries"), &AIConfigService::entries);
	ClassDB::bind_method(D_METHOD("patch_config", "patch", "scope"), &AIConfigService::patch_config, DEFVAL("project"));
	ClassDB::bind_method(D_METHOD("update", "patch"), &AIConfigService::update);
	ClassDB::bind_method(D_METHOD("update_global", "patch"), &AIConfigService::update_global);
	ClassDB::bind_method(D_METHOD("set_runtime_override", "override"), &AIConfigService::set_runtime_override);
	ClassDB::bind_method(D_METHOD("get_runtime_override"), &AIConfigService::get_runtime_override);
	ClassDB::bind_method(D_METHOD("invalidate", "reason"), &AIConfigService::invalidate, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_default_provider"), &AIConfigService::get_default_provider);
	ClassDB::bind_method(D_METHOD("get_default_model"), &AIConfigService::get_default_model);
	ClassDB::bind_method(D_METHOD("get_provider_config", "provider"), &AIConfigService::get_provider_config);
	ClassDB::bind_method(D_METHOD("get_system_prompt", "agent_id"), &AIConfigService::get_system_prompt, DEFVAL(String()));

	ADD_SIGNAL(MethodInfo("config_invalidated", PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("config_changed", PropertyInfo(Variant::DICTIONARY, "config")));
}

AIConfigService::AIConfigService() {
	global_config_path = "user://net.nextengine/config/settings.jsonc";
	project_config_path = "res://net.nextengine.v1.jsonc";
	opencode_config_path = "res://.opencode/config.jsonc";
	account_config_path = "user://net.nextengine/config/account.jsonc";
	managed_config_path = "user://net.nextengine/config/managed.jsonc";
}

Dictionary AIConfigService::_default_config() {
	Dictionary fake_provider;
	fake_provider["type"] = "fake";
	fake_provider["model"] = "fake-model";
	fake_provider["response_text"] = "Fake provider response.";

	Dictionary providers;
	providers["fake"] = fake_provider;

	Array system;
	system.push_back("You are NextEngine Agent.");

	Dictionary main_agent;
	main_agent["provider"] = "fake";
	main_agent["model"] = "fake-model";
	main_agent["system"] = system;

	Dictionary agents;
	agents["main"] = main_agent;

	Dictionary permissions;
	permissions["rules"] = Array();

	Dictionary mcp;
	mcp["servers"] = Dictionary();

	Dictionary skills;
	skills["sources"] = Array();

	Dictionary metadata;
	metadata["version"] = 1;

	Dictionary result;
	result["default_provider"] = "fake";
	result["default_model"] = "fake-model";
	result["providers"] = providers;
	result["agents"] = agents;
	result["permissions"] = permissions;
	result["mcp"] = mcp;
	result["skills"] = skills;
	result["metadata"] = metadata;
	return result;
}

Dictionary AIConfigService::_dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return Dictionary(p_value).duplicate(true);
	}
	return Dictionary();
}

Array AIConfigService::_array_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::ARRAY) {
		return Array(p_value).duplicate(true);
	}
	return Array();
}

Dictionary AIConfigService::_merge_dicts(const Dictionary &p_base, const Dictionary &p_patch) {
	Dictionary result = p_base.duplicate(true);
	for (const KeyValue<Variant, Variant> &kv : p_patch) {
		if (kv.value.get_type() == Variant::DICTIONARY && result.get(kv.key, Variant()).get_type() == Variant::DICTIONARY) {
			result[kv.key] = _merge_dicts(Dictionary(result[kv.key]), Dictionary(kv.value));
		 } else {
			result[kv.key] = kv.value;
		}
	}
	return result;
}

String AIConfigService::_strip_json_comments(const String &p_text) {
	String without_comments;
	bool in_string = false;
	bool escaped = false;
	for (int i = 0; i < p_text.length(); i++) {
		const char32_t ch = p_text[i];
		const char32_t next = i + 1 < p_text.length() ? p_text[i + 1] : 0;
		if (in_string) {
			without_comments += String::chr(ch);
			if (escaped) {
				escaped = false;
			} else if (ch == '\\') {
				escaped = true;
			} else if (ch == '"') {
				in_string = false;
			}
			continue;
		}

		if (ch == '"') {
			in_string = true;
			without_comments += String::chr(ch);
			continue;
		}
		if (ch == '/' && next == '/') {
			while (i < p_text.length() && p_text[i] != '\n') {
				i++;
			}
			without_comments += "\n";
			continue;
		}
		if (ch == '/' && next == '*') {
			i += 2;
			while (i + 1 < p_text.length() && !(p_text[i] == '*' && p_text[i + 1] == '/')) {
				if (p_text[i] == '\n') {
					without_comments += "\n";
				}
				i++;
			}
			i++;
			continue;
		}
		without_comments += String::chr(ch);
	}

	String result;
	in_string = false;
	escaped = false;
	for (int i = 0; i < without_comments.length(); i++) {
		const char32_t ch = without_comments[i];
		if (in_string) {
			result += String::chr(ch);
			if (escaped) {
				escaped = false;
			} else if (ch == '\\') {
				escaped = true;
			} else if (ch == '"') {
				in_string = false;
			}
			continue;
		}
		if (ch == '"') {
			in_string = true;
			result += String::chr(ch);
			continue;
		}
		if (ch == ',') {
			int lookahead = i + 1;
			while (lookahead < without_comments.length() && (without_comments[lookahead] == ' ' || without_comments[lookahead] == '\t' || without_comments[lookahead] == '\r' || without_comments[lookahead] == '\n')) {
				lookahead++;
			}
			if (lookahead < without_comments.length() && (without_comments[lookahead] == '}' || without_comments[lookahead] == ']')) {
				continue;
			}
		}
		result += String::chr(ch);
	}
	return result;
}

bool AIConfigService::_parse_json_text(const String &p_text, const String &p_label, Dictionary &r_data, AIError &r_error) {
	r_data = Dictionary();
	if (p_text.strip_edges().is_empty()) {
		return true;
	}

	Ref<JSON> json;
	json.instantiate();
	const Error parse_err = json->parse(_strip_json_comments(p_text));
	if (parse_err != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		Dictionary details;
		details["source"] = p_label;
		details["line"] = json->get_error_line();
		details["message"] = json->get_error_message();
		r_error = AIError::make(AI_ERROR_VALIDATION, "Failed to parse config JSON.", details);
		return false;
	}

	r_data = Dictionary(json->get_data()).duplicate(true);
	return true;
}

bool AIConfigService::_read_json_file(const String &p_path, Dictionary &r_data, AIError &r_error) {
	r_data = Dictionary();
	if (p_path.strip_edges().is_empty() || !FileAccess::exists(p_path)) {
		return true;
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to open config file: " + p_path);
		return false;
	}

	return _parse_json_text(file->get_as_text(), p_path, r_data, r_error);
}

bool AIConfigService::_write_json_file(const String &p_path, const Dictionary &p_data, AIError &r_error) {
	if (p_path.strip_edges().is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Config path is required.");
		return false;
	}

	const String base_dir = p_path.get_base_dir();
	if (!base_dir.is_empty()) {
		const Error dir_err = DirAccess::make_dir_recursive_absolute(base_dir);
		if (dir_err != OK) {
			r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to create config directory: " + base_dir);
			return false;
		}
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	if (file.is_null() || err != OK) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to write config file: " + p_path);
		return false;
	}
	file->store_string(JSON::stringify(p_data, "\t") + "\n");
	file->flush();
	return true;
}

Dictionary AIConfigService::_migrate_config(const Dictionary &p_config) {
	Dictionary result = p_config.duplicate(true);
	if (result.has("provider") && !result.has("default_provider")) {
		result["default_provider"] = result["provider"];
	}
	if (result.has("model") && !result.has("default_model")) {
		result["default_model"] = result["model"];
	}
	if (result.has("openai_api_key")) {
		Dictionary providers = _dictionary_from_variant(result.get("providers", Dictionary()));
		Dictionary openai = _dictionary_from_variant(providers.get("openai", Dictionary()));
		openai["type"] = "openai-compatible";
		openai["api_key"] = result["openai_api_key"];
		providers["openai"] = openai;
		result["providers"] = providers;
		result.erase("openai_api_key");
	}
	return result;
}

bool AIConfigService::_environment_config(Dictionary &r_data, AIError &r_error) {
	r_data = Dictionary();
	OS *os = OS::get_singleton();
	if (!os) {
		return true;
	}

	const String json_env = os->has_environment("NEXT_AGENT_CONFIG_JSON") ? os->get_environment("NEXT_AGENT_CONFIG_JSON") : os->get_environment("NEXT_AGENT_CONFIG");
	if (!json_env.strip_edges().is_empty()) {
		if (!_parse_json_text(json_env, "env:NEXT_AGENT_CONFIG_JSON", r_data, r_error)) {
			return false;
		}
		r_data = _migrate_config(r_data);
	}

	Dictionary env_patch;
	if (os->has_environment("NEXT_AGENT_PROVIDER")) {
		env_patch["default_provider"] = os->get_environment("NEXT_AGENT_PROVIDER");
	}
	if (os->has_environment("NEXT_AGENT_MODEL")) {
		env_patch["default_model"] = os->get_environment("NEXT_AGENT_MODEL");
	}

	const String openai_key = os->has_environment("NEXT_AGENT_OPENAI_API_KEY") ? os->get_environment("NEXT_AGENT_OPENAI_API_KEY") : os->get_environment("OPENAI_API_KEY");
	if (!openai_key.is_empty()) {
		Dictionary providers = _dictionary_from_variant(env_patch.get("providers", r_data.get("providers", Dictionary())));
		Dictionary openai = _dictionary_from_variant(providers.get("openai", Dictionary()));
		openai["type"] = "openai-compatible";
		openai["api_key"] = openai_key;
		providers["openai"] = openai;
		env_patch["providers"] = providers;
		if (!env_patch.has("default_provider") && !r_data.has("default_provider")) {
			env_patch["default_provider"] = "openai";
		}
	}

	if (!env_patch.is_empty()) {
		r_data = _merge_dicts(r_data, env_patch);
	}
	return true;
}

String AIConfigService::_resolve_variables_in_string(const String &p_value) {
	String result = p_value;
	int start = result.find("{");
	while (start >= 0) {
		const int end = result.find("}", start);
		if (end < 0) {
			break;
		}

		const String token = result.substr(start + 1, end - start - 1);
		String replacement;
		bool replaced = false;
		if (token.begins_with("env:")) {
			const String env_name = token.substr(4);
			replacement = OS::get_singleton() ? OS::get_singleton()->get_environment(env_name) : String();
			replaced = true;
		} else if (token.begins_with("file:")) {
			const String path = token.substr(5);
			if (FileAccess::exists(path)) {
				Error err = OK;
				Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ, &err);
				if (file.is_valid() && err == OK) {
					replacement = file->get_as_text().strip_edges();
					replaced = true;
				}
			}
		}

		if (replaced) {
			result = result.substr(0, start) + replacement + result.substr(end + 1);
			start = result.find("{", start + replacement.length());
		} else {
			start = result.find("{", end + 1);
		}
	}
	return result;
}

Variant AIConfigService::_resolve_variables(const Variant &p_value) {
	if (p_value.get_type() == Variant::STRING) {
		return _resolve_variables_in_string(p_value);
	}
	if (p_value.get_type() == Variant::DICTIONARY) {
		Dictionary source = p_value;
		Dictionary result;
		for (const KeyValue<Variant, Variant> &kv : source) {
			result[kv.key] = _resolve_variables(kv.value);
		}
		return result;
	}
	if (p_value.get_type() == Variant::ARRAY) {
		Array source = p_value;
		Array result;
		for (int i = 0; i < source.size(); i++) {
			result.push_back(_resolve_variables(source[i]));
		}
		return result;
	}
	return p_value;
}

void AIConfigService::_sort_entries_by_priority(Vector<AIConfigEntry> &r_entries) {
	for (int i = 0; i < r_entries.size(); i++) {
		for (int j = i + 1; j < r_entries.size(); j++) {
			if (r_entries[j].priority < r_entries[i].priority) {
				const AIConfigEntry tmp = r_entries[i];
				r_entries.write[i] = r_entries[j];
				r_entries.write[j] = tmp;
			}
		}
	}
}

void AIConfigService::_push_entry(Vector<AIConfigEntry> &r_entries, const String &p_source, const String &p_path, int p_priority, const Dictionary &p_data) {
	if (p_data.is_empty()) {
		return;
	}

	AIConfigEntry entry;
	entry.source = p_source;
	entry.path = p_path;
	entry.priority = p_priority;
	entry.data = _migrate_config(p_data);
	r_entries.push_back(entry);
}

Vector<AIConfigEntry> AIConfigService::_discover_entries_locked(AIError &r_error) const {
	Vector<AIConfigEntry> result;

	AIConfigEntry defaults;
	defaults.source = "default";
	defaults.priority = 0;
	defaults.data = _default_config();
	result.push_back(defaults);

	Dictionary global_data;
	if (!_read_json_file(global_config_path, global_data, r_error)) {
		return result;
	}
	_push_entry(result, "global", global_config_path, 100, global_data);

	Dictionary project_data;
	if (!_read_json_file(project_config_path, project_data, r_error)) {
		return result;
	}
	_push_entry(result, "project", project_config_path, 200, project_data);

	Dictionary opencode_data;
	if (!_read_json_file(opencode_config_path, opencode_data, r_error)) {
		return result;
	}
	_push_entry(result, "opencode", opencode_config_path, 250, opencode_data);

	Dictionary env_data;
	if (!_environment_config(env_data, r_error)) {
		return result;
	}
	_push_entry(result, "env", String(), 300, env_data);

	Dictionary account_data;
	if (!_read_json_file(account_config_path, account_data, r_error)) {
		return result;
	}
	_push_entry(result, "account", account_config_path, 400, account_data);

	Dictionary remote_data;
	if (!_read_json_file(remote_config_path, remote_data, r_error)) {
		return result;
	}
	_push_entry(result, "remote", remote_config_path, 500, remote_data);

	Dictionary managed_data;
	if (!_read_json_file(managed_config_path, managed_data, r_error)) {
		return result;
	}
	_push_entry(result, "managed", managed_config_path, 900, managed_data);

	if (!runtime_override.is_empty()) {
		AIConfigEntry runtime;
		runtime.source = "runtime";
		runtime.priority = 1000;
		runtime.data = runtime_override.duplicate(true);
		result.push_back(runtime);
	}

	_sort_entries_by_priority(result);
	return result;
}

bool AIConfigService::_ensure_loaded_locked(AIError &r_error) {
	if (cache_valid) {
		return true;
	}

	Vector<AIConfigEntry> discovered = _discover_entries_locked(r_error);
	if (r_error.is_error()) {
		return false;
	}

	Dictionary merged;
	for (int i = 0; i < discovered.size(); i++) {
		merged = _merge_dicts(merged, discovered[i].data);
	}
	merged = _dictionary_from_variant(_resolve_variables(merged));

	cached_entries = discovered;
	cached_config = merged;
	cache_valid = true;
	return true;
}

void AIConfigService::set_global_config_path(const String &p_path) {
	MutexLock lock(mutex);
	global_config_path = p_path.strip_edges();
	cache_valid = false;
}

String AIConfigService::get_global_config_path() const {
	MutexLock lock(mutex);
	return global_config_path;
}

void AIConfigService::set_project_config_path(const String &p_path) {
	MutexLock lock(mutex);
	project_config_path = p_path.strip_edges();
	cache_valid = false;
}

String AIConfigService::get_project_config_path() const {
	MutexLock lock(mutex);
	return project_config_path;
}

void AIConfigService::set_opencode_config_path(const String &p_path) {
	MutexLock lock(mutex);
	opencode_config_path = p_path.strip_edges();
	cache_valid = false;
}

String AIConfigService::get_opencode_config_path() const {
	MutexLock lock(mutex);
	return opencode_config_path;
}

void AIConfigService::set_account_config_path(const String &p_path) {
	MutexLock lock(mutex);
	account_config_path = p_path.strip_edges();
	cache_valid = false;
}

String AIConfigService::get_account_config_path() const {
	MutexLock lock(mutex);
	return account_config_path;
}

void AIConfigService::set_managed_config_path(const String &p_path) {
	MutexLock lock(mutex);
	managed_config_path = p_path.strip_edges();
	cache_valid = false;
}

String AIConfigService::get_managed_config_path() const {
	MutexLock lock(mutex);
	return managed_config_path;
}

void AIConfigService::set_remote_config_path(const String &p_path) {
	MutexLock lock(mutex);
	remote_config_path = p_path.strip_edges();
	cache_valid = false;
}

String AIConfigService::get_remote_config_path() const {
	MutexLock lock(mutex);
	return remote_config_path;
}

Dictionary AIConfigService::get_config() {
	MutexLock lock(mutex);
	AIError error;
	if (!_ensure_loaded_locked(error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	Dictionary result = cached_config.duplicate(true);
	result["success"] = true;
	return result;
}

Array AIConfigService::entries() {
	MutexLock lock(mutex);
	AIError error;
	if (!_ensure_loaded_locked(error)) {
		Array failed;
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		failed.push_back(result);
		return failed;
	}

	Array result;
	for (int i = 0; i < cached_entries.size(); i++) {
		result.push_back(cached_entries[i].to_dictionary());
	}
	return result;
}

Dictionary AIConfigService::patch_config(const Dictionary &p_patch, const String &p_scope) {
	const String scope = p_scope.strip_edges().to_lower();
	if (scope == "global") {
		return update_global(p_patch);
	}
	if (scope == "runtime") {
		set_runtime_override(_merge_dicts(get_runtime_override(), p_patch));
		invalidate("patch_runtime");
		Dictionary config = get_config();
		call_deferred("emit_signal", SNAME("config_changed"), config);
		return config;
	}
	if (scope == "project" || scope.is_empty()) {
		return update(p_patch);
	}

	Dictionary result;
	result["success"] = false;
	result["error"] = AIError::make(AI_ERROR_VALIDATION, "Unsupported config patch scope: " + p_scope).to_dictionary();
	return result;
}

Dictionary AIConfigService::update(const Dictionary &p_patch) {
	const String target_path = project_config_path.is_empty() ? global_config_path : project_config_path;
	Dictionary existing;
	AIError error;
	if (!_read_json_file(target_path, existing, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	const Dictionary updated = _merge_dicts(existing, p_patch);
	if (!_write_json_file(target_path, updated, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}
	invalidate("update");
	Dictionary config = get_config();
	call_deferred("emit_signal", SNAME("config_changed"), config);
	return config;
}

Dictionary AIConfigService::update_global(const Dictionary &p_patch) {
	Dictionary existing;
	AIError error;
	if (!_read_json_file(global_config_path, existing, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	const Dictionary updated = _merge_dicts(existing, p_patch);
	if (!_write_json_file(global_config_path, updated, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}
	invalidate("update_global");
	Dictionary config = get_config();
	call_deferred("emit_signal", SNAME("config_changed"), config);
	return config;
}

void AIConfigService::set_runtime_override(const Dictionary &p_override) {
	MutexLock lock(mutex);
	runtime_override = p_override.duplicate(true);
	cache_valid = false;
}

Dictionary AIConfigService::get_runtime_override() const {
	MutexLock lock(mutex);
	return runtime_override.duplicate(true);
}

void AIConfigService::invalidate(const String &p_reason) {
	{
		MutexLock lock(mutex);
		cached_entries.clear();
		cached_config.clear();
		cache_valid = false;
	}
	call_deferred("emit_signal", SNAME("config_invalidated"), p_reason);
}

String AIConfigService::get_default_provider() {
	Dictionary config = get_config();
	return String(config.get("default_provider", "fake")).strip_edges();
}

String AIConfigService::get_default_model() {
	Dictionary config = get_config();
	return String(config.get("default_model", "fake-model")).strip_edges();
}

Dictionary AIConfigService::get_provider_config(const String &p_provider) {
	const Dictionary config = get_config();
	const Dictionary providers = _dictionary_from_variant(config.get("providers", Dictionary()));
	const String provider = p_provider.strip_edges().is_empty() ? String(config.get("default_provider", "fake")) : p_provider.strip_edges();
	return _dictionary_from_variant(providers.get(provider, Dictionary()));
}

Array AIConfigService::get_system_prompt(const String &p_agent_id) {
	const Dictionary config = get_config();
	const Dictionary agents = _dictionary_from_variant(config.get("agents", Dictionary()));
	const String agent_id = p_agent_id.strip_edges().is_empty() ? String("main") : p_agent_id.strip_edges();
	const Dictionary agent = _dictionary_from_variant(agents.get(agent_id, Dictionary()));
	Array system = _array_from_variant(agent.get("system", Array()));
	if (system.is_empty()) {
		const Variant system_prompt = agent.get("system_prompt", agent.get("systemPrompt", Variant()));
		if (system_prompt.get_type() == Variant::ARRAY) {
			system = _array_from_variant(system_prompt);
		} else if (system_prompt.get_type() == Variant::STRING || system_prompt.get_type() == Variant::STRING_NAME) {
			system.push_back(String(system_prompt));
		}
	}
	if (system.is_empty()) {
		system.push_back("You are NextEngine Agent.");
	}
	return system;
}
