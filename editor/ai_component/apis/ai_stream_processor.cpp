#include "ai_stream_processor.h"

#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/variant/dictionary.h"

AIStreamProcessor::AIStreamProcessor() {
	tracked_tags.push_back("thought");
	tracked_tags.push_back("final_answer");
	tracked_tags.push_back("tool");
	reset();
}

void AIStreamProcessor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("process_stream_data", "new_data"), &AIStreamProcessor::process_stream_data);
	ClassDB::bind_method(D_METHOD("parse_full_xml", "xml_data"), &AIStreamProcessor::parse_full_xml);
	ClassDB::bind_method(D_METHOD("get_tag_contents_as_dict"), &AIStreamProcessor::get_tag_contents_as_dict);
	ClassDB::bind_method(D_METHOD("get_tag_content", "tag_name"), &AIStreamProcessor::get_tag_content);
	ClassDB::bind_method(D_METHOD("get_current_tag_name"), &AIStreamProcessor::get_current_tag_name);
	ClassDB::bind_method(D_METHOD("get_current_tag_content"), &AIStreamProcessor::get_current_tag_content);
	ClassDB::bind_method(D_METHOD("get_full_content"), &AIStreamProcessor::get_full_content);
	ClassDB::bind_method(D_METHOD("reset"), &AIStreamProcessor::reset);
}

String AIStreamProcessor::process_stream_data(const String& new_data) {
	// 将新数据添加到累积内容中
	accumulated_data += new_data;
	
	// 处理数据以提取标签内容
	int pos = 0;
	
	while (pos < accumulated_data.length()) {
		// 查找下一个标签开始位置
		int tag_start = find_tag_start(accumulated_data, pos);
		
		// 如果没有找到标签开始位置，处理剩余内容
		if (tag_start == -1) {
			// 如果当前在标签内，将剩余内容添加到当前标签内容中
			if (!current_open_tag.is_empty()) {
				String remaining_content = accumulated_data.substr(pos);
				current_tag_content += remaining_content;
				update_tag_content(current_open_tag, current_tag_content);
			}
			break;
		}
		
		// 处理标签前的文本内容
		if (tag_start > pos) {
			String text_content = accumulated_data.substr(pos, tag_start - pos);
			if (!current_open_tag.is_empty()) {
				current_tag_content += text_content;
				update_tag_content(current_open_tag, current_tag_content);
			}
		}
		
		// 查找标签结束位置
		int tag_end = find_tag_end(accumulated_data, tag_start);
		
		// 如果没有找到标签结束位置，说明标签未完整，等待更多数据
		if (tag_end == -1) {
			// 如果当前在标签内，将标签前的内容添加到当前标签内容中
			if (!current_open_tag.is_empty()) {
				String partial_content = accumulated_data.substr(pos, tag_start - pos);
				current_tag_content += partial_content;
				update_tag_content(current_open_tag, current_tag_content);
			}
			break;
		}
		
		// 提取标签内容
		String tag_content = accumulated_data.substr(tag_start + 1, tag_end - tag_start - 1);
		
		// 处理标签
		if (is_closing_tag(tag_content)) {
			// 处理闭合标签
			String tag_name = extract_tag_name(tag_content.substr(1)); // 移除 '/' 字符
			
			// 如果是当前打开的标签，则保存内容
			if (tag_name == current_open_tag) {
				update_tag_content(current_open_tag, current_tag_content);
				current_open_tag = "";
				current_tag_content = "";
			}
		} else if (is_self_closing_tag(tag_content)) {
			// 处理自闭合标签
			String tag_name = extract_tag_name(tag_content);
			// 对于自闭合标签，我们将其内容设置为空字符串
			if (is_tag_tracked(tag_name)) {
				tag_contents[tag_name] = "";
			}
		} else {
			// 处理开始标签
			String tag_name = extract_tag_name(tag_content);
			
			// 如果是受跟踪的标签
			if (is_tag_tracked(tag_name)) {
				// 保存之前标签的内容
				if (!current_open_tag.is_empty()) {
					update_tag_content(current_open_tag, current_tag_content);
				}
				
				// 设置新标签
				current_open_tag = tag_name;
				current_tag_content = "";
			}
		}
		
		pos = tag_end + 1;
	}
	
	return accumulated_data;
}

void AIStreamProcessor::parse_full_xml(const String& xml_data) {
	// 重置当前状态
	reset();
	
	// 使用现有的流式处理逻辑来解析完整的XML
	process_stream_data(xml_data);
}

const HashMap<String, String>& AIStreamProcessor::get_tag_contents() const {
	return tag_contents;
}

Dictionary AIStreamProcessor::get_tag_contents_as_dict() const {
	Dictionary result;
	for (const KeyValue<String, String> &E : tag_contents) {
		result[E.key] = E.value;
	}
	return result;
}

const String& AIStreamProcessor::get_tag_content(const String& tag_name) const {
	static const String empty_string = "";
	HashMap<String, String>::ConstIterator it = tag_contents.find(tag_name);
	if (it != tag_contents.end()) {
		return it->value;
	}
	return empty_string;
}

const String& AIStreamProcessor::get_current_tag_name() const {
	return current_open_tag;
}

const String& AIStreamProcessor::get_current_tag_content() const {
	return current_tag_content;
}

const String& AIStreamProcessor::get_full_content() const {
	return accumulated_data;
}

void AIStreamProcessor::reset() {
	accumulated_data = "";
	tag_contents.clear();
	current_open_tag = "";
	current_tag_content = "";
}

bool AIStreamProcessor::is_tag_tracked(const String& tag_name) const {
	for (int i = 0; i < tracked_tags.size(); i++) {
		if (tracked_tags[i] == tag_name) {
			return true;
		}
	}
	return false;
}

void AIStreamProcessor::update_tag_content(const String& tag_name, const String& content) {
	if (is_tag_tracked(tag_name)) {
		tag_contents[tag_name] = content;
	}
}

int AIStreamProcessor::find_tag_start(const String& data, int start_pos) const {
	return data.find("<", start_pos);
}

int AIStreamProcessor::find_tag_end(const String& data, int start_pos) const {
	return data.find(">", start_pos);
}

String AIStreamProcessor::extract_tag_name(const String& tag_content) const {
	// 查找第一个空格或结束字符的位置
	int space_pos = tag_content.find(" ");
	int end_pos = (space_pos != -1) ? space_pos : tag_content.length();
	return tag_content.substr(0, end_pos);
}

bool AIStreamProcessor::is_closing_tag(const String& tag_content) const {
	return tag_content.begins_with("/");
}

bool AIStreamProcessor::is_self_closing_tag(const String& tag_content) const {
	return tag_content.ends_with("/");
}