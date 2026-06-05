#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <cli/commands.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <core/defects4j.h>

static int select_project(struct project_list projects);
static int select_project_bug(struct bug_list bugs);
static int get_bug_index(struct bug_list bugs, int value);

int cmd_run_bugs(int argc, char* argv[]) {
	if (argc <= 1) {
		printf("%s: Try '%s %s --help' for more information!\n", (argv-1)[0], (argv-1)[0], argv[0]);
		return  1;
	}
	struct bug_list bug_list = {0};
	struct project_list project_list = {0};
	defects4j_list_projects(&project_list);
	int project_id_index = select_project(project_list);
	if (project_id_index < 0) {
		fprintf(stderr, "Error selecting project\n");
		return 1;
	}
	fprintf(stderr, "Selected project: %s\n", project_list.items[project_id_index]);
	defects4j_list_bugs(project_list.items[project_id_index], &bug_list);
	int bug_id_index =  select_project_bug(bug_list);
	if (bug_id_index < 0) {
		fprintf(stderr, "Error selecting bug\n");
		return 1;
	}
	fprintf(stderr, "Selected bug: %d\n", bug_list.items[bug_id_index]);
	defects4j_checkout_bug(
			project_list.items[project_id_index],
			bug_list.items[bug_id_index]);
	return  1;
}

static int select_project(struct project_list projects) {
	printf("Select project from list:\n");
	for (size_t i = 0; i < projects.count; i++) {
		printf("%zu -> %s\n", i, projects.items[i]);
	}
	printf(": ");
	char *line = NULL;
	size_t size = 0;
	getline(&line, &size, stdin);
	char *endptr;
	errno = 0;
	unsigned long project_index = strtoul(line, &endptr, 10);
	fprintf(stderr, "line: %s; num: %lu\n", line, project_index);
	free(line);
	if (endptr == line) { return -1; }
	if (errno != 0) { return -1; }
	return project_index;
}


static int select_project_bug(struct bug_list bugs) {
	printf("Select bug from:");
	int old_num = bugs.items[0];
	// this it not a great implementation but kind of works
	// it doesn't take into consideration when there is a
	// single bug surrounded by two gaps
	printf(" [%d-", old_num);
	for (size_t i = 1; i < bugs.count; i++) {
		if (old_num + 1 != bugs.items[i]) {
			printf("%d] [%d-", old_num, bugs.items[i]);
		}
		old_num = bugs.items[i];
	}
	printf("%d]\n", old_num);
	printf(": ");
	char *line = NULL;
	size_t size = 0;
	getline(&line, &size, stdin);
	char *endptr;
	errno = 0;
	unsigned long bug_id = strtoul(line, &endptr, 10);
	fprintf(stderr, "line: %s; num: %lu\n", line, bug_id);
	free(line);
	if (endptr == line) { return -1; }
	if (errno != 0) { return -1; }
	int bug_index = get_bug_index(bugs, bug_id);
	return bug_index;
}

static int get_bug_index(struct bug_list bugs, int value) {
	for (size_t i = 0; i < bugs.count; i++) {
		if (bugs.items[i] == value) { return i; }
	}
	return -1;
}
