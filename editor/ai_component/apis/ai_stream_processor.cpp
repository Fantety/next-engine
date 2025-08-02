#include "ai_stream_processor.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

AIStreamProcessor::AIStreamProcessor() {
    xml_parser.instantiate();
    reset();
}

String AIStreamProcessor::process_stream_data(const String& new_data) {
    // 将新数据添加到累积内容中
    accumulated_content += new_data;
    
    // 将新数据添加到缓冲区
    buffer_data += new_data;
    
    // 尝试解析缓冲区中的数据
    parse_buffer();
    
    return accumulated_content;
}

void AIStreamProcessor::process_tag_open(const String& tag_name) {
    // 保存当前标签内容到标签内容映射中
    if (!current_tag.is_empty()) {
        tag_contents[current_tag] = current_tag_content;
    }
    
    // 设置新标签
    current_tag = tag_name;
    current_tag_content = "";
}

void AIStreamProcessor::process_tag_close() {
    // 保存当前标签内容到标签内容映射中
    if (!current_tag.is_empty()) {
        tag_contents[current_tag] = current_tag_content;
    }
    // 重置当前标签
    current_tag = "";
    current_tag_content = "";
}

void AIStreamProcessor::process_content(const String& content) {
    // 将内容添加到当前标签内容中
    if (!current_tag.is_empty()) {
        current_tag_content += content;
    }
}

const HashMap<String, String>& AIStreamProcessor::get_tag_contents() const {
    return tag_contents;
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
    return current_tag;
}

const String& AIStreamProcessor::get_current_tag_content() const {
    return current_tag_content;
}

const String& AIStreamProcessor::get_full_content() const {
    return accumulated_content;
}

void AIStreamProcessor::reset() {
    accumulated_content = "";
    tag_contents.clear();
    current_tag = "";
    current_tag_content = "";
    buffer_data = "";
}

bool AIStreamProcessor::parse_buffer() {
    // 只有当缓冲区数据足够长时才尝试解析
    if (buffer_data.length() < 5) {  // 至少需要一些字符才能构成有效的标签
        return false;
    }
    
    // 检查缓冲区是否包含至少一个完整的标签对
    // 寻找第一个开始标签
    int first_open_tag_pos = buffer_data.find("<");
    if (first_open_tag_pos == -1) {
        // 没有找到开始标签，可能是纯文本内容
        return false;
    }
    
    // 寻找第一个结束标签
    int first_close_tag_pos = buffer_data.find(">", first_open_tag_pos);
    if (first_close_tag_pos == -1) {
        // 没有找到结束标签，说明标签不完整
        // 检查是否有部分标签内容可以处理
        if (first_open_tag_pos + 1 < buffer_data.length()) {
            String partial_tag = buffer_data.substr(first_open_tag_pos + 1);
            // 如果部分标签看起来像是我们关心的标签，暂时不处理，等待完整标签
            if (partial_tag.begins_with("question") || partial_tag.begins_with("thought") || 
                partial_tag.begins_with("tool") || partial_tag.begins_with("observation") || 
                partial_tag.begins_with("final_answer")) {
                // 对于特定标签，即使不完整也要处理其内容
                // 检查是否有标签内容可以处理
                String tag_content = buffer_data.substr(first_open_tag_pos + 1 + partial_tag.length());
                if (!tag_content.is_empty()) {
                    // 处理部分标签内容
                    process_tag_open(partial_tag);
                    process_content(tag_content);
                }
                return false;
            }
        }
        // 不是特定标签的部分内容，移除这部分数据
        buffer_data = buffer_data.substr(first_open_tag_pos + 1);
        return false;
    }
    
    // 提取标签名
    String first_tag_content = buffer_data.substr(first_open_tag_pos + 1, first_close_tag_pos - first_open_tag_pos - 1);
    
    // 处理标签名中可能包含的属性
    int first_space_pos = first_tag_content.find(" ");
    if (first_space_pos != -1) {
        first_tag_content = first_tag_content.substr(0, first_space_pos);
    }
    
    // 检查是否为自闭合标签
    if (first_tag_content.ends_with("/")) {
        first_tag_content = first_tag_content.substr(0, first_tag_content.length() - 1);
        // 检查是否为特定标签
        if (first_tag_content != "question" && first_tag_content != "thought" && first_tag_content != "tool" && 
            first_tag_content != "observation" && first_tag_content != "final_answer") {
            // 不是特定标签，移除这部分数据并返回
            buffer_data = buffer_data.substr(first_close_tag_pos + 1);
            return true;
        }
        
        // 自闭合标签，可以直接处理
        // 使用XMLParser处理缓冲区数据
        Vector<uint8_t> buffer;
        CharString utf8_data = buffer_data.utf8();
        buffer.resize(utf8_data.length());
        memcpy(buffer.ptrw(), utf8_data.get_data(), utf8_data.length());
        
        // 创建一个新的XMLParser实例来处理这部分数据
        Ref<XMLParser> parser;
        parser.instantiate();
        parser->_open_buffer(buffer.ptr(), buffer.size());
        
        // 解析XML数据
        while (parser->read() == OK) {
            switch (parser->get_node_type()) {
                case XMLParser::NODE_ELEMENT: {
                    String tag_name = parser->get_node_name();
                    // 检查是否为特定标签
                    if (tag_name == "question" || tag_name == "thought" || tag_name == "tool" || 
                        tag_name == "observation" || tag_name == "final_answer") {
                        process_tag_open(tag_name);
                    }
                    break;
                }
                case XMLParser::NODE_ELEMENT_END: {
                    String tag_name = parser->get_node_name();
                    // 检查是否为特定标签的结束
                    if (tag_name == "question" || tag_name == "thought" || tag_name == "tool" || 
                        tag_name == "observation" || tag_name == "final_answer") {
                        process_tag_close();
                    }
                    break;
                }
                case XMLParser::NODE_TEXT: {
                    String text = parser->get_node_data();
                    process_content(text);
                    break;
                }
                default:
                    break;
            }
        }
        
        // 清空缓冲区
        buffer_data = "";
        return true;
    }
    
    // 检查是否为特定标签
    if (first_tag_content != "question" && first_tag_content != "thought" && first_tag_content != "tool" && 
        first_tag_content != "observation" && first_tag_content != "final_answer") {
        // 不是特定标签，移除这部分数据并返回
        buffer_data = buffer_data.substr(first_close_tag_pos + 1);
        return true;
    }
    
    // 非自闭合标签，需要寻找对应的结束标签
    String closing_tag = "</" + first_tag_content + ">";
    int closing_tag_pos = buffer_data.find(closing_tag, first_close_tag_pos);
    
    if (closing_tag_pos == -1) {
        // 没有找到对应的结束标签，说明数据不完整
        // 检查是否有标签内容可以处理
        String tag_content = buffer_data.substr(first_close_tag_pos + 1);
        if (!tag_content.is_empty()) {
            // 处理部分标签内容
            process_tag_open(first_tag_content);
            process_content(tag_content);
        }
        // 保留开始标签和内容，但移除已处理的内容
        buffer_data = buffer_data.substr(0, first_close_tag_pos + 1) + tag_content;
        return false;
    }
    
    // 找到了完整的标签对，可以处理这部分数据
    String complete_data = buffer_data.substr(0, closing_tag_pos + closing_tag.length());
    
    // 使用XMLParser处理完整数据
    Vector<uint8_t> buffer;
    CharString utf8_data = complete_data.utf8();
    buffer.resize(utf8_data.length());
    memcpy(buffer.ptrw(), utf8_data.get_data(), utf8_data.length());
    
    // 创建一个新的XMLParser实例来处理这部分数据
    Ref<XMLParser> parser;
    parser.instantiate();
    parser->_open_buffer(buffer.ptr(), buffer.size());
    
    // 解析XML数据
    while (parser->read() == OK) {
        switch (parser->get_node_type()) {
            case XMLParser::NODE_ELEMENT: {
                String tag_name = parser->get_node_name();
                // 检查是否为特定标签
                if (tag_name == "question" || tag_name == "thought" || tag_name == "tool" || 
                    tag_name == "observation" || tag_name == "final_answer") {
                    process_tag_open(tag_name);
                }
                break;
            }
            case XMLParser::NODE_ELEMENT_END: {
                String tag_name = parser->get_node_name();
                // 检查是否为特定标签的结束
                if (tag_name == "question" || tag_name == "thought" || tag_name == "tool" || 
                    tag_name == "observation" || tag_name == "final_answer") {
                    process_tag_close();
                }
                break;
            }
            case XMLParser::NODE_TEXT: {
                String text = parser->get_node_data();
                process_content(text);
                break;
            }
            default:
                break;
        }
    }
    
    // 更新缓冲区，保留未处理的数据
    buffer_data = buffer_data.substr(closing_tag_pos + closing_tag.length());
    
    return true;
}