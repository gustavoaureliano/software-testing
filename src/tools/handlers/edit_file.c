#include <tools/handlers.h>
#include <tools/workspace.h>
#include <cjson/cJSON.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define EDIT_MAX_FILE_SIZE (1024 * 1024)
#define EDIT_MAX_TEXT_SIZE 4096

static int read_editable_file(const char *path, char *content, size_t *length);
static int write_replacement(const char *parent_path, const char *filename,
		const char *content, size_t length, const struct stat *original);

int edit_file(const char *arguments_json, struct tool_context context, struct tool_result *out) {
	cJSON *root = NULL;
	cJSON *path_node = NULL;
	cJSON *old_node = NULL;
	cJSON *new_node = NULL;
	const char *old_text = NULL;
	const char *new_text = NULL;
	char resolved_path[PATH_MAX];
	char parent_path[PATH_MAX];
	char filename[NAME_MAX + 1];
	char destination_path[PATH_MAX];
	char *original = NULL;
	char *replacement = NULL;
	size_t original_length = 0;
	size_t old_length = 0;
	size_t new_length = 0;
	size_t prefix_length = 0;
	size_t result_length = 0;
	const char *match = NULL;
	struct stat path_info;
	if (out == NULL) { return 1; }
	memset(out, 0, sizeof(*out));
	if (arguments_json == NULL || arguments_json[0] == '\0') {
		tool_result_set_error(out, "edit_file requires JSON arguments.");
		return 0;
	}
	if (context.workspace_root == NULL || context.workspace_root[0] == '\0') {
		tool_result_set_error(out, "Workspace context is unavailable.");
		return 0;
	}
	root = cJSON_Parse(arguments_json);
	if (root == NULL || !cJSON_IsObject(root)) {
		tool_result_set_error(out, "edit_file arguments must be a JSON object.");
		goto cleanup;
	}
	path_node = cJSON_GetObjectItemCaseSensitive(root, "path");
	old_node = cJSON_GetObjectItemCaseSensitive(root, "old_text");
	new_node = cJSON_GetObjectItemCaseSensitive(root, "new_text");
	if (!cJSON_IsString(path_node) || path_node->valuestring == NULL ||
		path_node->valuestring[0] == '\0') {
		tool_result_set_error(out, "edit_file 'path' must be a non-empty string.");
		goto cleanup;
	}
	if (!cJSON_IsString(old_node) || old_node->valuestring == NULL ||
		old_node->valuestring[0] == '\0') {
		tool_result_set_error(out, "edit_file 'old_text' must be a non-empty string.");
		goto cleanup;
	}
	if (!cJSON_IsString(new_node) || new_node->valuestring == NULL) {
		tool_result_set_error(out, "edit_file 'new_text' must be a string.");
		goto cleanup;
	}
	old_text = old_node->valuestring;
	new_text = new_node->valuestring;
	old_length = strlen(old_text);
	new_length = strlen(new_text);
	if (old_length > EDIT_MAX_TEXT_SIZE || new_length > EDIT_MAX_TEXT_SIZE - old_length) {
		tool_result_set_error(out, "Combined old_text and new_text must be at most 4096 bytes.");
		goto cleanup;
	}
	if (strcmp(old_text, new_text) == 0) {
		tool_result_set_error(out, "old_text and new_text are identical; no edit was made.");
		goto cleanup;
	}
	if (resolve_workspace_existing_path(context.workspace_root, path_node->valuestring, resolved_path) != 0) {
		tool_result_set_error(out, "Target is unavailable or outside the workspace.");
		goto cleanup;
	}
	if (resolve_workspace_write_parent(context.workspace_root, path_node->valuestring, parent_path, filename) != 0) {
		tool_result_set_error(out, "Target parent is unavailable or outside the workspace.");
		goto cleanup;
	}
	int path_length = snprintf(destination_path, sizeof(destination_path), "%s/%s", parent_path, filename);
	if (path_length < 0 || (size_t)path_length >= sizeof(destination_path)) {
		tool_result_set_error(out, "Target path is too long.");
		goto cleanup;
	}
	if (strcmp(destination_path, resolved_path) != 0) {
		tool_result_set_error(out, "Target must be an existing file, not a final symbolic link.");
		goto cleanup;
	}
	if (lstat(resolved_path, &path_info) != 0 || !S_ISREG(path_info.st_mode)) {
		tool_result_set_error(out, "Target must be an existing regular file, not a symbolic link.");
		goto cleanup;
	}
	if (path_info.st_size < 0 || path_info.st_size > EDIT_MAX_FILE_SIZE) {
		tool_result_set_error(out, "Target file exceeds the 1 MiB limit.");
		goto cleanup;
	}
	original = malloc(EDIT_MAX_FILE_SIZE + 2);
	if (original == NULL || read_editable_file(resolved_path, original, &original_length) != 0) {
		tool_result_set_error(out, "Unable to read target as a bounded text file.");
		goto cleanup;
	}
	match = strstr(original, old_text);
	if (match == NULL) {
		tool_result_set_error(out, "old_text was not found. Reread the file and copy its exact text, including whitespace.");
		goto cleanup;
	}
	if (strstr(match + 1, old_text) != NULL) {
		tool_result_set_error(out, "old_text matches more than once. Include more surrounding text to make it unique.");
		goto cleanup;
	}
	prefix_length = (size_t)(match - original);
	result_length = original_length - old_length + new_length;
	if (result_length > EDIT_MAX_FILE_SIZE) {
		tool_result_set_error(out, "Edited file would exceed the 1 MiB limit.");
		goto cleanup;
	}
	replacement = malloc(result_length + 1);
	if (replacement == NULL) {
		tool_result_set_error(out, "Unable to allocate edited content.");
		goto cleanup;
	}
	memcpy(replacement, original, prefix_length);
	memcpy(replacement + prefix_length, new_text, new_length);
	memcpy(replacement + prefix_length + new_length,
		original + prefix_length + old_length,
		original_length - prefix_length - old_length);
	replacement[result_length] = '\0';

	if (write_replacement(parent_path, filename, replacement, result_length, &path_info) != 0) {
		tool_result_set_error(out, "Unable to write or commit replacement; inspect the file before retrying.");
		goto cleanup;
	}
	snprintf(out->content, sizeof(out->content), "Edited %s (one exact replacement).", filename);
	out->is_error = false;

cleanup:
	free(replacement);
	free(original);
	cJSON_Delete(root);
	return 0;
}

static int read_editable_file(const char *path, char *content, size_t *length) {
	FILE *file = fopen(path, "rb");
	if (file == NULL) { return 1; }
	size_t used = fread(content, 1, EDIT_MAX_FILE_SIZE + 1, file);
	int failed = ferror(file) || used > EDIT_MAX_FILE_SIZE || memchr(content, '\0', used) != NULL;
	if (fclose(file) != 0) { failed = 1; }
	if (failed) { return 1; }
	content[used] = '\0';
	*length = used;
	return 0;
}

static int write_replacement(const char *parent_path, const char *filename,
		const char *content, size_t length, const struct stat *original) {
	char temporary_path[PATH_MAX];
	char destination_path[PATH_MAX];
	struct stat current;
	int written = snprintf(temporary_path, sizeof(temporary_path), "%s/.swt-edit-XXXXXX", parent_path);
	if (written < 0 || (size_t)written >= sizeof(temporary_path)) { return 1; }
	written = snprintf(destination_path, sizeof(destination_path), "%s/%s", parent_path, filename);
	if (written < 0 || (size_t)written >= sizeof(destination_path)) { return 1; }
	int fd = mkstemp(temporary_path);
	if (fd < 0) { return 1; }
	int failed = fchmod(fd, original->st_mode & 0777) != 0;
	if (!failed) {
		FILE *file = fdopen(fd, "wb");
		if (file == NULL) {
			failed = 1;
			close(fd);
		} else {
			if (fwrite(content, 1, length, file) != length) { failed = 1; }
			if (fclose(file) != 0) { failed = 1; }
		}
	} else {
		close(fd);
	}
	if (!failed) {
		if (lstat(destination_path, &current) != 0 ||
			!S_ISREG(current.st_mode) ||
			current.st_dev != original->st_dev ||
			current.st_ino != original->st_ino) {
			failed = 1;
		} else if (rename(temporary_path, destination_path) != 0) {
			failed = 1;
		}
	}
	if (failed) { unlink(temporary_path); }
	return failed;
}
