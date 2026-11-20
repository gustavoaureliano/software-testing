#include <tools/handlers.h>
#include <cjson/cJSON.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void make_path(
	char *out, size_t capacity,
	const char *directory, const char *name
) {
	int length = snprintf(out, capacity, "%s/%s", directory, name);
	assert(length >= 0 && (size_t)length < capacity);
}

static void write_original(const char *path) {
	FILE *file = fopen(path, "wb");
	assert(file != NULL);
	assert(fputs("return 0;\n", file) >= 0);
	assert(fclose(file) == 0);
}

static void assert_file_contents(const char *path, const char *expected) {
	char buffer[64];
	FILE *file = fopen(path, "rb");
	assert(file != NULL);

	size_t count = fread(buffer, 1, sizeof(buffer), file);
	assert(!ferror(file));
	assert(count == strlen(expected));
	assert(memcmp(buffer, expected, count) == 0);
	assert(fclose(file) == 0);
}

static void run_case(
	const char *name,
	const char *workspace,
	const char *requested_path,
	const char *patch_text,
	bool expect_error,
	const char *expected_contents,
	const char *original_path,
	const char *outside_path
) {
	write_original(original_path);
	cJSON *arguments = cJSON_CreateObject();
	assert(arguments != NULL);
	assert(cJSON_AddStringToObject(arguments, "path", requested_path) != NULL);
	assert(cJSON_AddStringToObject(arguments, "patch", patch_text) != NULL);

	char *json = cJSON_PrintUnformatted(arguments);
	assert(json != NULL);

	struct tool_context context = {
		.workspace_root = workspace
	};
	struct tool_result result;

	int status = apply_patch(json, context, &result);

	if (status != 0 || result.is_error != expect_error) {
		fprintf(stderr, "FAIL: %s\n%s\n", name, result.content);
	}

	assert(status == 0);
	assert(result.is_error == expect_error);
	assert(result.content[0] != '\0');

	assert_file_contents(original_path, expected_contents);
	assert_file_contents(outside_path, "return 0;\n");

	cJSON_free(json);
	cJSON_Delete(arguments);

	printf("PASS: %s\n", name);
}

int main(void) {
	char directory[] = "/tmp/opencode/test-patch-XXXXXX";
	assert(mkdtemp(directory) != NULL);

	char workspace[256];
	char original[256];
	char outside[256];
	char symlink_path[256];

	make_path(workspace, sizeof(workspace), directory, "workspace");
	assert(mkdir(workspace, 0700) == 0);

	make_path(original, sizeof(original), workspace, "example.c");
	make_path(outside, sizeof(outside), directory, "outside.c");
	make_path(symlink_path, sizeof(symlink_path), workspace, "link.c");

	write_original(original);
	write_original(outside);
	assert(symlink("example.c", symlink_path) == 0);

	const char *matching_patch =
		"--- original\n"
		"+++ modified\n"
		"@@ -1 +1 @@\n"
		"-return 0;\n"
		"+return 1;\n";

	const char *nonmatching_patch =
		"--- original\n"
		"+++ modified\n"
		"@@ -1 +1 @@\n"
		"-return 99;\n"
		"+return 1;\n";

	run_case(
		"matching patch",
		workspace, "example.c", matching_patch, false,
		"return 1;\n",
		original, outside
	);

	run_case(
		"nonmatching patch",
		workspace, "example.c", nonmatching_patch, true,
		"return 0;\n",
		original, outside
	);

	run_case(
		"invalid headers",
		workspace, "example.c", "not a unified diff\n", true,
		"return 0;\n",
		original, outside
	);

	run_case(
		"outside workspace",
		workspace, "../outside.c", matching_patch, true,
		"return 0;\n",
		original, outside
	);

	run_case(
		"symlink target",
		workspace, "link.c", matching_patch, true,
		"return 0;\n",
		original, outside
	);

	assert(unlink(symlink_path) == 0);
	assert(unlink(original) == 0);
	assert(unlink(outside) == 0);
	assert(rmdir(workspace) == 0);
	assert(rmdir(directory) == 0);

	puts("All patch application tests passed.");
	return 0;
}
