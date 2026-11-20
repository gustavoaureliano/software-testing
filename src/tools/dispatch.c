#include "tools/registry.h"
#include <string.h>
#include <tools/dispatch.h>

int dispatch_tool_call(struct tool_registry registry, struct tool_context context, struct llm_tool_call *call, struct tool_result *out) {
	const struct tool_registry_entry *tool_entry = NULL;
	tool_handler_fn handler = NULL;
	if (out == NULL) { return 1; }
	memset(out, 0, sizeof(*out));
	if (registry.items == NULL || registry.count == 0) {
		tool_result_set_error(out, "Tool registry is unavailable.");
		return 0;
	}
	if (call == NULL) {
		tool_result_set_error(out, "Tool call is invalid.");
		return 0;
	}
	if (context.workspace_root == NULL || strlen(context.workspace_root) == 0) {
		tool_result_set_error(out, "Workspace context is unavailable.");
		return 0;
	}
	if (strlen(call->name) == 0) {
		tool_result_set_error(out, "Requested tool is unavailable.");
		return 0;
	}
	tool_entry = tool_registry_find(registry, call->name);
	if (tool_entry == NULL) {
		tool_result_set_error(out, "Requested tool is unavailable.");
		return 0;
	}
	handler = tool_entry->handler;
	if (handler == NULL) {
		tool_result_set_error(out, "Requested tool has no handler.");
		return 0;
	}
	if (handler(call->arguments, context, out) != 0) {
		tool_result_set_error(out, "Tool execution failed.");
		return 0;
	}
	return 0;
}
