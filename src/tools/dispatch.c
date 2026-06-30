#include "tools/registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tools/dispatch.h>
#include <unistd.h>

#define MAX_PATH 8192

int dispatch_tool_call(struct tool_registry registry, struct llm_tool_call *call, struct tool_result *out) {
	if (registry.items == NULL) { return 1; }
	if (registry.count == 0) { return 1; }
	if (call == NULL) { return 1; }
	if (strlen(call->name) == 0) { return 1; }
	const struct tool_registry_entry *tool_entry = tool_registry_find(registry, call->name);
	if (tool_entry == NULL) { return 1; }
	tool_handler_fn handler = tool_entry->handler;
	if (handler == NULL) { return 1; }
	// temporary workspace for test
	char *workspace = NULL;
	if (getcwd(workspace, 0) == NULL) {
		fprintf(stderr, "Failed to get current working directory!\n");
		return 1;
	}
	struct tool_context context = {
		.workspace_root = workspace
	};
	int ret = handler(call->arguments, context, out);
	free(workspace);
	return ret;
}
