/**************************************************************************/
/*  ai_agent_v1_ui_config_adapter.h                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "editor/agent_v1/agents/ai_agent_service_v1.h"
#include "editor/agent_v1/config/ai_config_service.h"
#include "editor/agent_v1/mcp/ai_mcp_service_v1.h"
#include "editor/agent_v1/skills/ai_skill_service_v1.h"

class AIAgentV1UIConfigAdapter : public RefCounted {
	GDCLASS(AIAgentV1UIConfigAdapter, RefCounted);

	Ref<AIConfigService> config_service;
	Ref<AIV1MCPService> mcp_service;
	Ref<AIV1SkillService> skill_service;
	Ref<AIAgentService> agent_service;

	void _ensure_defaults();
	void _wire_service_signals();

	static Dictionary _dictionary_from_variant(const Variant &p_value);
	static Array _array_from_variant(const Variant &p_value);
	Array _models_from_config(const Dictionary &p_config) const;
	Array _model_profiles_from_config(const Dictionary &p_config, bool p_enabled_only = true) const;
	Array _mcp_servers_from_config(const Dictionary &p_config) const;
	Array _skills_from_config(const Dictionary &p_config) const;
	Array _rules_from_config(const Dictionary &p_config) const;
	String _custom_instructions_from_config(const Dictionary &p_config) const;
	Array _marquees_from_config(const Dictionary &p_config) const;

	void _config_changed(const Dictionary &p_config);
	void _mcp_status_changed(const Array &p_statuses, const Dictionary &p_summary);
	void _skill_tools_changed();

protected:
	static void _bind_methods();

public:
	AIAgentV1UIConfigAdapter();

	void set_config_service(const Ref<AIConfigService> &p_service);
	Ref<AIConfigService> get_config_service() const;
	void set_mcp_service(const Ref<AIV1MCPService> &p_service);
	Ref<AIV1MCPService> get_mcp_service() const;
	void set_skill_service(const Ref<AIV1SkillService> &p_service);
	Ref<AIV1SkillService> get_skill_service() const;
	void set_agent_service(const Ref<AIAgentService> &p_service);
	Ref<AIAgentService> get_agent_service() const;

	Dictionary get_settings_snapshot();
	Array list_models();
	Array list_model_provider_presets();
	Array list_model_profiles(bool p_enabled_only = true);
	Dictionary get_model_profile(const String &p_profile_id);
	Dictionary add_model_profile(const Dictionary &p_profile, const String &p_scope = "project");
	Dictionary update_model_profile(const String &p_profile_id, const Dictionary &p_profile, const String &p_scope = "project");
	Dictionary remove_model_profile(const String &p_profile_id, const String &p_scope = "project");
	Array list_agents();
	Dictionary patch_settings(const Dictionary &p_patch, const String &p_scope = "project");
};
