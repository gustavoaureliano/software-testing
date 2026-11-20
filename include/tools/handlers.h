#ifndef TOOLS_HANDLERS_H
#define TOOLS_HANDLERS_H

#include <tools/registry.h>

int read_file(const char *arguments_json, struct tool_context context, struct tool_result *out);
int list_files(const char *arguments_json, struct tool_context context, struct tool_result *out);
int grep_files(const char *arguments_json, struct tool_context context, struct tool_result *out);
int apply_patch(const char *arguments_json, struct tool_context context, struct tool_result *out);
int edit_file(const char *arguments_json, struct tool_context context, struct tool_result *out);
int create_file(const char *arguments_json, struct tool_context context, struct tool_result *out);

#endif
