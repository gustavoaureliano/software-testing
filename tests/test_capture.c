#include <core/io.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_combined_output(void) {
	char output[128];
	size_t size;
	int exit_code;

	char *args[] = {
		"/bin/sh", "-c",
		"printf 'stdout message\\n'; printf 'stderr message\\n' >&2",
		NULL
	};

	int result = run_program_capture_combined(
		args, output, sizeof(output), &size, 2000, &exit_code
	);

	assert(result == 0);
	assert(exit_code == 0);
	assert(strstr(output, "stdout message") != NULL);
	assert(strstr(output, "stderr message") != NULL);
	assert(size == strlen(output));

	puts("PASS: stdout and stderr captured");
}

static void test_large_output(void) {
	char output[64];
	size_t size;
	int exit_code;

	char *args[] = {
		"/bin/sh", "-c",
		"i=0; while [ \"$i\" -lt 10000 ]; do "
		"printf '0123456789abcdef\\n'; i=$((i + 1)); done",
		NULL
	};

	int result = run_program_capture_combined(
		args, output, sizeof(output), &size, 5000, &exit_code
	);

	assert(result == 0);
	assert(exit_code == 0);
	assert(size == sizeof(output) - 1);
	assert(output[size] == '\0');

	puts("PASS: large output drained with bounded capture");
}

static void test_timeout(int close_output) {
	char output[128];
	size_t size;
	int exit_code;

	char *args[] = {
		"/bin/sh", "-c",
		close_output
			? "exec 1>&- 2>&-; exec sleep 5"
			: "exec sleep 5",
		NULL
	};

	int result = run_program_capture_combined(
		args, output, sizeof(output), &size, 100, &exit_code
	);

	assert(result != 0);
	assert(exit_code == -1);
	assert(strcmp(output, "Command timed out.") == 0);
	assert(size == strlen(output));

	puts(close_output
		? "PASS: timeout after stdout/stderr close"
		: "PASS: timeout with silent child");
}

int main(void) {
	test_combined_output();
	test_large_output();
	test_timeout(0);
	test_timeout(1);

	puts("All capture tests passed.");
	return 0;
}
