#include <core/io.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <cli/commands.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int run_program_capture(char *argv[], char *output, size_t output_capacity, size_t *output_size, int *exit_code) {
	if (output == NULL) { return 1; }
	if (output_capacity == 0) { return 1; }
	if (output_size == NULL) { return 1; }
	if (exit_code == NULL) { return 1; }
	int fildes[2];
	int status = pipe(fildes);
	if (status == -1 ) {
		// i'm not really sure if it sets the errno
		perror("pipe error:");
		return 1;
	}

	pid_t pid = fork();
	switch (pid) {
		case -1:
			perror("fork");
			close(fildes[0]);
			close(fildes[1]);
			return 1;
		case 0: // child
			close(fildes[0]);
			dup2(fildes[1], STDOUT_FILENO);
			// dup2(fildes[1], STDERR_FILENO);
			close(fildes[1]);
			execvp(argv[0], argv);
			perror("execlp error:");
			_exit(127);
			break;
		default: // parent
			close(fildes[1]);
			char buffer[1024];
			size_t used = 0;
			ssize_t n;
			while ((n = read(fildes[0], buffer, sizeof(buffer))) > 0) {
				size_t available;
				if (used >= output_capacity - 1) { break; }
				available = output_capacity - 1 - used;
				size_t to_copy = (size_t) n;
				if (to_copy > available) {
					to_copy = available;
				}
				memcpy(output + used, buffer, to_copy);
				used += to_copy;
				if (to_copy < (size_t) n) {
					break;
				}
			}
			output[used] = '\0';
			*output_size = used;
			close(fildes[0]);
			if (waitpid(pid , &status, 0) < 0) {
				fprintf(stderr, "Failed to waitpid(%d)\n", pid);
				return 1;
			}
			if (n < 0) {
				fprintf(stderr, "Error reading, total bytes read so far: %zu\n", used);
				return 1;;
			}
			if (WIFEXITED(status)) {
				printf("exit code: %d\n", WEXITSTATUS(status));
				*exit_code = WEXITSTATUS(status);
			} else if (WIFSIGNALED(status)) {
				printf("killed by signal: %d\n", WTERMSIG(status));
				// It seems that 128 + WTERMSIG is a shell convention, not sure about that
				*exit_code = 128 + WTERMSIG(status);
			} else {
				*exit_code = -1;
			}
			return 0;
	}
	return 1;
}
