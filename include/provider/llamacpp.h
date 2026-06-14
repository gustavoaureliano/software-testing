#ifndef LLAMACPP_PROVIDER_H
#define LLAMACPP_PROVIDER_H

#include <stddef.h>

struct llm_message {
	const char *role;
	const char *content;
};

struct llm_tool_call {
	char id[128];
	char name[128];
	char arguments[4096];
};

struct llm_response {
	char content[8192];
	struct llm_tool_call tool_calls[8];
	size_t tool_call_count;
	int has_content;
};

int llm_chat(
		const char *base_url,
		const char *model,
		const struct llm_message *messages,
		size_t message_count,
		char *raw_response,
		size_t raw_response_capacity,
		struct llm_response *out
		);

#endif
