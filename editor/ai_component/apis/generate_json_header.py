#!/usr/bin/env python3
import os
import sys
import json
import traceback

def json_to_cpp_string(json_str):
    try:
        # 转义特殊字符（注意处理顺序）
        escaped = json_str.replace('\\', '\\\\')  # 必须先处理反斜杠
        escaped = escaped.replace('"', '\\"')
        escaped = escaped.replace('\n', '\\n')
        escaped = escaped.replace('\t', '\\t')
        escaped = escaped.replace('\r', '\\r')
        
        # 分割为多行（保持结构可读性）
        lines = []
        current_line = []
        current_len = 0
        
        for char in escaped:
            current_line.append(char)
            current_len += 1
            if current_len >= 80 and char in [' ', ',', '{', '}', '[', ']']:
                lines.append(''.join(current_line))
                current_line = []
                current_len = 0
        
        if current_line:
            lines.append(''.join(current_line))
        
        return '\\\n'.join(f'    "{line}"' for line in lines)
    except Exception as e:
        print(f"String conversion error: {str(e)}")
        traceback.print_exc()
        sys.exit(1)

def main():
    if len(sys.argv) != 3:
        print("Usage: python generate_json_header.py <input.json> <output.h>")
        sys.exit(1)
    
    input_path = os.path.abspath(sys.argv[1])
    output_path = os.path.abspath(sys.argv[2])
    
    print(f"Generating header from {input_path} to {output_path}")
    
    try:
        # 验证输入文件是否存在
        if not os.path.exists(input_path):
            raise FileNotFoundError(f"Input file not found: {input_path}")
        
        # 读取并验证JSON
        with open(input_path, 'r', encoding='utf-8') as f:
            json_content = f.read()
        
        json.loads(json_content)  # 验证JSON格式
        
        # 转换为C++字符串
        cpp_string = json_to_cpp_string(json_content)
        
        # 变量命名
        namespace = "AITools"
        var_name = os.path.splitext(os.path.basename(input_path))[0].upper() + "_JSON_STR"
        guard_macro = var_name.upper() + "_H"
        
        # 生成头文件内容
        header_content = f"""#ifndef {guard_macro}
#define {guard_macro}

#include "core/string/ustring.h"

namespace {namespace} {{

// Auto-generated from {os.path.basename(input_path)}
// DO NOT EDIT - Changes will be overwritten
const String {var_name} =
{cpp_string};

}} // namespace {namespace}

#endif // {guard_macro}
"""
        # 写入文件
        output_dir = os.path.dirname(output_path)
        if not os.path.exists(output_dir):
            print(f"Creating directory: {output_dir}")
            os.makedirs(output_dir, exist_ok=True)
        
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(header_content)
            print(f"Wrote {len(header_content)} bytes to {output_path}")
            
        # 验证文件是否存在且非空
        if not os.path.exists(output_path):
            raise RuntimeError(f"File was not created: {output_path}")
        
        if os.path.getsize(output_path) == 0:
            raise RuntimeError(f"File is empty: {output_path}")
            
        print("Generation successful")
        sys.exit(0)
        
    except Exception as e:
        print(f"ERROR: {str(e)}")
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()
