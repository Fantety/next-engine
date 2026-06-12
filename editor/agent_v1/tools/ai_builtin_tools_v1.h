/**************************************************************************/
/*  ai_builtin_tools_v1.h                                                  */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/tools/ai_tool_v1.h"

class AITodoService;

class AIV1ReadFileTool : public AIV1Tool {
	GDCLASS(AIV1ReadFileTool, AIV1Tool);

protected:
	static void _bind_methods();

public:
	AIV1ReadFileTool();
	virtual bool execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) override;
};

class AIV1WriteFileTool : public AIV1Tool {
	GDCLASS(AIV1WriteFileTool, AIV1Tool);

protected:
	static void _bind_methods();

public:
	AIV1WriteFileTool();
	virtual bool execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) override;
};

class AIV1ShellTool : public AIV1Tool {
	GDCLASS(AIV1ShellTool, AIV1Tool);

protected:
	static void _bind_methods();

public:
	AIV1ShellTool();
	virtual bool execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) override;
};

class AIV1TodoWriteTool : public AIV1Tool {
	GDCLASS(AIV1TodoWriteTool, AIV1Tool);

	Ref<AITodoService> todo_service;

protected:
	static void _bind_methods();

public:
	AIV1TodoWriteTool();

	void set_todo_service(const Ref<AITodoService> &p_service);
	Ref<AITodoService> get_todo_service() const;

	virtual bool execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) override;
};

class AIV1BuiltinTools : public RefCounted {
	GDCLASS(AIV1BuiltinTools, RefCounted);

protected:
	static void _bind_methods();

public:
	Ref<AIV1ReadFileTool> create_read_file_tool() const;
	Ref<AIV1WriteFileTool> create_write_file_tool() const;
	Ref<AIV1ShellTool> create_shell_tool() const;
	Ref<AIV1TodoWriteTool> create_todo_write_tool() const;
};
