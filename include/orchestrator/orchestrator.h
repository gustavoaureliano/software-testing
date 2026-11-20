#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include "provider/llamacpp.h"
#include "tools/registry.h"
#include <stdbool.h>
#include <stddef.h>

struct orchestrator_config {
	const char *base_url;
	const char *model;
	size_t max_turns;
	enum llm_tool_choice_mode initial_tool_choice;
};

enum orchestrator_stop_reason {
	ORCHESTRATOR_STOP_NONE,
	ORCHESTRATOR_STOP_COMPLETED,
	ORCHESTRATOR_STOP_TURN_LIMIT,
	ORCHESTRATOR_STOP_PROVIDER_ERROR,
	ORCHESTRATOR_STOP_INTERNAL_ERROR
};

struct orchestrator_result {
	char content[8192];
	bool has_content;
	size_t turns_completed;
	enum orchestrator_stop_reason stop_reason;
};

int orchestrator_run(
		struct orchestrator_config config,
		struct llm_message_list initial_messages,
		struct tool_registry registry,
		struct tool_context context,
		struct orchestrator_result *out
		);

#endif
