/**************************************************************************/
/*  ai_agent_types.h                                                       */
/**************************************************************************/

#pragma once

//AI Agent在对话当中的角色
enum AIAgentRole {
	AI_AGENT_ROLE_SYSTEM,
	AI_AGENT_ROLE_USER,
	AI_AGENT_ROLE_ASSISTANT,
	AI_AGENT_ROLE_TOOL,
	AI_AGENT_ROLE_CONTEXT,
	AI_AGENT_ROLE_ERROR,
};

//AI Agent状态标识
enum AIAgentState {
	//空闲状态AI代理当前没有执行任何任务，处于等待接收新任务的闲置状态。
	AI_AGENT_STATE_IDLE,
	//上下文准备阶段代理正在为执行目标任务做前置准备，比如加载历史对话上下文、拉取需要的工具资源、整理输入参数等预处理工作。
	AI_AGENT_STATE_PREPARING_CONTEXT,
	//流式响应阶段代理正在生成AI回复内容，并且以流式的方式逐步返回响应数据。
	AI_AGENT_STATE_STREAMING,
	//等待用户确认某个高风险工具调用，例如删除脚本文件。
	AI_AGENT_STATE_WAITING_TOOL_APPROVAL,
	//已取消状态代理的当前任务被主动终止，比如用户手动取消任务、系统触发了强制取消逻辑，任务提前结束。
	AI_AGENT_STATE_CANCELLED,
	//失败状态代理在执行任务的过程中发生了错误，任务执行失败，比如模型API调用失败、上下文加载失败、内容生成异常等问题。
	AI_AGENT_STATE_FAILED,
};
