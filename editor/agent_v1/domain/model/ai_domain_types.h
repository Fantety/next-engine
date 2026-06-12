/**************************************************************************/
/*  ai_domain_types.h                                                     */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_error.h"

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

enum AISessionInputDelivery {
	AI_SESSION_INPUT_DELIVERY_STEER,
	AI_SESSION_INPUT_DELIVERY_QUEUE,
};

enum AISessionMessageType {
	AI_SESSION_MESSAGE_USER,
	AI_SESSION_MESSAGE_ASSISTANT,
	AI_SESSION_MESSAGE_SYSTEM,
	AI_SESSION_MESSAGE_COMPACTION,
};

enum AIToolStatus {
	AI_TOOL_STATUS_PENDING,
	AI_TOOL_STATUS_RUNNING,
	AI_TOOL_STATUS_SUCCESS,
	AI_TOOL_STATUS_FAILED,
};

String ai_session_input_delivery_to_string(AISessionInputDelivery p_delivery);
AISessionInputDelivery ai_session_input_delivery_from_string(const String &p_delivery);

String ai_session_message_type_to_string(AISessionMessageType p_type);
AISessionMessageType ai_session_message_type_from_string(const String &p_type);

String ai_tool_status_to_string(AIToolStatus p_status);
AIToolStatus ai_tool_status_from_string(const String &p_status);

struct AILocationRef {
	String directory;
	String workspace_id;

	Dictionary to_dictionary() const;
	static AILocationRef from_dictionary(const Dictionary &p_dict);
};

struct AIModelRef {
	String provider;
	String model;
	Dictionary metadata;

	Dictionary to_dictionary() const;
	static AIModelRef from_dictionary(const Dictionary &p_dict);
};

struct AITokenUsage {
	int64_t input_tokens = 0;
	int64_t output_tokens = 0;
	int64_t cache_read_tokens = 0;
	int64_t cache_write_tokens = 0;

	int64_t get_total_tokens() const;
	Dictionary to_dictionary() const;
	static AITokenUsage from_dictionary(const Dictionary &p_dict);
};

struct AIFileAttachment {
	String id;
	String path;
	String name;
	String mime;
	int64_t size_bytes = 0;
	Dictionary metadata;

	Dictionary to_dictionary() const;
	static AIFileAttachment from_dictionary(const Dictionary &p_dict);
};

struct AIAgentReference {
	String id;
	String name;
	Dictionary metadata;

	Dictionary to_dictionary() const;
	static AIAgentReference from_dictionary(const Dictionary &p_dict);
};

struct AIPromptReference {
	String id;
	String kind;
	String label;
	Variant value;
	Dictionary metadata;

	Dictionary to_dictionary() const;
	static AIPromptReference from_dictionary(const Dictionary &p_dict);
};

struct AIPrompt {
	String text;
	Vector<AIFileAttachment> files;
	Vector<AIAgentReference> agents;
	Vector<AIPromptReference> references;

	Dictionary to_dictionary() const;
	static AIPrompt from_dictionary(const Dictionary &p_dict);
};

struct AISessionInfo {
	String id;
	String project_id;
	AILocationRef location;
	String path;
	String title;
	String agent_id;
	AIModelRef model;
	double cost = 0.0;
	AITokenUsage tokens;
	uint64_t time_created = 0;
	uint64_t time_updated = 0;

	Dictionary to_dictionary() const;
	static AISessionInfo from_dictionary(const Dictionary &p_dict);
};

struct AISessionInput {
	int64_t admitted_seq = 0;
	String id;
	String message_id;
	String session_id;
	AIPrompt prompt;
	AISessionInputDelivery delivery = AI_SESSION_INPUT_DELIVERY_QUEUE;
	uint64_t time_created = 0;
	int64_t promoted_seq = 0;

	bool is_promoted() const;
	Dictionary to_dictionary() const;
	static AISessionInput from_dictionary(const Dictionary &p_dict);
};

struct AIToolState {
	AIToolStatus status = AI_TOOL_STATUS_PENDING;
	Variant input;
	Variant progress;
	Variant output;
	PackedStringArray output_paths;
	AIError error;
	Variant result;
	Dictionary provider;
	Dictionary metadata;

	Dictionary to_dictionary() const;
	static AIToolState from_dictionary(const Dictionary &p_dict);
	static AIToolState pending(const Variant &p_input = Variant(), const Dictionary &p_provider = Dictionary());
	static AIToolState running(const Variant &p_input, const Dictionary &p_provider = Dictionary());
	static AIToolState success(const Variant &p_input, const Variant &p_output, const PackedStringArray &p_output_paths = PackedStringArray(), const Dictionary &p_provider = Dictionary());
	static AIToolState failed(const AIError &p_error, const Variant &p_input = Variant(), const Dictionary &p_provider = Dictionary());
};

struct AIAssistantContent {
	String type;
	String id;
	String name;
	String text;
	AIToolState tool_state;
	Dictionary provider_metadata;
	Dictionary metadata;

	Dictionary to_dictionary() const;
	static AIAssistantContent from_dictionary(const Dictionary &p_dict);
	static AIAssistantContent text_content(const String &p_text, const String &p_id = String());
	static AIAssistantContent reasoning_content(const String &p_text, const Dictionary &p_provider_metadata = Dictionary(), const String &p_id = String());
	static AIAssistantContent tool_content(const String &p_call_id, const String &p_name, const AIToolState &p_state, const Dictionary &p_provider_metadata = Dictionary());
};

struct AISessionMessage {
	int64_t seq = 0;
	String id;
	String session_id;
	AISessionMessageType type = AI_SESSION_MESSAGE_USER;
	String text;
	Vector<AIFileAttachment> files;
	Vector<AIAgentReference> agents;
	Vector<AIPromptReference> references;
	Vector<AIAssistantContent> content;
	Dictionary metadata;
	uint64_t time_created = 0;

	bool is_runner_visible_system_message(int64_t p_baseline_seq) const;
	Dictionary to_dictionary() const;
	static AISessionMessage from_dictionary(const Dictionary &p_dict);
	static AISessionMessage user_message(const String &p_session_id, int64_t p_seq, const String &p_id, const AIPrompt &p_prompt, uint64_t p_time_created);
	static AISessionMessage assistant_shell(const String &p_session_id, int64_t p_seq, const String &p_id, const Dictionary &p_metadata = Dictionary(), uint64_t p_time_created = 0);
	static AISessionMessage system_message(const String &p_session_id, int64_t p_seq, const String &p_id, const String &p_text, const Dictionary &p_metadata = Dictionary(), uint64_t p_time_created = 0);
	static AISessionMessage compaction_message(const String &p_session_id, int64_t p_seq, const String &p_id, const String &p_text, const Dictionary &p_metadata = Dictionary(), uint64_t p_time_created = 0);
};

struct AIContextEpoch {
	String session_id;
	String baseline;
	Dictionary snapshot;
	String agent_id;
	int64_t baseline_seq = 0;
	int64_t replacement_seq = 0;
	int revision = 0;

	bool has_replacement() const;
	Dictionary to_dictionary() const;
	static AIContextEpoch from_dictionary(const Dictionary &p_dict);
};
