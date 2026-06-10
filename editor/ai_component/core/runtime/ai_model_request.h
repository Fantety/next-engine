/**************************************************************************/
/*  ai_model_request.h                                                    */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

enum AIModelPartType {
	AI_MODEL_PART_TEXT,
	AI_MODEL_PART_IMAGE,
	AI_MODEL_PART_AUDIO,
	AI_MODEL_PART_FILE,
};

struct AIModelPart {
	AIModelPartType type = AI_MODEL_PART_TEXT;
	String text;
	String mime;
	String name;
	String data;
	Dictionary metadata;

	Dictionary to_dictionary() const;

	static String type_to_string(AIModelPartType p_type);
	static AIModelPart text_part(const String &p_text);
	static AIModelPart data_part(AIModelPartType p_type, const String &p_mime, const String &p_data, const String &p_name = String());
};

struct AIModelMessage {
	String id;
	String role;
	Vector<AIModelPart> parts;
	Dictionary metadata;

	Dictionary to_dictionary() const;

	static AIModelMessage text_message(const String &p_role, const String &p_text, const String &p_id = String());
};

struct AIModelToolDefinition {
	String name;
	String description;
	Dictionary input_schema;
	Dictionary metadata;

	bool is_valid() const;
	Dictionary to_dictionary() const;
};

struct AIModelRequest {
	String request_id;
	String provider;
	String model;
	Vector<AIModelPart> system;
	Vector<AIModelMessage> messages;
	Vector<AIModelToolDefinition> tools;
	Dictionary provider_options;
	Dictionary metadata;
	int max_output_tokens = 0;
	bool stream = true;

	Dictionary to_dictionary() const;
};
