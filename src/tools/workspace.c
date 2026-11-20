
#include <sys/stat.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tools/workspace.h>
#include <stddef.h>

static int is_path_within_workspace(const char *resolved_workspace, const char *resolved_path) {
	size_t workspace_length = 0;
	if (resolved_workspace == NULL) { return 0; }
	if (resolved_path == NULL) { return 0; }
	if (strcmp(resolved_workspace, "/") == 0) { return 1; }
	workspace_length = strlen(resolved_workspace);
	if (strncmp(resolved_path, resolved_workspace, workspace_length) != 0) { return 0; }
	if (resolved_path[workspace_length] != '\0' && resolved_path[workspace_length] != '/') {
		return 0;
	}
	return 1;
}

int resolve_workspace_existing_path(const char *workspace_root, const char *requested_path, char resolved_path[PATH_MAX]) {
	char resolved_workspace[PATH_MAX];
	char candidate_path[PATH_MAX];
	int written = 0;
	if (workspace_root == NULL) { return 1; }
	if (requested_path == NULL) { return 1; }
	if (resolved_path == NULL) { return 1; }
	if (requested_path[0] == '\0') { return 1; }
	if (requested_path[0] == '/') { 
		written = snprintf(candidate_path, sizeof(candidate_path), "%s", requested_path);
	} else {
		written = snprintf(candidate_path, sizeof(candidate_path), "%s/%s", workspace_root, requested_path);
	}
	if (written < 0 || (size_t)written >= sizeof(candidate_path)) { return 1; }
	if (realpath(workspace_root, resolved_workspace) == NULL) { return 1; }
	if (realpath(candidate_path, resolved_path) == NULL) { return 1; }
	if (!is_path_within_workspace(resolved_workspace, resolved_path)) { return 1; }
	return 0;
}

int resolve_workspace_write_parent(const char *workspace_root, const char *requested_path, char resolved_parent[PATH_MAX], char filename[NAME_MAX + 1]) {
	char resolved_workspace[PATH_MAX];
	char candidate_path[PATH_MAX];
	char parent_path[PATH_MAX];
	const char *last_separator = NULL;
	const char *filename_start = NULL;
	size_t parent_length = 0;
	size_t filename_length = 0;
	int written = 0;
	struct stat parent_info;
	if (workspace_root == NULL) { return 1; }
	if (requested_path == NULL) { return 1; }
	if (resolved_parent == NULL) { return 1; }
	if (filename == NULL) { return 1; }
	if (requested_path[0] == '\0') { return 1; }
	if (requested_path[0] == '/') {
		written = snprintf(candidate_path, sizeof(candidate_path), "%s", requested_path);
	} else {
		written = snprintf(candidate_path, sizeof(candidate_path), "%s/%s", workspace_root, requested_path);
	}
	if (written < 0 || (size_t) written >= sizeof(candidate_path)) { return 1; }
	last_separator = strrchr(candidate_path, '/');
	if (last_separator == NULL) { return 1; }
	filename_start = last_separator + 1;
	filename_length = strlen(filename_start);
	if (filename_length == 0 || filename_length > NAME_MAX) { return 1; }
	if (strcmp(filename_start, ".") == 0 || strcmp(filename_start, "..") == 0) {
		return 1;
	}
	memcpy(filename, filename_start, filename_length);
	filename[filename_length] = '\0';
	if (last_separator == candidate_path) {
		parent_path[0] = '/';
		parent_path[1] = '\0';
	} else {
		parent_length =  (size_t) (last_separator - candidate_path);
		if (parent_length >= sizeof(parent_path)) { return 1; }
		memcpy(parent_path, candidate_path, parent_length);
		parent_path[parent_length] = '\0';
	}
	if (realpath(workspace_root, resolved_workspace) == NULL) { return 1; }
	if (realpath(parent_path, resolved_parent) == NULL) { return 1; }
	if (!is_path_within_workspace(resolved_workspace, resolved_parent)) {
		return 1;
	}
	if (stat(resolved_parent, &parent_info) != 0) { return 1; }
	if (!S_ISDIR(parent_info.st_mode)) {
		return 1;
	}
	return 0;
}
