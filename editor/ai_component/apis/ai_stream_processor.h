#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

class AIStreamProcessor : public RefCounted {
	GDCLASS(AIStreamProcessor, RefCounted);

private:
	String accumulated_data;
	HashMap<String, String> tag_contents;
	String current_open_tag;
	String current_tag_content;
	Vector<String> tracked_tags;
	
	bool is_tag_tracked(const String& tag_name) const;
	void update_tag_content(const String& tag_name, const String& content);
	
	// Helper methods for parsing
	int find_tag_start(const String& data, int start_pos = 0) const;
	int find_tag_end(const String& data, int start_pos = 0) const;
	String extract_tag_name(const String& tag_content) const;
	bool is_closing_tag(const String& tag_content) const;
	bool is_self_closing_tag(const String& tag_content) const;

protected:
	static void _bind_methods();

public:
	AIStreamProcessor();

	String process_stream_data(const String& new_data);
	void parse_full_xml(const String& xml_data);
	
	const HashMap<String, String>& get_tag_contents() const;
	Dictionary get_tag_contents_as_dict() const;
	const String& get_tag_content(const String& tag_name) const;
	
	const String& get_current_tag_name() const;
	const String& get_current_tag_content() const;
	const String& get_full_content() const;
	
	void reset();
};