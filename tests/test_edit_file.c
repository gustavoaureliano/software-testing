#include <tools/handlers.h>

#include <assert.h>
#include <cjson/cJSON.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_text(const char *path, const char *text) {
	FILE *file = fopen(path, "wb");
	assert(file != NULL);
	assert(fwrite(text, 1, strlen(text), file) == strlen(text));
	assert(fclose(file) == 0);
}

static void expect_text(const char *path, const char *expected) {
	FILE *file = fopen(path, "rb");
	char buffer[256];
	assert(file != NULL);
	size_t count = fread(buffer, 1, sizeof(buffer), file);
	assert(!ferror(file));
	assert(count == strlen(expected));
	assert(memcmp(buffer, expected, count) == 0);
	assert(fclose(file) == 0);
}

static void run_edit(const char *workspace, const char *path,
	const char *old_text, const char *new_text, bool error) {
	cJSON *arguments = cJSON_CreateObject();
	assert(arguments != NULL);
	assert(cJSON_AddStringToObject(arguments, "path", path) != NULL);
	assert(cJSON_AddStringToObject(arguments, "old_text", old_text) != NULL);
	assert(cJSON_AddStringToObject(arguments, "new_text", new_text) != NULL);
	char *json = cJSON_PrintUnformatted(arguments);
	assert(json != NULL);
	struct tool_context context = { .workspace_root = workspace };
	struct tool_result result;
	int status = edit_file(json, context, &result);
	if (status != 0 || result.is_error != error) {
		fprintf(stderr, "Unexpected edit result for %s: %s\n", path, result.content);
	}
	assert(status == 0);
	assert(result.is_error == error);
	assert(result.content[0] != '\0');
	cJSON_free(json);
	cJSON_Delete(arguments);
}

int main(void) {
	char root[] = "/tmp/opencode/test-edit-XXXXXX";
	assert(mkdtemp(root) != NULL);
	char workspace[256], target[256], outside[256], link[256];
	assert(snprintf(workspace, sizeof(workspace), "%s/workspace", root) < (int)sizeof(workspace));
	assert(mkdir(workspace, 0700) == 0);
	assert(snprintf(target, sizeof(target), "%s/README.txt", workspace) < (int)sizeof(target));
	assert(snprintf(outside, sizeof(outside), "%s/outside.txt", root) < (int)sizeof(outside));
	assert(snprintf(link, sizeof(link), "%s/link.txt", workspace) < (int)sizeof(link));
	write_text(outside, "outside\n");
	write_text(target, "Hello\n-The CLI Team\n");
	assert(chmod(target, 0640) == 0);

	run_edit(workspace, "README.txt", "-The CLI Team\n",
		"-The CLI Team\n\nA joke.\n", false);
	expect_text(target, "Hello\n-The CLI Team\n\nA joke.\n");
	struct stat info;
	assert(stat(target, &info) == 0);
	assert((info.st_mode & 0777) == 0640);

	write_text(target, "Hello\n-The CLI Team\n");
	run_edit(workspace, "README.txt", "Hello", "Goodbye", false);
	expect_text(target, "Goodbye\n-The CLI Team\n");

	write_text(target, "Hello\n-The CLI Team\n");
	run_edit(workspace, "README.txt", "Hello\n", "", false);
	expect_text(target, "-The CLI Team\n");

	write_text(target, "last line");
	run_edit(workspace, "README.txt", "last line", "new last line", false);
	expect_text(target, "new last line");

	write_text(target, "aaaa");
	run_edit(workspace, "README.txt", "aa", "b", true);
	expect_text(target, "aaaa");
	run_edit(workspace, "README.txt", "missing", "b", true);
	expect_text(target, "aaaa");
	run_edit(workspace, "README.txt", "aaaa", "aaaa", true);
	expect_text(target, "aaaa");
	run_edit(workspace, "README.txt", "", "new", true);
	expect_text(target, "aaaa");

	assert(symlink("README.txt", link) == 0);
	run_edit(workspace, "link.txt", "aaaa", "b", true);
	expect_text(target, "aaaa");
	assert(unlink(link) == 0);
	assert(symlink(outside, link) == 0);
	run_edit(workspace, "link.txt", "outside", "changed", true);
	expect_text(outside, "outside\n");
	assert(unlink(link) == 0);
	run_edit(workspace, "../outside.txt", "outside", "changed", true);
	expect_text(outside, "outside\n");

	FILE *big = fopen(target, "wb");
	assert(big != NULL);
	for (int i = 0; i < 1025; i++) {
		char block[1024];
		memset(block, 'a', sizeof(block));
		assert(fwrite(block, 1, sizeof(block), big) == sizeof(block));
	}
	assert(fclose(big) == 0);
	run_edit(workspace, "README.txt", "a", "b", true);
	assert(stat(target, &info) == 0);
	assert(info.st_size == 1025 * 1024);

	assert(unlink(target) == 0);
	assert(unlink(outside) == 0);
	assert(rmdir(workspace) == 0);
	assert(rmdir(root) == 0);
	puts("All edit_file tests passed.");
	return 0;
}
