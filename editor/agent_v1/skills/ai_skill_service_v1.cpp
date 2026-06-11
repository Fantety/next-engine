/**************************************************************************/
/*  ai_skill_service_v1.cpp                                                */
/**************************************************************************/

#include "ai_skill_service_v1.h"

#include "editor/agent_v1/core/base/ai_id.h"
#include "editor/agent_v1/domain/context/ai_system_context.h"

#include "core/crypto/crypto_core.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/os/os.h"
#include "core/variant/variant.h"

void AIV1SkillScriptToolAdapter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_skill_id"), &AIV1SkillScriptToolAdapter::get_skill_id);
	ClassDB::bind_method(D_METHOD("get_tool_name"), &AIV1SkillScriptToolAdapter::get_tool_name);
	ClassDB::bind_method(D_METHOD("get_agent_tool_name"), &AIV1SkillScriptToolAdapter::get_agent_tool_name);
}

void AIV1SkillScriptToolAdapter::setup(AIV1SkillService *p_service, const Dictionary &p_manifest, const Dictionary &p_descriptor, const String &p_permission_default) {
	service_id = p_service ? p_service->get_instance_id() : ObjectID();
	manifest = p_manifest.duplicate(true);
	descriptor = p_descriptor.duplicate(true);
	skill_id = String(manifest.get("id", String())).strip_edges();
	skill_name = String(manifest.get("name", skill_id)).strip_edges();
	tool_name = String(descriptor.get("name", String())).strip_edges();
	agent_tool_name = AIV1SkillService::make_tool_name(skill_id, tool_name);
	permission_default = p_permission_default.strip_edges().to_lower();
	if (permission_default.is_empty()) {
		permission_default = "ask";
	}

	Dictionary tool_metadata = AIV1SkillService::_metadata_for_skill_tool(manifest, descriptor, agent_tool_name);
	tool_metadata["action"] = "skill.script.run";
	configure(String(descriptor.get("description", "Tool from skill " + skill_name + ".")), Dictionary(descriptor.get("inputSchema", descriptor.get("input_schema", Dictionary()))), Callable(), tool_metadata);
}

String AIV1SkillScriptToolAdapter::get_skill_id() const {
	return skill_id;
}

String AIV1SkillScriptToolAdapter::get_tool_name() const {
	return tool_name;
}

String AIV1SkillScriptToolAdapter::get_agent_tool_name() const {
	return agent_tool_name;
}

bool AIV1SkillScriptToolAdapter::execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) {
	AIV1SkillService *service = Object::cast_to<AIV1SkillService>(ObjectDB::get_instance(service_id));
	if (service == nullptr) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Skill service is not available.");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	return service->_execute_script_tool(manifest, descriptor, permission_default, p_arguments, p_context, r_result, r_error);
}

void AIV1SkillService::_bind_methods() {
	ClassDB::bind_static_method("AIV1SkillService", D_METHOD("sanitize_name_part", "text"), &AIV1SkillService::sanitize_name_part);
	ClassDB::bind_static_method("AIV1SkillService", D_METHOD("make_tool_name", "skill_id", "tool_name"), &AIV1SkillService::make_tool_name);
	ClassDB::bind_method(D_METHOD("set_tool_registry", "tool_registry"), &AIV1SkillService::set_tool_registry);
	ClassDB::bind_method(D_METHOD("get_tool_registry"), &AIV1SkillService::get_tool_registry);
	ClassDB::bind_method(D_METHOD("import_config", "config"), &AIV1SkillService::import_config);
	ClassDB::bind_method(D_METHOD("clear"), &AIV1SkillService::clear);
	ClassDB::bind_method(D_METHOD("refresh"), &AIV1SkillService::refresh);
	ClassDB::bind_method(D_METHOD("list_manifests"), &AIV1SkillService::list_manifests);
	ClassDB::bind_method(D_METHOD("select", "prompt", "explicit_names"), &AIV1SkillService::select);
	ClassDB::bind_method(D_METHOD("make_context_source", "skill_id", "required", "priority"), &AIV1SkillService::make_context_source, DEFVAL(true), DEFVAL(150));
	ClassDB::bind_method(D_METHOD("read_resource", "skill_id", "path", "kind"), &AIV1SkillService::read_resource);

	ADD_SIGNAL(MethodInfo("skills_changed"));
	ADD_SIGNAL(MethodInfo("tools_changed"));
}

AIV1SkillService::~AIV1SkillService() {
	clear();
}

Dictionary AIV1SkillService::_dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return Dictionary(p_value).duplicate(true);
	}
	return Dictionary();
}

Array AIV1SkillService::_array_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::ARRAY) {
		return Array(p_value).duplicate(true);
	}
	return Array();
}

String AIV1SkillService::sanitize_name_part(const String &p_text) {
	String sanitized;
	for (int i = 0; i < p_text.length(); i++) {
		const char32_t c = p_text[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
			sanitized += c;
		} else {
			sanitized += "_";
		}
	}
	while (sanitized.contains("__")) {
		sanitized = sanitized.replace("__", "_");
	}
	sanitized = sanitized.strip_edges().trim_prefix("_").trim_suffix("_");
	return sanitized.is_empty() ? String("skill") : sanitized;
}

String AIV1SkillService::make_tool_name(const String &p_skill_id, const String &p_tool_name) {
	return "skill__" + sanitize_name_part(p_skill_id) + "__" + sanitize_name_part(p_tool_name);
}

String AIV1SkillService::_manifest_id_from_directory(const String &p_path) {
	return _normalize_skill_id(p_path.trim_suffix("/").trim_suffix("\\").get_file());
}

String AIV1SkillService::_normalize_skill_id(const String &p_text) {
	return sanitize_name_part(p_text.strip_edges()).to_lower();
}

String AIV1SkillService::_frontmatter_value(const String &p_frontmatter, const String &p_key) {
	const Vector<String> lines = p_frontmatter.split("\n");
	const String key_prefix = p_key + ":";
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (!line.begins_with(key_prefix)) {
			continue;
		}

		String value = line.substr(key_prefix.length()).strip_edges();
		if ((value.begins_with("\"") && value.ends_with("\"")) || (value.begins_with("'") && value.ends_with("'"))) {
			value = value.substr(1, value.length() - 2);
		}
		return value.strip_edges();
	}
	return String();
}

String AIV1SkillService::_strip_markdown_frontmatter(const String &p_markdown) {
	if (!p_markdown.begins_with("---\n") && !p_markdown.begins_with("---\r\n")) {
		return p_markdown.strip_edges();
	}

	const int first_line_end = p_markdown.find("\n");
	if (first_line_end < 0) {
		return p_markdown.strip_edges();
	}

	const int frontmatter_end = p_markdown.find("\n---", first_line_end + 1);
	if (frontmatter_end < 0) {
		return p_markdown.strip_edges();
	}

	int body_start = frontmatter_end + 4;
	if (body_start < p_markdown.length() && p_markdown[body_start] == '\r') {
		body_start++;
	}
	if (body_start < p_markdown.length() && p_markdown[body_start] == '\n') {
		body_start++;
	}
	return p_markdown.substr(body_start).strip_edges();
}

bool AIV1SkillService::_is_valid_effect(const String &p_effect) {
	const String effect = p_effect.strip_edges().to_lower();
	return effect == "allow" || effect == "ask" || effect == "deny";
}

bool AIV1SkillService::_is_path_safe_relative(const String &p_path) {
	const String path = p_path.strip_edges().replace("\\", "/").simplify_path();
	if (path.is_empty() || path.is_absolute_path() || path.begins_with("res://") || path.begins_with("user://")) {
		return false;
	}
	if (path == "." || path == ".." || path.begins_with("../") || path.contains("/../")) {
		return false;
	}
	return true;
}

String AIV1SkillService::_content_hash(const String &p_text) {
	const PackedByteArray bytes = p_text.to_utf8_buffer();
	unsigned char hash[32];
	const uint8_t empty = 0;
	const uint8_t *bytes_ptr = bytes.size() > 0 ? bytes.ptr() : &empty;
	if (CryptoCore::sha256(bytes_ptr, bytes.size(), hash) != OK) {
		return String::num_uint64(p_text.hash64(), 16);
	}
	return String::hex_encode_buffer(hash, 32);
}

String AIV1SkillService::_command_preview(const Array &p_command) {
	Vector<String> parts;
	for (int i = 0; i < p_command.size(); i++) {
		parts.push_back(String(p_command[i]));
	}
	return String(" ").join(parts);
}

String AIV1SkillService::_permission_resource_for_script_tool(const String &p_skill_id, const String &p_tool_name, const Array &p_command) {
	const String preview = _command_preview(p_command);
	return p_skill_id + "/" + p_tool_name + "?command_hash=" + _content_hash(preview) + "&command=" + preview;
}

Dictionary AIV1SkillService::_metadata_for_skill_tool(const Dictionary &p_manifest, const Dictionary &p_descriptor, const String &p_agent_tool_name) {
	Dictionary tool_metadata;
	tool_metadata["tool_origin"] = "skill";
	tool_metadata["skill_id"] = String(p_manifest.get("id", String()));
	tool_metadata["skill_name"] = String(p_manifest.get("name", String()));
	tool_metadata["skill_tool_name"] = String(p_descriptor.get("name", String()));
	tool_metadata["skill_agent_tool_name"] = p_agent_tool_name;
	tool_metadata["source"] = "skill";
	return tool_metadata;
}

bool AIV1SkillService::_read_text_file(const String &p_path, String &r_text, AIError &r_error) {
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		Dictionary details;
		details["path"] = p_path;
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Failed to read Skill file.", details);
		return false;
	}
	r_text = file->get_as_text();
	r_error = AIError::none();
	return true;
}

bool AIV1SkillService::_read_markdown_summary_file(const String &p_path, String &r_name, String &r_description, AIError &r_error) {
	r_name.clear();
	r_description.clear();

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		Dictionary details;
		details["path"] = p_path;
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Failed to read Skill summary.", details);
		return false;
	}

	if (file->eof_reached()) {
		r_error = AIError::none();
		return true;
	}

	const String first_line = file->get_line().strip_edges();
	if (first_line != "---") {
		r_error = AIError::none();
		return true;
	}

	String frontmatter;
	int line_count = 0;
	int byte_count = 0;
	while (!file->eof_reached() && line_count < 200 && byte_count < 16384) {
		const String line = file->get_line();
		if (line.strip_edges() == "---") {
			break;
		}
		frontmatter += line + "\n";
		line_count++;
		byte_count += line.to_utf8_buffer().size();
	}

	r_name = _frontmatter_value(frontmatter, "name");
	r_description = _frontmatter_value(frontmatter, "description");
	r_error = AIError::none();
	return true;
}

bool AIV1SkillService::_parse_json_file(const String &p_path, Dictionary &r_data, AIError &r_error) {
	String text;
	if (!_read_text_file(p_path, text, r_error)) {
		return false;
	}

	Ref<JSON> json;
	json.instantiate();
	const Error parse_err = json->parse(text);
	if (parse_err != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		Dictionary details;
		details["path"] = p_path;
		details["line"] = json->get_error_line();
		details["message"] = json->get_error_message();
		r_error = AIError::make(AI_ERROR_VALIDATION, "Invalid Skill manifest JSON.", details);
		return false;
	}
	r_data = Dictionary(json->get_data()).duplicate(true);
	r_error = AIError::none();
	return true;
}

bool AIV1SkillService::_parse_skill_directory(const String &p_dir, SkillRecord &r_record, AIError &r_error) {
	const String dir = p_dir.strip_edges().simplify_path();
	const String manifest_path = dir.path_join("skill.json");
	const String skill_path = dir.path_join("SKILL.md");
	const bool has_manifest = FileAccess::exists(manifest_path);
	const bool has_skill = FileAccess::exists(skill_path);
	if (!has_manifest && !has_skill) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Skill directory has neither skill.json nor SKILL.md.");
		return false;
	}

	Dictionary manifest;
	if (has_manifest) {
		if (!_parse_json_file(manifest_path, manifest, r_error)) {
			return false;
		}
	} else {
		String name;
		String description;
		if (!_read_markdown_summary_file(skill_path, name, description, r_error)) {
			return false;
		}
		manifest["name"] = name;
		manifest["description"] = description;
	}

	String id = String(manifest.get("id", String())).strip_edges();
	if (id.is_empty()) {
		id = _manifest_id_from_directory(dir);
	}
	id = _normalize_skill_id(id);
	if (id.is_empty() || !AIId::is_valid_name(id, 128)) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Skill id is invalid.");
		return false;
	}

	String name = String(manifest.get("name", String())).strip_edges();
	String description = String(manifest.get("description", String())).strip_edges();
	if ((name.is_empty() || description.is_empty()) && has_skill) {
		String frontmatter_name;
		String frontmatter_description;
		if (!_read_markdown_summary_file(skill_path, frontmatter_name, frontmatter_description, r_error)) {
			return false;
		}
		if (name.is_empty()) {
			name = frontmatter_name;
		}
		if (description.is_empty()) {
			description = frontmatter_description;
		}
	}
	if (name.is_empty()) {
		name = id;
	}

	String entry = String(manifest.get("entry", String())).strip_edges();
	if (entry.is_empty()) {
		entry = "SKILL.md";
	}
	if (!_is_path_safe_relative(entry)) {
		Dictionary details;
		details["entry"] = entry;
		r_error = AIError::make(AI_ERROR_PERMISSION, "Skill entry path escapes its directory.", details);
		return false;
	}

	manifest["id"] = id;
	manifest["name"] = name;
	manifest["description"] = description;
	manifest["entry"] = entry;
	manifest["source"] = String(manifest.get("source", "user")).strip_edges().is_empty() ? String("user") : String(manifest.get("source", "user")).strip_edges();
	manifest["root_path"] = dir;
	if (manifest.get("triggers", Variant()).get_type() != Variant::ARRAY) {
		manifest["triggers"] = Array();
	}
	if (manifest.get("resources", Variant()).get_type() != Variant::ARRAY) {
		manifest["resources"] = Array();
	}
	if (manifest.get("tools", Variant()).get_type() != Variant::ARRAY) {
		manifest["tools"] = Array();
	}

	r_record.manifest = manifest;
	r_record.root_dir = dir;
	r_record.entry_path = dir.path_join(entry).simplify_path();
	r_error = AIError::none();
	return true;
}

bool AIV1SkillService::_resource_declared(const SkillRecord &p_record, const String &p_path, const String &p_kind, Dictionary *r_resource) {
	const Array resources = _array_from_variant(p_record.manifest.get("resources", Array()));
	const String path = p_path.strip_edges().replace("\\", "/").simplify_path();
	const String kind = p_kind.strip_edges().to_lower();
	for (int i = 0; i < resources.size(); i++) {
		if (resources[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary resource = resources[i];
		const String resource_path = String(resource.get("path", String())).strip_edges().replace("\\", "/").simplify_path();
		const String resource_kind = String(resource.get("kind", String())).strip_edges().to_lower();
		if (resource_path == path && (kind.is_empty() || resource_kind == kind)) {
			if (r_resource) {
				*r_resource = resource.duplicate(true);
			}
			return true;
		}
	}
	return false;
}

bool AIV1SkillService::_is_disabled_id_or_name(const Dictionary &p_manifest, const Array &p_disabled) {
	for (int i = 0; i < p_disabled.size(); i++) {
		const String disabled = String(p_disabled[i]).strip_edges().to_lower();
		if (disabled.is_empty()) {
			continue;
		}
		if (disabled == String(p_manifest.get("id", String())).strip_edges().to_lower() ||
				disabled == String(p_manifest.get("name", String())).strip_edges().to_lower()) {
			return true;
		}
	}
	return false;
}

bool AIV1SkillService::_matches_id_or_name(const Dictionary &p_manifest, const String &p_text) {
	const String text = p_text.strip_edges().to_lower();
	if (text.is_empty()) {
		return false;
	}
	return text == String(p_manifest.get("id", String())).strip_edges().to_lower() ||
			text == String(p_manifest.get("name", String())).strip_edges().to_lower();
}

bool AIV1SkillService::_trigger_matches_prompt(const Dictionary &p_trigger, const String &p_prompt_lower) {
	const String type = String(p_trigger.get("type", "keyword")).strip_edges().to_lower();
	const String value = String(p_trigger.get("value", String())).strip_edges().to_lower();
	if (value.is_empty()) {
		return false;
	}
	if (type == "keyword" || type == "pattern" || type == "intent") {
		return p_prompt_lower.contains(value);
	}
	if (type == "filetype") {
		return p_prompt_lower.contains("." + value.trim_prefix("."));
	}
	return false;
}

void AIV1SkillService::_close_registration_list(Vector<Ref<AIScopedRegistration>> &r_registrations) {
	for (int i = 0; i < r_registrations.size(); i++) {
		if (r_registrations[i].is_valid()) {
			r_registrations.write[i]->close();
		}
	}
	r_registrations.clear();
}

void AIV1SkillService::set_tool_registry(const Ref<AIV1ToolRegistry> &p_tool_registry) {
	MutexLock lock(mutex);
	tool_registry = p_tool_registry;
}

Ref<AIV1ToolRegistry> AIV1SkillService::get_tool_registry() const {
	MutexLock lock(mutex);
	return tool_registry;
}

bool AIV1SkillService::import_config_struct(const Dictionary &p_config, AIError &r_error) {
	Dictionary skill_config = p_config;
	if (p_config.get("skills", Variant()).get_type() == Variant::DICTIONARY) {
		skill_config = Dictionary(p_config["skills"]).duplicate(true);
	}

	Array sources = _array_from_variant(skill_config.get("sources", Array()));
	Array enabled = _array_from_variant(skill_config.get("enabled_skill_ids", skill_config.get("enabledSkillIDs", Array())));
	Array disabled = _array_from_variant(skill_config.get("disabled_skill_ids", skill_config.get("disabledSkillIDs", Array())));
	const bool imported_auto_select = bool(skill_config.get("auto_select", skill_config.get("autoSelect", true)));
	const int imported_max_skills = MAX(0, int(skill_config.get("max_skills_per_turn", skill_config.get("maxSkillsPerTurn", 4))));
	const bool imported_script_tools_enabled = bool(skill_config.get("script_tools_enabled", skill_config.get("scriptToolsEnabled", false)));
	String imported_script_permission_default = String(skill_config.get("script_permission_default", skill_config.get("scriptPermissionDefault", "ask"))).strip_edges().to_lower();
	if (!_is_valid_effect(imported_script_permission_default)) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Invalid Skill script permission default.");
		return false;
	}

	MutexLock lock(mutex);
	source_roots = sources.duplicate(true);
	enabled_skill_ids = enabled.duplicate(true);
	disabled_skill_ids = disabled.duplicate(true);
	auto_select = imported_auto_select;
	max_skills_per_turn = imported_max_skills;
	script_tools_enabled = imported_script_tools_enabled;
	script_permission_default = imported_script_permission_default;
	r_error = AIError::none();
	return true;
}

Dictionary AIV1SkillService::import_config(const Dictionary &p_config) {
	AIError error;
	const bool ok = import_config_struct(p_config, error);
	Dictionary result;
	result["success"] = ok && !error.is_error();
	if (!bool(result["success"])) {
		result["error"] = error.to_dictionary();
	}
	return result;
}

void AIV1SkillService::clear() {
	Vector<Ref<AIScopedRegistration>> registrations_to_close;
	{
		MutexLock lock(mutex);
		registrations_to_close = registrations;
		registrations.clear();
		skills.clear();
		skill_order.clear();
	}
	_close_registration_list(registrations_to_close);
}

bool AIV1SkillService::refresh_struct(AIError &r_error) {
	Array roots;
	{
		MutexLock lock(mutex);
		roots = source_roots.duplicate(true);
	}

	HashMap<String, SkillRecord> discovered;
	Vector<String> discovered_order;
	for (int i = 0; i < roots.size(); i++) {
		const String root = String(roots[i]).strip_edges().simplify_path();
		if (root.is_empty()) {
			continue;
		}

		if (FileAccess::exists(root.path_join("skill.json")) || FileAccess::exists(root.path_join("SKILL.md"))) {
			SkillRecord record;
			if (!_parse_skill_directory(root, record, r_error)) {
				return false;
			}
			const String id = String(record.manifest.get("id", String()));
			if (discovered.has(id)) {
				r_error = AIError::make(AI_ERROR_CONFLICT, "Duplicate Skill id: " + id);
				return false;
			}
			discovered[id] = record;
			discovered_order.push_back(id);
			continue;
		}

		Ref<DirAccess> dir = DirAccess::open(root);
		if (dir.is_null()) {
			continue;
		}
		dir->list_dir_begin();
		String entry = dir->get_next();
		while (!entry.is_empty()) {
			if (dir->current_is_dir() && entry != "." && entry != "..") {
				const String child = root.path_join(entry).simplify_path();
				if (FileAccess::exists(child.path_join("skill.json")) || FileAccess::exists(child.path_join("SKILL.md"))) {
					SkillRecord record;
					if (!_parse_skill_directory(child, record, r_error)) {
						dir->list_dir_end();
						return false;
					}
					const String id = String(record.manifest.get("id", String()));
					if (discovered.has(id)) {
						dir->list_dir_end();
						r_error = AIError::make(AI_ERROR_CONFLICT, "Duplicate Skill id: " + id);
						return false;
					}
					discovered[id] = record;
					discovered_order.push_back(id);
				}
			}
			entry = dir->get_next();
		}
		dir->list_dir_end();
	}

	Vector<Ref<AIScopedRegistration>> new_registrations;
	HashMap<String, SkillRecord> old_skills;
	Vector<String> old_order;
	Vector<Ref<AIScopedRegistration>> old_registrations;
	{
		MutexLock lock(mutex);
		old_skills = skills;
		old_order = skill_order;
		old_registrations = registrations;
		skills = discovered;
		skill_order = discovered_order;
		registrations.clear();
		if (!_register_skill_tools_locked(new_registrations, r_error)) {
			skills = old_skills;
			skill_order = old_order;
			registrations = old_registrations;
			_close_registration_list(new_registrations);
			return false;
		}
		registrations = new_registrations;
	}
	_close_registration_list(old_registrations);
	r_error = AIError::none();
	call_deferred("emit_signal", SNAME("skills_changed"));
	if (script_tools_enabled) {
		call_deferred("emit_signal", SNAME("tools_changed"));
	}
	return true;
}

Dictionary AIV1SkillService::refresh() {
	AIError error;
	const bool ok = refresh_struct(error);
	Dictionary result;
	result["success"] = ok && !error.is_error();
	if (!bool(result["success"])) {
		result["error"] = error.to_dictionary();
	}
	result["manifests"] = list_manifests();
	return result;
}

bool AIV1SkillService::_register_skill_tools_locked(Vector<Ref<AIScopedRegistration>> &r_new_registrations, AIError &r_error) {
	if (!script_tools_enabled) {
		r_error = AIError::none();
		return true;
	}
	if (tool_registry.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "ToolRegistry is required for Skill script tools.");
		return false;
	}

	for (int i = 0; i < skill_order.size(); i++) {
		const String skill_id = skill_order[i];
		if (!skills.has(skill_id)) {
			continue;
		}
		const SkillRecord record = skills[skill_id];
		const Array tools = _array_from_variant(record.manifest.get("tools", Array()));
		for (int tool_index = 0; tool_index < tools.size(); tool_index++) {
			if (tools[tool_index].get_type() != Variant::DICTIONARY) {
				continue;
			}
			const Dictionary descriptor = Dictionary(tools[tool_index]).duplicate(true);
			const String source_tool_name = String(descriptor.get("name", String())).strip_edges();
			const Array command = _array_from_variant(descriptor.get("command", Array()));
			if (source_tool_name.is_empty() || command.is_empty()) {
				continue;
			}

			const String agent_tool_name = make_tool_name(skill_id, source_tool_name);
			Ref<AIV1SkillScriptToolAdapter> tool;
			tool.instantiate();
			tool->setup(this, record.manifest, descriptor, script_permission_default);
			Dictionary tool_metadata = _metadata_for_skill_tool(record.manifest, descriptor, agent_tool_name);
			tool_metadata["source"] = "skill";
			Ref<AIScopedRegistration> scope = tool_registry->register_tool_scope_struct(agent_tool_name, tool, "skill", tool_metadata, &r_error);
			if (scope.is_null()) {
				_close_registration_list(r_new_registrations);
				return false;
			}
			r_new_registrations.push_back(scope);
		}
	}
	r_error = AIError::none();
	return true;
}

Array AIV1SkillService::list_manifests() const {
	MutexLock lock(mutex);
	Array result;
	for (int i = 0; i < skill_order.size(); i++) {
		const String id = skill_order[i];
		if (!skills.has(id)) {
			continue;
		}
		result.push_back(skills[id].manifest.duplicate(true));
	}
	return result;
}

Array AIV1SkillService::select(const String &p_prompt, const Array &p_explicit_names) const {
	MutexLock lock(mutex);
	Array result;
	int selected_count = 0;
	const int limit = max_skills_per_turn <= 0 ? 0 : max_skills_per_turn;

	for (int explicit_index = 0; explicit_index < p_explicit_names.size(); explicit_index++) {
		if (limit > 0 && selected_count >= limit) {
			break;
		}
		const String explicit_name = String(p_explicit_names[explicit_index]);
		for (int i = 0; i < skill_order.size(); i++) {
			const String id = skill_order[i];
			if (!skills.has(id)) {
				continue;
			}
			const Dictionary manifest = skills[id].manifest;
			if (_is_disabled_id_or_name(manifest, disabled_skill_ids) || !_matches_id_or_name(manifest, explicit_name)) {
				continue;
			}
			bool already_selected = false;
			for (int result_index = 0; result_index < result.size(); result_index++) {
				already_selected = already_selected || String(Dictionary(result[result_index]).get("skill_id", String())) == id;
			}
			if (already_selected) {
				continue;
			}
			Dictionary selected;
			selected["skill_id"] = id;
			selected["reason"] = "explicit";
			selected["priority"] = 0;
			selected["explicit"] = true;
			result.push_back(selected);
			selected_count++;
			break;
		}
	}

	for (int enabled_index = 0; enabled_index < enabled_skill_ids.size(); enabled_index++) {
		if (limit > 0 && selected_count >= limit) {
			break;
		}
		const String enabled_name = String(enabled_skill_ids[enabled_index]);
		for (int i = 0; i < skill_order.size(); i++) {
			const String id = skill_order[i];
			if (!skills.has(id)) {
				continue;
			}
			const Dictionary manifest = skills[id].manifest;
			if (_is_disabled_id_or_name(manifest, disabled_skill_ids) || !_matches_id_or_name(manifest, enabled_name)) {
				continue;
			}
			bool already_selected = false;
			for (int result_index = 0; result_index < result.size(); result_index++) {
				already_selected = already_selected || String(Dictionary(result[result_index]).get("skill_id", String())) == id;
			}
			if (already_selected) {
				continue;
			}
			Dictionary selected;
			selected["skill_id"] = id;
			selected["reason"] = "enabled";
			selected["priority"] = 50;
			selected["explicit"] = false;
			result.push_back(selected);
			selected_count++;
			break;
		}
	}

	if (!auto_select) {
		return result;
	}

	const String prompt_lower = p_prompt.to_lower();
	for (int i = 0; i < skill_order.size(); i++) {
		if (limit > 0 && selected_count >= limit) {
			break;
		}
		const String id = skill_order[i];
		if (!skills.has(id)) {
			continue;
		}
		const Dictionary manifest = skills[id].manifest;
		if (_is_disabled_id_or_name(manifest, disabled_skill_ids)) {
			continue;
		}
		bool already_selected = false;
		for (int result_index = 0; result_index < result.size(); result_index++) {
			already_selected = already_selected || String(Dictionary(result[result_index]).get("skill_id", String())) == id;
		}
		if (already_selected) {
			continue;
		}

		const Array triggers = _array_from_variant(manifest.get("triggers", Array()));
		String matched_reason;
		for (int trigger_index = 0; trigger_index < triggers.size(); trigger_index++) {
			if (triggers[trigger_index].get_type() != Variant::DICTIONARY) {
				continue;
			}
			const Dictionary trigger = triggers[trigger_index];
			if (_trigger_matches_prompt(trigger, prompt_lower)) {
				matched_reason = "trigger:" + String(trigger.get("type", "keyword")) + ":" + String(trigger.get("value", String()));
				break;
			}
		}
		if (matched_reason.is_empty()) {
			continue;
		}

		Dictionary selected;
		selected["skill_id"] = id;
		selected["reason"] = matched_reason;
		selected["priority"] = 100;
		selected["explicit"] = false;
		result.push_back(selected);
		selected_count++;
	}
	return result;
}

bool AIV1SkillService::_load_skill_document_locked(const String &p_skill_id, Dictionary &r_document, AIError &r_error) const {
	const String skill_id = _normalize_skill_id(p_skill_id);
	if (!skills.has(skill_id)) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Skill is not registered: " + p_skill_id);
		return false;
	}

	const SkillRecord record = skills[skill_id];
	String content;
	if (!_read_text_file(record.entry_path, content, r_error)) {
		return false;
	}
	const String guidance = _strip_markdown_frontmatter(content);
	const String hash = _content_hash(content);

	r_document["id"] = skill_id;
	r_document["manifest"] = record.manifest.duplicate(true);
	r_document["guidance"] = guidance;
	r_document["hash"] = hash;
	r_document["entry_path"] = record.entry_path;
	r_error = AIError::none();
	return true;
}

Dictionary AIV1SkillService::make_context_source(const String &p_skill_id, bool p_required, int p_priority) {
	Dictionary document;
	AIError error;
	{
		MutexLock lock(mutex);
		if (!_load_skill_document_locked(p_skill_id, document, error)) {
			Dictionary result;
			result["success"] = false;
			result["error"] = error.to_dictionary();
			return result;
		}
	}

	const Dictionary manifest = document.get("manifest", Dictionary());
	String text = "## Skill: " + String(manifest.get("name", document.get("id", p_skill_id))).strip_edges();
	const String description = String(manifest.get("description", String())).strip_edges();
	const String guidance = String(document.get("guidance", String())).strip_edges();
	if (!description.is_empty()) {
		text += "\n\n" + description;
	}
	if (!guidance.is_empty()) {
		text += "\n\n" + guidance;
	}

	Dictionary source_metadata;
	source_metadata["source"] = "skill";
	source_metadata["skill_id"] = document.get("id", p_skill_id);
	source_metadata["skill_name"] = manifest.get("name", String());
	source_metadata["entry"] = manifest.get("entry", String());
	source_metadata["entry_path"] = document.get("entry_path", String());
	source_metadata["hash"] = document.get("hash", String());

	AISystemContextSource source;
	source.domain = "skill/" + String(document.get("id", p_skill_id));
	source.text = text;
	source.required = p_required;
	source.available = true;
	source.priority = p_priority;
	source.metadata = source_metadata;
	source.content_hash = String(document.get("hash", String()));

	Dictionary result;
	result["success"] = true;
	result["source"] = source.to_dictionary();
	result["document"] = document.duplicate(true);
	return result;
}

Dictionary AIV1SkillService::read_resource(const String &p_skill_id, const String &p_path, const String &p_kind) {
	SkillRecord record;
	{
		MutexLock lock(mutex);
		const String skill_id = _normalize_skill_id(p_skill_id);
		if (!skills.has(skill_id)) {
			Dictionary result;
			result["success"] = false;
			result["error"] = AIError::make(AI_ERROR_UNAVAILABLE, "Skill is not registered: " + p_skill_id).to_dictionary();
			return result;
		}
		record = skills[skill_id];
	}

	const String relative_path = p_path.strip_edges().replace("\\", "/").simplify_path();
	const String kind = p_kind.strip_edges().to_lower();
	if (!_is_path_safe_relative(relative_path)) {
		Dictionary details;
		details["path"] = p_path;
		Dictionary result;
		result["success"] = false;
		result["error"] = AIError::make(AI_ERROR_PERMISSION, "Skill resource path escapes its directory.", details).to_dictionary();
		return result;
	}
	if (!_resource_declared(record, relative_path, kind)) {
		Dictionary details;
		details["path"] = relative_path;
		details["kind"] = kind;
		Dictionary result;
		result["success"] = false;
		result["error"] = AIError::make(AI_ERROR_UNAVAILABLE, "Skill resource is not declared.", details).to_dictionary();
		return result;
	}

	const String full_path = record.root_dir.path_join(relative_path).simplify_path();
	if (kind == "script" || kind == "asset") {
		Dictionary result;
		result["success"] = true;
		result["skill_id"] = String(record.manifest.get("id", String()));
		result["path"] = relative_path;
		result["kind"] = kind;
		result["file_path"] = full_path;
		return result;
	}

	String text;
	AIError error;
	if (!_read_text_file(full_path, text, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	Dictionary result;
	result["success"] = true;
	result["skill_id"] = String(record.manifest.get("id", String()));
	result["path"] = relative_path;
	result["kind"] = kind;
	result["text"] = text;
	result["content"] = text;
	result["content_hash"] = _content_hash(text);
	Dictionary resource_metadata;
	resource_metadata["source"] = "skill";
	resource_metadata["skill_id"] = result["skill_id"];
	resource_metadata["path"] = relative_path;
	resource_metadata["kind"] = kind;
	result["metadata"] = resource_metadata;
	return result;
}

bool AIV1SkillService::_execute_script_tool(const Dictionary &p_manifest, const Dictionary &p_descriptor, const String &p_permission_default, const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) const {
	const String skill_id = String(p_manifest.get("id", String())).strip_edges();
	const String skill_name = String(p_manifest.get("name", skill_id)).strip_edges();
	const String tool_name = String(p_descriptor.get("name", String())).strip_edges();
	const String agent_tool_name = make_tool_name(skill_id, tool_name);
	const Array command = _array_from_variant(p_descriptor.get("command", Array()));
	if (command.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Skill script tool command is required.");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Dictionary source = p_context.source.duplicate(true);
	source["external"] = true;
	source["skill_id"] = skill_id;
	source["skill_name"] = skill_name;
	source["skill_tool_name"] = tool_name;
	source["skill_agent_tool_name"] = agent_tool_name;
	source["skill_arguments"] = p_arguments.duplicate(true);

	if (p_context.permission_service.is_null()) {
		r_error = AIError::make(AI_ERROR_PERMISSION, "PermissionService is required for Skill script tools.");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	const String action = "skill.script.run";
	const String resource = _permission_resource_for_script_tool(skill_id, tool_name, command);
	Dictionary permission_input;
	permission_input["session_id"] = p_context.session_id;
	permission_input["action"] = action;
	permission_input["resource"] = resource;
	permission_input["reason"] = "Execute Skill script tool `" + tool_name + "` from `" + skill_name + "`.";
	permission_input["source"] = source;
	const String permission_default = p_permission_default.strip_edges().to_lower();
	if (!permission_default.is_empty()) {
		permission_input["default_effect"] = permission_default;
	}

	AIPermissionDecision decision;
	if (!p_context.permission_service->assert_permission_struct(permission_input, decision, r_error)) {
		if (!r_error.is_error()) {
			r_error = decision.error.is_error() ? decision.error : AIError::make(AI_ERROR_PERMISSION, "Skill script permission denied.");
		}
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	const String executable = String(command[0]).strip_edges();
	if (executable.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Skill script tool executable is required.");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	List<String> arguments;
	for (int i = 1; i < command.size(); i++) {
		arguments.push_back(String(command[i]));
	}

	String output;
	int exit_code = 0;
	const Error err = OS::get_singleton()->execute(executable, arguments, &output, &exit_code, true);
	if (err != OK) {
		Dictionary details;
		details["command"] = command.duplicate(true);
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to launch Skill script tool.", details);
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Dictionary structured;
	structured["skill_id"] = skill_id;
	structured["tool_name"] = tool_name;
	structured["command"] = command.duplicate(true);
	structured["exit_code"] = exit_code;
	structured["output"] = output;

	Dictionary tool_metadata = _metadata_for_skill_tool(p_manifest, p_descriptor, agent_tool_name);
	tool_metadata["skill_permission_action"] = action;
	tool_metadata["skill_permission_resource"] = resource;
	structured["metadata"] = tool_metadata.duplicate(true);

	if (exit_code != 0) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Skill script tool failed.", structured.duplicate(true));
		r_result = AIV1ToolExecutionResult::fail(r_error);
		r_result.metadata = tool_metadata;
		return false;
	}

	r_result = AIV1ToolExecutionResult::ok(structured, output, structured);
	r_result.metadata = tool_metadata;
	return true;
}
