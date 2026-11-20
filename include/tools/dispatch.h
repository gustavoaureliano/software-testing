#ifndef TOOLS_DISPATCH_H
#define TOOLS_DISPATCH_H

#include "provider/llamacpp.h"
#include "tools/registry.h"

int dispatch_tool_call(struct tool_registry registry, struct tool_context context, struct llm_tool_call *call, struct tool_result *out);

#endif
