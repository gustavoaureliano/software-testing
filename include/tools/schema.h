#ifndef TOOL_SCHEMA_H
#define TOOL_SCHEMA_H

#include <stdbool.h>

struct llm_tool_schema;

struct llm_tool_schema *llm_tool_schema_create_object(void);

int llm_tool_schema_add_string_property(struct llm_tool_schema *schema, const char *name, const char *description, bool required);

int llm_tool_schema_add_integer_property(struct llm_tool_schema *schema, const char *name, const char *description, bool required);

int llm_tool_schema_add_bool_property(struct llm_tool_schema *schema, const char *name, const char *description, bool required);

void llm_tool_schema_destroy(struct llm_tool_schema *schema);

#endif
