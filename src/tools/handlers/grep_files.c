#include <tools/workspace.h>
#include <tools/registry.h>
#include <tools/handlers.h>
#include <cjson/cJSON.h>
#include <sys/stat.h>
#include <core/io.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#define GREP_CAPTURE_CAPACITY 7800

int grep_files(const char *arguments_json, struct tool_context context, struct tool_result *out) {
	cJSON *root = NULL;
	cJSON *path_node = NULL;
	cJSON *pattern_node = NULL;
	const char *requested_path = ".";
	char resolved_path[PATH_MAX];
	char program_output[GREP_CAPTURE_CAPACITY];
	char *pattern = NULL;
	size_t output_size = 0;
	int exit_code = 0;
	struct stat path_info;
	char *args[] = {
		"/usr/bin/grep",
		"-r",
		"-n",
		"-H",
		"-I",
		"-m",
		"50",
		"--", // end of command options
		(char *)pattern,
		resolved_path,
		NULL
	};
	if (out == NULL) { return 1; }
	if (arguments_json == NULL || strlen(arguments_json) == 0) {
		tool_result_set_error(out, "grep_files requires JSON arguments.");
		return 0;
	}
	if (context.workspace_root == NULL || strlen(context.workspace_root) == 0) {
		tool_result_set_error(out, "Workspace context is unavailable.");
		return 0;
	}
	root = cJSON_Parse(arguments_json);
	if (root == NULL || !cJSON_IsObject(root)) {
		tool_result_set_error(out, "grep_files arguments must be a valid JSON object.");
		goto cleanup;
	}
	path_node = cJSON_GetObjectItemCaseSensitive(root, "path");
	if (path_node != NULL) {
		if (
				!cJSON_IsString(path_node) ||
				path_node->valuestring == NULL ||
				strlen(path_node->valuestring) == 0
		   ) {
			tool_result_set_error(out, "grep_files 'path' must be a non-empty string");
			goto cleanup;
		}
		requested_path = path_node->valuestring;
	}
	if (resolve_workspace_existing_path(context.workspace_root, requested_path, resolved_path) != 0) {
		tool_result_set_error(out, "Requested path is unavailable or outside the workspace.");
		goto cleanup;
	}
	if (
			stat(resolved_path, &path_info) != 0 ||
			(!S_ISDIR(path_info.st_mode) && !S_ISREG(path_info.st_mode))
	   ) {
		tool_result_set_error(out, "Requested path is not a file or directory.");
		goto cleanup;
	}
	pattern_node = cJSON_GetObjectItemCaseSensitive(root, "pattern");
	if (
			!cJSON_IsString(pattern_node) ||
			pattern_node->valuestring == NULL ||
			strlen(pattern_node->valuestring) == 0
	   ) {
		tool_result_set_error(out, "grep_files 'pattern' must be a non-empty string");
		goto cleanup;
	}
	pattern = pattern_node->valuestring;
	args[8] = pattern;
	if (run_program_capture(args, program_output, sizeof(program_output), &output_size, &exit_code) != 0) {
		tool_result_set_error(out, "Failed to execute grep_files.");
		goto cleanup;
	}
	if (exit_code == 1) {
		snprintf(out->content, sizeof(out->content), "No matches found.");
		out->is_error = false;
		goto cleanup;
	}
	if (exit_code != 0) {
		tool_result_set_error(out, "grep_files command failed.");
		goto cleanup;
	}
	memcpy(out->content, program_output, output_size);
	out->content[output_size] = '\0';
	if (output_size == sizeof(program_output) - 1) {
		snprintf(out->content + output_size, sizeof(out->content) - output_size, "\n... search output may be truncated; narrow the path or pattern.");
	}
	out->is_error = false;
cleanup:
	cJSON_Delete(root);
	return 0;
}
