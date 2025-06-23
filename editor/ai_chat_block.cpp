#include "ai_chat_block.h"

void AIChatBlock::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_text", "text"), &AIChatBlock::set_text);
    ClassDB::bind_method(D_METHOD("to_bbcode"), &AIChatBlock::to_bbcode);
}

AIChatBlock::AIChatBlock() {
    chat_content = memnew(RichTextLabel);
    chat_content->set_use_bbcode(true);
    add_child(chat_content);
    chat_content->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    chat_content->set_scroll_follow(true);
    chat_content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    chat_content->set_v_size_flags(Control::SIZE_EXPAND_FILL);
}

void AIChatBlock::set_text(const String &p_text) {
    chat_content->set_text(p_text);
}
void AIChatBlock::add_text(const String &p_text) {
    chat_content->add_text(p_text);
    mark_text+=p_text;
}

void AIChatBlock::set_fit_content(bool fit){
    chat_content->set_fit_content(fit);
}

void AIChatBlock::to_bbcode(){
    String bbcode = mark_text;
    // 1. 标题转换
    bbcode = _replace_markdown_headings(bbcode);
    // 2. 粗体/斜体
    bbcode = _replace_markdown_bold_italic(bbcode);
    // 3. 链接和图片
    //bbcode = _replace_markdown_links(bbcode);
    //bbcode = _replace_markdown_images(bbcode);
    // 4. 代码块和行内代码
    //bbcode = _replace_markdown_code_blocks(bbcode);
    // 5. 列表处理
    //bbcode = _replace_markdown_lists(bbcode);
    // 6. 引用
    //bbcode = _replace_markdown_quotes(bbcode);
    // 7. 处理换行
    bbcode = bbcode.replace("\n\n", "[br][br]");
    bbcode = bbcode.replace("\n", "[br]");
    chat_content->set_text(bbcode);
}


// --- 辅助函数 ---
String AIChatBlock::_replace_markdown_headings(const String &text) {
    String result = text;
    PackedStringArray lines = result.split("\n");
    for (int i = 0; i < lines.size(); i++) {
        String line = lines[i];
        if (line.begins_with("# ")) {
            lines.set(i, "[h1]" + line.substr(2).strip_edges() + "[/h1]");
        } else if (line.begins_with("## ")) {
            lines.set(i, "[h2]" + line.substr(3).strip_edges() + "[/h2]");
        } else if (line.begins_with("### ")) {
            lines.set(i, "[h3]" + line.substr(4).strip_edges() + "[/h3]");
        }
    }
    return String("\n").join(lines);
}

String AIChatBlock::_replace_markdown_bold_italic(const String &text) {
    String result = text;
    // 粗体：**text** 或 __text__
    int pos = 0;
    while ((pos = result.find("**", pos)) != -1) {
        int end_pos = result.find("**", pos + 2);
        if (end_pos != -1) {
            result = result.erase(end_pos, 2);  // 先删除后标记
            result = result.erase(pos, 2);      // 再删除前标记
            result = result.insert(pos, "[b]");
            result = result.insert(end_pos - 2 + 4, "[/b]"); // 位置需要补偿
            pos = end_pos - 2 + 8; // 位置补偿：原**占2字符，[b]占3字符
        } else {
            break;
        }
    }
    // 斜体：*text* 或 _text_
    pos = 0;
    while ((pos = result.find("*", pos)) != -1) {
        if (pos > 0 && result[pos-1] == '\\') continue; // 跳过转义
        int end_pos = result.find("*", pos + 1);
        if (end_pos != -1) {
            result = result.erase(end_pos, 1);  // 删除后*
            result = result.erase(pos, 1);      // 删除前*
            result = result.insert(pos, "[i]");
            result = result.insert(end_pos - 1 + 4, "[/i]"); // 位置补偿
            pos = end_pos - 1 + 7;
        } else {
            break;
        }
    }
    return result;
}

String AIChatBlock::_replace_markdown_links(const String &text) {
    String result = text;
    int pos = 0;
    
    while ((pos = result.find("[", pos)) != -1) {
        int text_end = result.find("]", pos);
        if (text_end == -1) break;
        
        int url_start = result.find("(", text_end);
        if (url_start != text_end + 1) continue;
        
        int url_end = result.find(")", url_start);
        if (url_end == -1) break;
        
        String link_text = result.substr(pos + 1, text_end - pos - 1);
        String url = result.substr(url_start + 1, url_end - url_start - 1);
        
        result = result.substr(0, pos) + 
                "[url=" + url + "]" + link_text + "[/url]" +
                result.substr(url_end + 1);
        
        pos = url_end + 1;
    }
    return result;
}
String AIChatBlock::_replace_markdown_code_blocks(const String &text) {
    String result;
    int pos = 0;
    int last_pos = 0;
    bool in_code_block = false;
    
    while ((pos = text.find("```", last_pos)) != -1) {
        result += text.substr(last_pos, pos - last_pos);
        if (!in_code_block) {
            // 代码块开始
            int line_end = text.find("\n", pos);
            if (line_end == -1) line_end = text.length();
            
            result += "[code]";
            last_pos = line_end + 1;
            in_code_block = true;
        } else {
            // 代码块结束
            result += "[/code]";
            last_pos = pos + 3;
            in_code_block = false;
        }
    }
    result += text.substr(last_pos);
    return result;
}
String AIChatBlock::_replace_markdown_images(const String &text) {
    String result;
    int pos = 0;
    int last_pos = 0;
    
    while ((pos = text.find("![", last_pos)) != -1) {
        result += text.substr(last_pos, pos - last_pos);
        
        int alt_end = text.find("]", pos);
        if (alt_end == -1) break;
        
        int url_start = text.find("(", alt_end);
        if (url_start != alt_end + 1) break;
        
        int url_end = text.find(")", url_start);
        if (url_end == -1) break;
        
        String alt_text = text.substr(pos + 2, alt_end - pos - 2);
        String url = text.substr(url_start + 1, url_end - url_start - 1);
        
        result += "[img=" + alt_text + "]" + url + "[/img]";
        last_pos = url_end + 1;
    }
    
    result += text.substr(last_pos);
    return result;
}
String AIChatBlock::_replace_markdown_quotes(const String &text) {
    String result;
    PackedStringArray lines = text.split("\n");
    bool in_quote = false;
    
    for (int i = 0; i < lines.size(); i++) {
        String line = lines[i];
        if (line.begins_with("> ")) {
            if (!in_quote) {
                line = "[quote]" + line.substr(2);
                in_quote = true;
            } else {
                line = line.substr(2);
            }
        } else if (in_quote) {
            line = "[/quote]" + line;
            in_quote = false;
        }
        lines.set(i, line);
    }
    
    if (in_quote) {
        lines.push_back("[/quote]");
    }
    
    return String("\n").join(lines);
}
String AIChatBlock::_replace_markdown_lists(const String &text) {
    String result;
    PackedStringArray lines = text.split("\n");
    int current_indent = 0;
    
    for (int i = 0; i < lines.size(); i++) {
        String line = lines[i];
        String stripped = line.strip_edges();
        int indent = line.length() - stripped.length();
        
        if (stripped.begins_with("- ") || stripped.begins_with("* ")) {
            // 计算列表层级
            int new_indent = indent / 4;  // 假设每级缩进4空格
            
            // 关闭上一级列表
            while (current_indent > new_indent) {
                result += "[/ul]";
                current_indent--;
            }
            
            // 开启新列表层级
            while (current_indent < new_indent) {
                result += "[ul]";
                current_indent++;
            }
            
            result += "[*]" + stripped.substr(2) + "\n";
        } else {
            // 非列表行，关闭所有打开的列表
            while (current_indent > 0) {
                result += "[/ul]";
                current_indent--;
            }
            result += line + "\n";
        }
    }
    
    // 关闭最后未闭合的列表
    while (current_indent > 0) {
        result += "[/ul]";
        current_indent--;
    }
    
    return result;
}
String AIChatBlock::_replace_markdown_inline_code(const String &text) {
    String result;
    int pos = 0;
    int last_pos = 0;
    
    while ((pos = text.find("`", last_pos)) != -1) {
        result += text.substr(last_pos, pos - last_pos);
        
        int end_pos = text.find("`", pos + 1);
        if (end_pos == -1) break;
        
        String code = text.substr(pos + 1, end_pos - pos - 1);
        result += "[code]" + code + "[/code]";
        last_pos = end_pos + 1;
    }
    
    result += text.substr(last_pos);
    return result;
}
// 其他辅助函数（图片、代码块等）类似实现...