#ifndef TOOLS_HANDLERS_H
#define TOOLS_HANDLERS_H

#include <tools/registry.h>

int read_file(const char *arguments_json, struct tool_context context, struct tool_result *out);

#endif
