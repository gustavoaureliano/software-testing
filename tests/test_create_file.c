#include <tools/handlers.h>

#include <assert.h>
#include <cjson/cJSON.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void check_file(const char *path, const char *expected) {
	FILE *file = fopen(path, "rb");
	char buffer[4096];
	assert(file != NULL);
	size_t count = fread(buffer, 1, sizeof(buffer), file);
	assert(!ferror(file));
	assert(count == strlen(expected));
	assert(memcmp(buffer, expected, count) == 0);
	assert(fclose(file) == 0);
}

static void run_case(const char *workspace, const char *path, const char *content, bool error) {
	cJSON *arguments = cJSON_CreateObject();
	assert(arguments != NULL);
	assert(cJSON_AddStringToObject(arguments, "path", path) != NULL);
	assert(cJSON_AddStringToObject(arguments, "content", content) != NULL);
	char *json = cJSON_PrintUnformatted(arguments);
	assert(json != NULL);
	struct tool_context context = { .workspace_root = workspace };
	struct tool_result result;
	int status = create_file(json, context, &result);
	if (status != 0 || result.is_error != error) {
		fprintf(stderr, "Unexpected result for %s: %s\n", path, result.content);
	}
	assert(status == 0);
	assert(result.is_error == error);
	assert(result.content[0] != '\0');
	cJSON_free(json);
	cJSON_Delete(arguments);
}

int main(void) {
	char root[] = "/tmp/opencode/test-create-XXXXXX";
	assert(mkdtemp(root) != NULL);
	char workspace[256], first[256], empty[256], outside[256], link[256], dangling[256];
	assert(snprintf(workspace, sizeof(workspace), "%s/workspace", root) < (int)sizeof(workspace));
	assert(mkdir(workspace, 0700) == 0);
	assert(snprintf(first, sizeof(first), "%s/first.txt", workspace) < (int)sizeof(first));
	assert(snprintf(empty, sizeof(empty), "%s/empty.txt", workspace) < (int)sizeof(empty));
	assert(snprintf(outside, sizeof(outside), "%s/outside.txt", root) < (int)sizeof(outside));
	assert(snprintf(link, sizeof(link), "%s/link.txt", workspace) < (int)sizeof(link));
	assert(snprintf(dangling, sizeof(dangling), "%s/dangling.txt", workspace) < (int)sizeof(dangling));

	run_case(workspace, "first.txt", "hello\n", false);
	check_file(first, "hello\n");
	run_case(workspace, "empty.txt", "", false);
	check_file(empty, "");
	run_case(workspace, "first.txt", "overwritten", true);
	check_file(first, "hello\n");

	assert(symlink(first, link) == 0);
	run_case(workspace, "link.txt", "overwritten", true);
	check_file(first, "hello\n");
	assert(symlink("missing.txt", dangling) == 0);
	run_case(workspace, "dangling.txt", "created", true);
	struct stat info;
	assert(lstat(dangling, &info) == 0 && S_ISLNK(info.st_mode));

	run_case(workspace, "../outside.txt", "outside", true);
	run_case(workspace, outside, "outside", true);
	assert(access(outside, F_OK) != 0);
	run_case(workspace, "new/subdir/file.txt", "missing parent", true);
	run_case(workspace, ".", "invalid target", true);

	char oversized[3502];
	memset(oversized, 'x', sizeof(oversized) - 1);
	oversized[sizeof(oversized) - 1] = '\0';
	run_case(workspace, "oversized.txt", oversized, true);
	char oversized_path[256];
	assert(snprintf(oversized_path, sizeof(oversized_path), "%s/oversized.txt", workspace) < (int)sizeof(oversized_path));
	assert(access(oversized_path, F_OK) != 0);

	struct tool_context context = { .workspace_root = workspace };
	struct tool_result result;
	assert(create_file("{broken", context, &result) == 0 && result.is_error);
	assert(create_file("{}", context, &result) == 0 && result.is_error);

	assert(unlink(dangling) == 0);
	assert(unlink(link) == 0);
	assert(unlink(empty) == 0);
	assert(unlink(first) == 0);
	assert(rmdir(workspace) == 0);
	assert(rmdir(root) == 0);
	puts("All create_file tests passed.");
	return 0;
}
