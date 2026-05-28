#include <inttypes.h>
#include <stdio.h>
#include <cli/commands.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <core/defects4j.h>

int cmd_run_bugs(int argc, char* argv[]) {
	if (argc <= 1) {
		printf("%s: Try '%s %s --help' for more information!\n", (argv-1)[0], (argv-1)[0], argv[0]);
		return  1;
	}
	char *project_id = "Closure";
	struct bug_list bug_list = {0};
	struct string_list string_list = {0};
	printf("size of bug_list: %zu\n", sizeof(bug_list));
	printf("size of struct bug_list: %zu\n", sizeof(struct bug_list));
	printf("size of string_list: %zu\n", sizeof(string_list));
	printf("size of struct string_list: %zu\n", sizeof(struct string_list));
	printf("bug_list count: %zu\n", bug_list.count);
	printf("string_list count: %zu\n", string_list.count);
	defects4j_list_bugs(project_id, &bug_list);
	defects4j_list_projects(&string_list);
	return  1;
}

