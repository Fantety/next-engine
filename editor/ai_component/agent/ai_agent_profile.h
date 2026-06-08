/**************************************************************************/
/*  ai_agent_profile.h                                                    */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/hash_set.h"

struct AIAgentProfile {
	String id;
	String display_name;
	// 此 profile 禁用的工具名称黑名单。未列出的工具保持其注册时的自然权限，粒度精确到单个工具。
	HashSet<String> denied_tools;
	// 此 profile 下的写入操作是否暂存到变更审查存储中而非直接生效。这是按 profile 的能力标记，在执行时判断，不依赖模式 ID。
	bool review_changes = false;

	bool denies_tool(const String &p_tool_name) const;
	String get_capabilities_id() const;
	String get_capabilities_summary() const;

	static AIAgentProfile from_id(const String &p_profile_id);
	static AIAgentProfile get_ask_profile();
	static AIAgentProfile get_auto_profile();
};
