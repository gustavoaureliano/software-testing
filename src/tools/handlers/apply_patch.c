#include <tools/registry.h>
#include <tools/workspace.h>
#include <core/io.h>
#include <limits.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdbool.h>
#include <tools/handlers.h>
#include <cjson/cJSON.h>
#include <unistd.h>

#define PATCH_MAX_FILE_SIZE (1024*1024)
#define PATCH_MAX_INPUT_SIZE 4096
#define PATCH_CAPTURE_CAPACITY 7800
#define PATCH_TIMEOUT_MS 30000

static int write_patch_input(const char *path, const char *patch_text);
static int validate_patch_text(const char *text, struct tool_result *out);

int apply_patch(const char *arguments_json, struct tool_context context, struct tool_result *out) {
	cJSON *root = NULL;
	cJSON *path_node = NULL;
	cJSON *patch_node = NULL;
	char resolved_path[PATH_MAX];
	char parent_path[PATH_MAX];
	char filename[NAME_MAX + 1];
	char destination_path[PATH_MAX];
	struct stat path_info;
	char temporary_directory[] = "/tmp/swt-patch-XXXXXX";
	char patch_path[PATH_MAX] = "";
	bool temporary_directory_created = false;
	char program_output[PATCH_CAPTURE_CAPACITY];
	size_t output_size = 0;
	int exit_code = -1;
	if (out == NULL) { return 1; }
	memset(out, 0, sizeof(*out));
	if (arguments_json == NULL || arguments_json[0] == '\0') {
		tool_result_set_error(out, "apply_patch requires JSON arguments.");
		return 0;
	}
	if (context.workspace_root == NULL || context.workspace_root[0] == '\0') {
		tool_result_set_error(out, "Workspace context is unavailable.");
		return 0;
	}
	root = cJSON_Parse(arguments_json);
	if (root == NULL || !cJSON_IsObject(root)) {
		tool_result_set_error(out, "apply_patch arguments must be a valid JSON object.");
		goto cleanup;
	}
	path_node = cJSON_GetObjectItemCaseSensitive(root, "path");
	if (
			!cJSON_IsString(path_node) ||
			path_node->valuestring == NULL ||
			path_node->valuestring[0] == '\0'
	   ) {
		tool_result_set_error(out, "apply_patch 'path' must be a non-empty string.");
		goto cleanup;
	}
	patch_node = cJSON_GetObjectItemCaseSensitive(root, "patch");
	if (
			!cJSON_IsString(patch_node) ||
			patch_node->valuestring == NULL ||
			patch_node->valuestring[0] == '\0'
	   ) {
		tool_result_set_error(out, "apply_patch 'patch' must be a non-empty string.");
		goto cleanup;
	}
	if (validate_patch_text(patch_node->valuestring, out) != 0) {
		// tool_result_set_error(
		// 		out,
		// 		"Expected a single-file unified diff beginning with "
		// 		"'--- original' and '+++ modified', ending with a newline, "
		// 		"and no larger than 4096 bytes."
		// 		);
		goto cleanup;
	}
	if (resolve_workspace_existing_path(context.workspace_root, path_node->valuestring, resolved_path) != 0) {
		tool_result_set_error(out, "Requested file is unavailable or outside the workspace.");
		goto cleanup;
	}
	if (stat(resolved_path, &path_info) != 0 || !S_ISREG(path_info.st_mode)) {
		tool_result_set_error(out, "Requested path is not a regular file.");
		goto cleanup;
	}
	if (
		path_info.st_size < 0 ||
		path_info.st_size > PATCH_MAX_FILE_SIZE
	   ) {
		tool_result_set_error(out, "Target file exceeds the 1 MiB limit.");
		goto cleanup;
	}
	if (resolve_workspace_write_parent(context.workspace_root, path_node->valuestring, parent_path, filename) != 0) {
		tool_result_set_error(out, "Target parent is unavailable or outside the workspace.");
		goto cleanup;
	}
	int destination_length = snprintf(destination_path, sizeof(destination_path), "%s/%s", parent_path, filename);
	if (
		destination_length < 0 ||
		(size_t)destination_length >= sizeof(destination_path)) {
		tool_result_set_error(out, "Destination path is too long.");
		goto cleanup;
	}
	if (strcmp(destination_path, resolved_path) != 0) {
		tool_result_set_error(out, "Target must not be a symbolic link or aliased final path.");
		goto cleanup;
	}
	if(mkdtemp(temporary_directory) == NULL) {
		tool_result_set_error(out, "Failed to create temporary patch directory.");
		goto cleanup;
	}
	temporary_directory_created = true;
	int length = snprintf(patch_path, sizeof(patch_path), "%s/input.patch", temporary_directory);
	if (length < 0 || (size_t)length >= sizeof(patch_path)) {
		patch_path[0] = '\0';
		tool_result_set_error(out, "Temporary patch path is too long.");
		goto cleanup;
	}
	if (write_patch_input(patch_path, patch_node->valuestring) != 0) {
		tool_result_set_error(out, "Failed to write temporary patch input.");
		goto cleanup;
	}
	char *check_args[] = {
		"/usr/bin/patch",
		"--dry-run",
		"--batch",
		"--forward",
		"--fuzz=0",
		"--no-backup-if-mismatch",
		"--reject-file=-",
		"--input",
		patch_path,
		"--",
		resolved_path,
		NULL
	};

	char *apply_args[] = {
		"/usr/bin/patch",
		"--batch",
		"--forward",
		"--fuzz=0",
		"--no-backup-if-mismatch",
		"--reject-file=-",
		"--input",
		patch_path,
		"--",
		resolved_path,
		NULL
	};
	for (int i = 0; i < 2; i++) {
		int execution_result = run_program_capture_combined(
				i == 0 ? check_args : apply_args,
				program_output,
				sizeof(program_output),
				&output_size,
				PATCH_TIMEOUT_MS,
				&exit_code
				);
		if (execution_result != 0 || exit_code != 0) {
			const char *message = i == 0 ? 
				"Patch check failed, no changes were applied." :
				"Patch application failed, the target may be partially modified. "
				"Inspect it before retrying.";
			snprintf(
					out->content,
					sizeof(out->content),
					"%s\n%s",
					message,
					program_output[0] != '\0' ? program_output : "No command diagnostics were captured."
					);
			out->is_error = true;
			goto cleanup;
		}
	}
	snprintf(out->content, sizeof(out->content), "Patch applied successfully.\n%s", program_output);
	out->is_error = false;
cleanup:
	if (temporary_directory_created) {
		if (patch_path[0] != '\0') { unlink(patch_path); }
		rmdir(temporary_directory);
	}
	cJSON_Delete(root);
	return 0;
}

static int validate_patch_text(const char *text, struct tool_result *out) {
	const char headers[] = "--- original\n+++ modified\n";
	size_t length = strlen(text);
	size_t header_length = sizeof(headers) - 1;
	if (length > PATCH_MAX_INPUT_SIZE) {
		tool_result_set_error(out, "Patch text exceeds the 4096-byte limit. Submit a smaller edit.");
		return 1;
	}
	if (strncmp(text, headers, header_length) != 0) {
		tool_result_set_error(
			out,
			"Invalid patch headers. Use exactly these first two lines:\n"
			"--- original\n"
			"+++ modified\n"
			"Do not put the target filename in these headers; "
			"the 'path' argument selects the file."
		);
		return 1;
	}
	if (text[length - 1] != '\n') {
		tool_result_set_error(
			out,
			"Patch text must end with a newline. "
			"Append a newline character to the patch string."
		);
		return 1;
	}
	const char *line = text + header_length;
	if (strncmp(line, "@@ ", 3) != 0) {
		tool_result_set_error(
			out,
			"Missing hunk header after the file headers. "
			"Use a unified-diff header such as '@@ -1 +1 @@'."
		);
		return 1;
	}
	while (*line != '\0') {
		const char *end = strchr(line, '\n');
		if (end == NULL) {
			tool_result_set_error(out, "Every patch line must end with a newline.");
			return 1;
		}
		if (
			strncmp(line, "---", 3) == 0 ||
			strncmp(line, "+++", 3) == 0
		   ) {
			tool_result_set_error(
				out,
				"Additional file-header-like lines are unsupported. "
				"Submit one file section per call. This restricted format "
				"also rejects body lines beginning with '---' or '+++'."
			);
			return 1;
		}
		if (
			strncmp(line, "@@ ", 3) != 0 &&
			line[0] != ' ' &&
			line[0] != '+' &&
			line[0] != '-'
		   ) {
			const char marker[] = "\\ No newline at end of file";
			if (
				(size_t)(end - line) != sizeof(marker) - 1 || 
				strncmp(line, marker, sizeof(marker) - 1) != 0
			   ) {
				tool_result_set_error(
					out,
					"Invalid patch body line. Prefix unchanged lines with "
					"a space, added lines with '+', and removed lines with '-'. "
					"An unchanged blank line must contain a space prefix."
				);
				return 1;
			}
		}
		line = end + 1;
	 }
	return 0;
}

static int write_patch_input(const char *path, const char *patch_text) {
	FILE *file = fopen(path, "wb");
	if (file == NULL) { return 1; }
	size_t length = strlen(patch_text);
	int failed = fwrite(patch_text, 1, length, file) != length;
	if (fclose(file) != 0) {
		failed = 1;
	}
	return failed;
}
