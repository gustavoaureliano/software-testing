#include <curl/curl.h>
#include <string.h>
#include <tools/schema.h>
#include <cjson/cJSON.h>
#include <stdlib.h>

struct llm_tool_schema {
	cJSON *root;
	cJSON *properties;
	cJSON *required;
};

static int llm_tool_schema_add_property_type(struct llm_tool_schema *schema, const char *name, const char *description, bool required, const char *type);

struct llm_tool_schema *llm_tool_schema_create_object(void) {
	struct llm_tool_schema *schema = calloc(1, sizeof(*schema));
	if (schema == NULL) { return NULL; }
	cJSON *root;
	cJSON *properties;
	cJSON *required;
	if ((root = cJSON_CreateObject()) == NULL) {
		return NULL;
	}
	if (cJSON_AddStringToObject(root, "type", "object") == NULL) {
		goto cleanup;
	}
	if ((properties = cJSON_CreateObject()) == NULL) {
		goto cleanup;
	}
	if (!cJSON_AddItemToObject(root, "properties", properties)) {
		goto cleanup;
	}
	if ((required = cJSON_CreateArray()) == NULL) {
		goto cleanup;
	}
	if (!cJSON_AddItemToObject(root, "required", required)) {
		goto cleanup;
	}
	*schema = (struct llm_tool_schema) {
		.root = root,
		.properties = properties,
		.required = required
	};
	return schema;
cleanup:
	cJSON_Delete(root);
	return NULL;
}

int llm_tool_schema_add_string_property(struct llm_tool_schema *schema, const char *name, const char *description, bool required) {
	return llm_tool_schema_add_property_type(schema, name, description, required, "string");
}

int llm_tool_schema_add_integer_property(struct llm_tool_schema *schema, const char *name, const char *description, bool required) {
	return llm_tool_schema_add_property_type(schema, name, description, required, "integer");
}

int llm_tool_schema_add_bool_property(struct llm_tool_schema *schema, const char *name, const char *description, bool required) {
	return llm_tool_schema_add_property_type(schema, name, description, required, "bool");
}

static int llm_tool_schema_add_property_type(struct llm_tool_schema *schema, const char *name, const char *description, bool required, const char *type) {
	cJSON *property;
	if (schema == NULL) { return 1; }
	if (schema->root == NULL) { return 1; }
	if (schema->properties == NULL) { return 1; }
	if (schema->required == NULL) { return 1; }
	if (name == NULL) { return 1; }
	if (strlen(name) <= 0) { return 1; }
	if (description == NULL) { return 1; }
	if (strlen(description) <= 0) { return 1; }
	if ((property = cJSON_CreateObject()) == NULL) {
		return 1;
	}
	if (cJSON_AddStringToObject(property, "type", type) == NULL) {
		goto cleanup;
	}
	if (cJSON_AddStringToObject(property, "description", description) == NULL) {
		goto cleanup;
	}
	if (!cJSON_AddItemToObject(schema->properties, name, property)) {
		goto cleanup;
	}
	if (required) {
		cJSON *string;
		if (!cJSON_IsArray(schema->required)) {
			goto cleanup;
		}
		if ((string = cJSON_CreateString(name)) == NULL) {
			goto cleanup;
		}
		if (!cJSON_AddItemToArray(schema->required, string)) {
			cJSON_Delete(string);
			goto cleanup;
		}
	}
	return 0;
cleanup:
	cJSON_Delete(property);
	return 1;
}

void llm_tool_schema_destroy(struct llm_tool_schema *schema) {
	free(schema);
}
