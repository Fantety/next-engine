该项目（NextEngine）是 Godot Engine 的二次开发项目。

主要改动集中在：
- editor/agent_v1 AI 相关功能
- editor/user_system 新增用户系统
- scene/gui 新增 MarkdownViewer 节点
- .devdocs/ 相关文档存放位置

在实现任何功能前请查看相关代码是否有直接可用的基础设施、相关domian数据模型，已经实现的代码或者模块，避免重复实现，造成代码冗余。
静态检查注意事项：
- 修改或新增 C/C++ 相关文件后，必须运行仓库的 prek 静态检查；本地没有 prek 时使用 `uvx --from prek==0.4.5 prek run --show-diff-on-failure --color=always --files <改动文件列表>`。
- 如果改动范围较大，使用 `uvx --from prek==0.4.5 prek run --all-files` 做全量检查。
- `clang-format`、`copyright-headers`、`file-format` 等 hook 会自动修改文件；提交前必须复跑到通过，并把 hook 自动修改的文件一并提交。
- 新增 `.cpp/.h` 等源码文件必须使用 Godot 标准版权头；include 顺序以 `clang-format` 和 `validate-includes` 的结果为准。
- 文本文件不要留下多余尾随空行、尾随空格或错误换行格式；以 `file-format` 的修复结果为准。
