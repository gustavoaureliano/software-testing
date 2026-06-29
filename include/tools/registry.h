#ifndef TOOLS_REGISTRY_H
#define TOOLS_REGISTRY_H

#include <stdbool.h>
#include <stdlib.h>
#include <tools/schema.h>

struct llm_tool_definition {
	const char *name;
	const char *description;
	const struct llm_tool_schema *parameters;
};

struct tool_result {
	char content[8192];
	bool is_error;
};

typedef int (*tool_handler_fn)(const char *arguments_json, struct tool_result *out);

struct tool_registry_entry {
	struct llm_tool_definition definition;
	tool_handler_fn handler;
};

struct tool_registry {
	const struct tool_registry_entry *items;
	size_t count;
};

const struct tool_registry_entry *tool_registry_find(const char *name);

 #endif
