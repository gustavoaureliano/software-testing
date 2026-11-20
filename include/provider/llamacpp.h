#ifndef LLAMACPP_PROVIDER_H
#define LLAMACPP_PROVIDER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <tools/registry.h>

#define MAX_URL_SIZE 1024
#define LLM_MAX_TOOL_CALLS 16


struct llm_tool_call {
	char id[128];
	char name[128];
	char arguments[4096];
};

struct llm_message {
	const char *role;
	const char *content;
	const char *tool_call_id;
	const struct llm_tool_call *tool_calls;
	size_t tool_call_count;
};

struct llm_message_list {
	const struct llm_message *items;
	size_t count;
};


struct llm_response {
	char content[8192];
	struct llm_tool_call tool_calls[LLM_MAX_TOOL_CALLS];
	size_t tool_call_count;
	bool has_content;
	bool tool_calls_truncated;
};

enum llm_tool_choice_mode {
	LLM_TOOL_CHOICE_NONE,
	LLM_TOOL_CHOICE_AUTO,
	LLM_TOOL_CHOICE_REQUIRED
};

int llm_chat(
		const char *base_url,
		const char *model,
		const struct llm_message_list messages,
		const struct tool_registry registry,
		enum llm_tool_choice_mode tool_choice,
		char *raw_response,
		size_t raw_response_capacity,
		struct llm_response *out
		);

#endif
