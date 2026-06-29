#ifndef TOOL_SCHEMA_INTERNAL_H
#define TOOL_SCHEMA_INTERNAL_H

#include <cjson/cJSON.h>
#include <tools/schema.h>

cJSON *llm_tool_schema_dup_json(const struct llm_tool_schema *schema);

#endif
