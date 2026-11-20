#include <limits.h>
#include <stdint.h>
#include <tools/handlers.h>
#include "tools/registry.h"
#include <tools/workspace.h>
#include <cjson/cJSON.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define READ_FILE_DEFAULT_START_LINE 1
#define READ_FILE_MAX_LINE_COUNT 200

static int parse_line_range(const cJSON *root, size_t *start_line, size_t *end_line);
static int parse_positive_line_number(const cJSON *node, size_t *out);
static int read_file_lines(const char *path, size_t start_line, size_t end_line, struct tool_result *out);

int read_file(const char *arguments_json, struct tool_context context, struct tool_result *out) {
	int ret = 1;
	cJSON *root = NULL;
	cJSON *path = NULL;
	const char *path_string = NULL;
	size_t start_line = 0;
	size_t end_line = 0;
	char resolved_path[PATH_MAX];
	if (out == NULL) { return ret; }
	if (arguments_json == NULL || strlen(arguments_json) == 0) {
		tool_result_set_error(out, "read_file requires JSON arguments.");
		return 0;
	}
	if (context.workspace_root == NULL || strlen(context.workspace_root) == 0) {
		tool_result_set_error(out, "Workspace context is unavailable.");
		return 0;
	}
	root = cJSON_Parse(arguments_json);
	if(root == NULL || !cJSON_IsObject(root)) {
		tool_result_set_error(out, "read_file arguments must be valid JSON object.");
		ret = 0;
		goto cleanup;
	}
	path = cJSON_GetObjectItem(root, "path");
	if (path == NULL || !cJSON_IsString(path)) {
		tool_result_set_error(out, "read_file requires a non-empty string 'path' argument.");
		ret = 0;
		goto cleanup;
	}
	path_string = path->valuestring;
	if (path_string == NULL || strlen(path_string) == 0) {
		tool_result_set_error(out, "read_file requires a non-empty string 'path' argument.");
		ret = 0;
		goto cleanup;
	}
	if (parse_line_range(root, &start_line, &end_line) != 0) {
		tool_result_set_error(out, "read_file line range must use positive integers and contain at most 200 lines.");
		ret = 0;
		goto cleanup;
	}
	if (resolve_workspace_existing_path(context.workspace_root, path_string, resolved_path) != 0) {
		tool_result_set_error(out, "Requested file is unavailable or outside the workspace.");
		ret = 0;
		goto cleanup;
	}
	if (read_file_lines(resolved_path, start_line, end_line, out) != 0) {
		tool_result_set_error(out, "Internal error while reading file.");
	}
	ret = 0;
cleanup:
	cJSON_Delete(root);
	return ret;
}

static int parse_positive_line_number(const cJSON *node, size_t *out) {
	double value = 0.0;
	size_t line_number = 0;
	if (node == NULL) { return 1; }
	if (out == NULL) { return 1; }
	if (!cJSON_IsNumber(node)) { return 1; }
	value = node->valuedouble;
	if (value < 1.0 || value > (double) SIZE_MAX) { return 1; }
	line_number = (size_t) value;
	if ((double) line_number != value) { return 1; } // verify later
	*out = line_number;
	return 0;
}

static int parse_line_range(const cJSON *root, size_t *start_line, size_t *end_line) {
	const cJSON *start_node = NULL;
	const cJSON *end_node = NULL;
	size_t start = READ_FILE_DEFAULT_START_LINE;
	size_t end = 0;
	if (root == NULL) { return 1; }
	if (start_line == NULL) { return 1; }
	if (end_line == NULL) { return 1; }
	start_node = cJSON_GetObjectItemCaseSensitive(root, "start_line");
	end_node = cJSON_GetObjectItemCaseSensitive(root, "end_line");
	if (start_node != NULL && parse_positive_line_number(start_node, &start) != 0) {
		return 1;
	}
	if (end_node != NULL) {
		if (parse_positive_line_number(end_node, &end) != 0) {
			return 1;
		}
	} else {
		if (start > SIZE_MAX - (READ_FILE_MAX_LINE_COUNT - 1)) {
			return 1;
		}
		end = start + READ_FILE_MAX_LINE_COUNT - 1;
	}
	if (end < start) { return 1; }
	if (end - start >= READ_FILE_MAX_LINE_COUNT) { return 1; }
	*start_line = start;
	*end_line = end;
	return 0;
}

static int read_file_lines(const char *path, size_t start_line, size_t end_line, struct tool_result *out) {
	FILE *fp = NULL;
	char *line = NULL;
	size_t line_capacity = 0;
	size_t line_number = 0;
	size_t used = 0;
	ssize_t line_length = 0;
	int ret = 1;
	if (path == NULL) { return 1; }
	if (out == NULL) { return 1; }
	if (start_line == 0 || end_line < start_line) { return 1; }
	fp = fopen(path, "r");
	if (!fp) {
		tool_result_set_error(out, "Failed to open requested file.");
		return 0;
	}
	while ((line_length = getline(&line, &line_capacity, fp)) != -1) {
		size_t length = (size_t) line_length;
		if (line_number == SIZE_MAX) {
			tool_result_set_error(out, "File has too many lines to read.");
			ret = 0;
			goto cleanup;
		}
		line_number++;
		if (line_number < start_line) { continue; }
		if (line_number > end_line) { break; }
		if (length > sizeof(out->content) - 1 - used) {
			tool_result_set_error(out, "Selected lines exceed the tool output limit. Request fewer lines.");
			ret = 0;
			goto cleanup;
		}
		memcpy(out->content + used, line, length);
		used += length;
	}
	if (ferror(fp)) {
		tool_result_set_error(out, "Failed while reading requested file.");
		ret = 0;
		goto cleanup;
	}
	if (used == 0) {
		snprintf(out->content, sizeof(out->content), "No content found in requested line range.");
	} else {
		out->content[used] = '\0';
	}
	out->is_error = false;
	ret = 0;
cleanup:
	free(line);
	fclose(fp);
	return ret;
}
