#pragma once

#include "core/io/xml_parser.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"

class AIStreamProcessor : public RefCounted {
	GDCLASS(AIStreamProcessor, RefCounted);

private:
	String accumulated_content;
	HashMap<String, String> tag_contents;
	String current_tag;
	String current_tag_content;
	Ref<XMLParser> xml_parser;
	String buffer_data;

	void process_tag_open(const String& tag_name);
	void process_tag_close();
	void process_content(const String& content);
	bool parse_buffer();

protected:
	static void _bind_methods();

public:
	AIStreamProcessor();

	String process_stream_data(const String& new_data);
	const HashMap<String, String>& get_tag_contents() const;
	const String& get_tag_content(const String& tag_name) const;
	const String& get_current_tag_name() const;
	const String& get_current_tag_content() const;
	const String& get_full_content() const;
	void reset();
};