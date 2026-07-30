#include <stdint.h>
#include <tools/handlers.h>
#include "tools/registry.h"
#include <cjson/cJSON.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int read_file_contents(const char *path, char **content, size_t *content_size);
static int get_file_size(FILE *fp, size_t *out);

int read_file(const char *arguments_json, struct tool_context context, struct tool_result *out) {
	int ret = 1;
	cJSON *root = NULL;
	cJSON *path = NULL;
	const char *path_string = NULL;
	char *file_content = NULL;
	size_t content_size = 0;
	if (out == NULL) { return ret; }
	if (arguments_json == NULL) { return ret; }
	if (strlen(arguments_json) == 0) { return ret; }
	if (context.workspace_root == NULL) { return ret; }
	if (strlen(context.workspace_root) == 0) { return ret; }
	if((root = cJSON_Parse(arguments_json)) == NULL) {
		return ret;
	}
	path = cJSON_GetObjectItem(root, "path");
	if (path == NULL) { goto cleanup; }
	if (!cJSON_IsString(path)) { goto cleanup; }
	path_string = path->valuestring;
	if (path_string == NULL) { goto cleanup; }
	if (strlen(path_string) == 0) { goto cleanup; }
	if (read_file_contents(path_string,  &file_content, &content_size) != 0) {
		goto cleanup;
	}
	if (file_content == NULL) {
		goto cleanup;
	}
	size_t available = sizeof(out->content) - 1;
	size_t to_copy = content_size;
	bool is_error = false;
	if (to_copy > available) {
		to_copy = available;
		is_error = true;
	}
	memcpy(out->content, file_content, to_copy);
	out->content[to_copy] = '\0';
	out->is_error = is_error;
	ret = 0;
cleanup:
	free(file_content);
	cJSON_Delete(root);
	return ret;
}

static int get_file_size(FILE *fp, size_t *out) {
	if (fseek(fp, 0L, SEEK_END) != 0) {
		fprintf(stderr, "Failed to determine file size: %s\n", strerror(errno));
		return 1;
	}
	long offset = ftell(fp);
	if (offset < 0) {
		fprintf(stderr, "Failed to determine file size: got negative value\n");
		return 1;
	}
	if ((unsigned long)offset > (unsigned long)SIZE_MAX) {
		fprintf(stderr, "Failed to determine file size: got value bigger than SIZE_MAX\n");
		return 1;
	}
	*out = (size_t)offset;
	return 0;
}

static int read_file_contents(const char *path, char **content, size_t *content_size) {
	FILE *fp = NULL;
	char *file_content = NULL;
	size_t file_size = 0;
	size_t read_bytes = 0;
	if (path == NULL) { return 1; }
	if (content == NULL) { return 1; }
	if (content_size == NULL) { return 1; }
	fp = fopen(path, "r");
	if (!fp) {
		fprintf(stderr, "Failed to read path '%s': %s\n", path, strerror(errno));
		return 1;
	}
	if (get_file_size(fp, &file_size) != 0) {
		goto cleanup;
	}
	if (file_size == SIZE_MAX) {
		fprintf(stderr, "Failed to read file '%s': got file of size SIZE_MAX\n", path);
		goto cleanup;
	}
	if (fseek(fp, 0L, SEEK_SET) != 0) {
		fprintf(stderr, "Failed to read file '%s': can't rewind file to the beggining\n", path);
		goto cleanup;
	}
	file_content = (char*) malloc(file_size + 1);
	if (file_content == NULL) {
		fprintf(stderr, "Failed to allocate buffer for '%s'\n", path);
		goto cleanup;
	}
	read_bytes = fread(file_content, sizeof(char), file_size, fp);
	if (read_bytes != file_size) {
		fprintf(stderr, "Failed to read file '%s': %s\n", path, strerror(errno));
		goto cleanup;
	}
	file_content[file_size] = '\0';
	fclose(fp);
	*content = file_content;
	*content_size = file_size;
	return 0;
cleanup:
	fclose(fp);
	free(file_content);
	return 1;
}
