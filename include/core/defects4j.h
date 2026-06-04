#ifndef DEFECTS4J_H
#define DEFECTS4J_H

#include <stddef.h>

#define MAX_PROJECTS 128
#define MAX_PROJECT_ID_LEN 128
#define MAX_BUGS 1024
#define MAX_BUG_ID_LEN 1024
#define MAX_CMD_OUTPUT 10240

struct project_list {
	char items[MAX_PROJECTS][MAX_PROJECT_ID_LEN];
	size_t count;
};

struct bug_list {
	int items[MAX_BUGS];
	size_t count;
};

int defects4j_list_projects(struct project_list *out);
int defects4j_list_bugs(char *project_id, struct bug_list *out);

#endif
