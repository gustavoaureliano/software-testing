#include <cli/cli.h>
#include <cli/commands.h>
#include <stdio.h>
#include <string.h>

int print_general_help(const char *program);

int run_subcommand(int argc, char *argv[]) {
	if (argc <= 1) {
		printf("%s: Try '%s --help' for more information!\n", argv[0], argv[0]);
		return 0;
	}

	const char *cmd = argv[1];

	if (strcmp(cmd, "--help") == 0) {
		return print_general_help(argv[0]);
	}

	if (strcmp(cmd, "run") == 0) {
		cmd_run_bugs(argc - 1, argv + 1);
		return 0;
	}

	fprintf(stderr, "Unknown command: %s\n", cmd);
	return 1;
}

int print_general_help(const char *program) {
	printf(
		"Usage:\n"
		"	%s <command> [options]\n"
		"\nCommands:\n"
		"	run:	run bugs\n"
		"\nOptions:\n"
		"	--help:	Show help\n",
		program
	);
	return 0;
}
