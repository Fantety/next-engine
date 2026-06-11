/**************************************************************************/
/*  ai_context_source_registry.h                                          */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/config/ai_config_service.h"
#include "editor/agent_v1/domain/context/ai_system_context.h"
#include "editor/agent_v1/domain/model/ai_domain_types.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"

class AIContextSourceRegistry : public RefCounted {
	GDCLASS(AIContextSourceRegistry, RefCounted);

	Ref<AIConfigService> config_service;
	Vector<AISystemContextSource> manual_sources;
	bool blocked = false;
	String blocked_reason;
	mutable Mutex mutex;

	static AISystemContextSource _make_source(const String &p_domain, const String &p_text, bool p_required, int p_priority, const Dictionary &p_metadata = Dictionary(), bool p_available = true);
	static Dictionary _dictionary_from_variant(const Variant &p_value);
	static Array _array_from_variant(const Variant &p_value);
	static void _append_text_sources(Vector<AISystemContextSource> &r_sources, const String &p_domain_prefix, const Variant &p_value, bool p_required, int p_priority, const Dictionary &p_metadata = Dictionary());
	static String _host_date();

protected:
	static void _bind_methods();

public:
	AIContextSourceRegistry();

	void set_config_service(const Ref<AIConfigService> &p_config_service);
	Ref<AIConfigService> get_config_service() const;

	void add_source(const Dictionary &p_source);
	void add_source_struct(const AISystemContextSource &p_source);
	void clear_sources();
	void set_blocked(bool p_blocked, const String &p_reason = String());

	bool load_struct(const String &p_agent_id, const AILocationRef &p_location, const String &p_provider, const String &p_model, AISystemContext &r_context, AIError &r_error) const;
	Dictionary load(const String &p_agent_id, const Dictionary &p_location, const String &p_provider, const String &p_model) const;
};
