#include <stdio.h>
#include <cli/commands.h>

int cmd_run_bugs(int argc, char* argv[]) {
	if (argc <= 1) { 
		printf("%s: Try '%s %s --help' for more information!\n", (argv-1)[0], (argv-1)[0], argv[0]);
		return  1;
	}
	printf("Running bug!\n");
	return  1;
}

