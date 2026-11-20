#include <tools/handlers.h>
#include <tools/workspace.h>
#include <cjson/cJSON.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CREATE_FILE_MAX_CONTENT_SIZE 3500

static int write_content(int fd, const char *content, size_t length);

int create_file(const char *arguments_json, struct tool_context context, struct tool_result *out) {
	cJSON *root = NULL;
	cJSON *path_node = NULL;
	cJSON *content_node = NULL;
	char parent_path[PATH_MAX];
	char filename[NAME_MAX + 1];
	char destination_path[PATH_MAX];
	int fd = -1;
	bool created = false;
	size_t length = 0;

	if (out == NULL) { return 1; }
	memset(out, 0, sizeof(*out));
	if (arguments_json == NULL || arguments_json[0] == '\0') {
		tool_result_set_error(out, "create_file requires JSON arguments.");
		return 0;
	}
	if (context.workspace_root == NULL || context.workspace_root[0] == '\0') {
		tool_result_set_error(out, "Workspace context is unavailable.");
		return 0;
	}
	root = cJSON_Parse(arguments_json);
	if (root == NULL || !cJSON_IsObject(root)) {
		tool_result_set_error(out, "create_file arguments must be a valid JSON object.");
		goto cleanup;
	}
	path_node = cJSON_GetObjectItemCaseSensitive(root, "path");
	if (!cJSON_IsString(path_node) || path_node->valuestring == NULL ||
		path_node->valuestring[0] == '\0') {
		tool_result_set_error(out, "create_file 'path' must be a non-empty string.");
		goto cleanup;
	}
	content_node = cJSON_GetObjectItemCaseSensitive(root, "content");
	if (!cJSON_IsString(content_node) || content_node->valuestring == NULL) {
		tool_result_set_error(out, "create_file 'content' must be a string (may be empty).");
		goto cleanup;
	}
	length = strlen(content_node->valuestring);
	if (length > CREATE_FILE_MAX_CONTENT_SIZE) {
		tool_result_set_error(out, "File content exceeds the 3500-byte limit.");
		goto cleanup;
	}
	if (resolve_workspace_write_parent(context.workspace_root, path_node->valuestring, parent_path, filename) != 0) {
		tool_result_set_error(out, "Target parent is unavailable or outside the workspace.");
		goto cleanup;
	}
	int written = snprintf(destination_path, sizeof(destination_path), "%s/%s", parent_path, filename);
	if (written < 0 || (size_t)written >= sizeof(destination_path)) {
		tool_result_set_error(out, "Destination path is too long.");
		goto cleanup;
	}
	fd = open(destination_path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0666);
	if (fd < 0) {
		if (errno == EEXIST || errno == ELOOP) {
			tool_result_set_error(out, "Target already exists; create_file will not overwrite files or symlinks.");
		} else {
			tool_result_set_error(out, "Unable to create file in the workspace.");
		}
		goto cleanup;
	}
	created = true;
	if (write_content(fd, content_node->valuestring, length) != 0) {
		tool_result_set_error(out, "Failed to write new file; removing incomplete file.");
		goto cleanup;
	}
	if (close(fd) != 0) {
		fd = -1;
		tool_result_set_error(out, "Failed to close new file; removing incomplete file.");
		goto cleanup;
	}
	fd = -1;
	created = false;
	snprintf(
		out->content,
		sizeof(out->content),
		"Created %s (%zu bytes; %s).",
		filename,
		length,
		length > 0 && content_node->valuestring[length - 1] == '\n'
			? "ends with a newline"
			: "does not end with a newline"
	);
	out->is_error = false;

cleanup:
	if (fd >= 0) { close(fd); }
	if (created) { unlink(destination_path); }
	cJSON_Delete(root);
	return 0;
}

static int write_content(int fd, const char *content, size_t length) {
	size_t used = 0;
	while (used < length) {
		ssize_t count = write(fd, content + used, length - used);
		if (count < 0 && errno == EINTR) { continue; }
		if (count <= 0) { return 1; }
		used += (size_t)count;
	}
	return 0;
}
