#include <core/defects4j.h>
#include <core/io.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int fill_bugs_list(struct bug_list *bug_list, const char *text);
static int fill_projects_list(struct project_list *string_list, const char *text);

// i'm not really checking if bug_id or project_id are valid
// i'm assuming the values provided are valid and correct
int defects4j_checkout_bug(char *project_id, int bug_id) {
	if (project_id == NULL) { return 1; }
	if (bug_id < 0) { return 1; }
	char bug_id_buff[MAX_BUG_ID_LEN];
	snprintf(bug_id_buff, MAX_BUG_ID_LEN, "%db", bug_id);
	// checkout_dir = ./runs/Closure_1b
	size_t checkout_dir_size = (strlen(DEFAULT_CHECKOUT_DIR) + strlen(project_id) + 1 + strlen(bug_id_buff) + 1) * sizeof(char);
	char *checkout_dir = (char *) malloc(checkout_dir_size);
	snprintf(checkout_dir, checkout_dir_size, "%s%s_%s", DEFAULT_CHECKOUT_DIR, project_id, bug_id_buff);
	char *args[] = {
		"defects4j",
		"checkout",
		"-p",
		project_id,
		"-v",
		bug_id_buff,
		"-w",
		checkout_dir,
		(char *) NULL
	};
	int exit_code = -1;
	int status = run_program_capture(args, NULL, 0, NULL, &exit_code);
	free(checkout_dir);
	if (status) {
		fprintf(stderr, "Error executing checkout\n");
		return 1;
	}
	if (exit_code) {
		fprintf(stderr, "Error executing checkout\n");
		return 1;
	}
	return 0;
}

int defects4j_list_projects(struct project_list *out) {
	char *args[] = {
		"defects4j",
		"pids",
		(char *) NULL
	};

	char output[MAX_CMD_OUTPUT];
	size_t output_size = -1;
	int exit_code = -1;
	if (run_program_capture(args, output, MAX_CMD_OUTPUT, &output_size, &exit_code)) {
		fprintf(stderr, "Error projects list\n");
		return 1;
	}
	if (exit_code) {
		fprintf(stderr, "Error projects list\n");
		return 1;
	}
	if (fill_projects_list(out, output)) {
		fprintf(stderr, "Error parsing project list\n");
		return 1;
	}
	return 0;
}

int defects4j_list_bugs(char *project_id, struct bug_list *out) {
	printf("project: %s\n", project_id);
	char *args[] = {
		"defects4j",
		"bids",
		"-p",
		project_id,
		(char *) NULL
	};
	char output[MAX_CMD_OUTPUT];
	size_t output_size = -1;
	int exit_code = -1;
	if (run_program_capture(args, output, MAX_CMD_OUTPUT, &output_size, &exit_code)) {
		fprintf(stderr, "Error capturing bugs list\n");
		return 1;
	}
	if (exit_code) {
		fprintf(stderr, "Error capturing bugs list\n");
		return 1;
	}
	if (fill_bugs_list(out, output)) {
		fprintf(stderr, "Error parsing bugs list\n");
		return 1;
	};
	return 0;
}

static int fill_bugs_list(struct bug_list *bug_list, const char *text) {
	const char *p = text;
	while (*p) {
		if (bug_list->count >= MAX_BUGS) {
			return 1;
		}

		const char *end = strchr(p, '\n');
		size_t len = end ? (size_t) (end - p) : strlen(p);

		// takes Windows style into consideration, probably not needed at all
		if (len > 0 && p[len - 1] == '\r') {
			len--;
		}

		if (len == 0) {
			if (!end) { break; }
			p = end + 1;
			continue;
		}

		errno = 0;
		char buffer[MAX_BUG_ID_LEN];
		size_t copy_len = len >= MAX_BUG_ID_LEN ? MAX_BUG_ID_LEN - 1 : len;
		memcpy(buffer, p, copy_len);
		buffer[copy_len] = '\0';
		char *endptr;
		unsigned long bug_id = strtoul(buffer, &endptr, 10);
		if (endptr == buffer) {
			fprintf(stderr, "Error parsing bug_id number (%s): no digits found\n", buffer);
			p = end + 1;
			continue;
		} else if (endptr != &buffer[copy_len]) {
			fprintf(stderr, "Error parsing bug_id number (%s): trailing characters found\n", buffer);
			p = end + 1;
			continue;
		}
		if (errno != 0) {
			fprintf(stderr, "Error parsing bug_id number (%s): %s\n", buffer, strerror(errno));
			p = end + 1;
			continue;
		}
		if (bug_id > INT_MAX) {
			p = end + 1;
			continue;
		}
		bug_list->items[bug_list->count] = (int) bug_id;
		bug_list->count++;
		if (!end) { break; }
		p = end + 1;
	}
	return 0;
}

static int fill_projects_list(struct project_list *string_list, const char *text) {
	const char *p = text;
	while (*p) {
		if (string_list->count >= MAX_PROJECTS) {
			return  1;
		}
		const char *end = strchr(p, '\n');
		size_t len = end ? (size_t) (end - p) : strlen(p);

		// takes Windows style into consideration, probably not needed at all
		if (len > 0 && p[len - 1] == '\r') {
			len--;
		}

		if (len == 0) {
			if (!end) { break; }
			p = end + 1;
			continue;
		}

		size_t copy_len = len >= MAX_PROJECT_ID_LEN ? MAX_PROJECT_ID_LEN - 1: len;
		memcpy(string_list->items[string_list->count], p, copy_len);
		string_list->items[string_list->count][copy_len] = '\0';
		string_list->count++;
		if (!end) { break; }
		p = end + 1;
	}
	return 0;
}
