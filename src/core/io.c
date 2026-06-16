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

static int copy_from_fd(int fd, char *output, size_t output_capacity);

int run_program_capture(char *argv[], char *output, size_t output_capacity, size_t *output_size, int *exit_code) {
	// if (output == NULL) { return 1; }
	// if (output_capacity == 0) { return 1; }
	// if (output_size == NULL) { return 1; }
	if (exit_code == NULL) { return 1; }
	int fildes[2];
	int status = pipe(fildes);
	if (status == -1 ) {
		// i'm not really sure if it sets the errno
		perror("pipe error:");
		return 1;
	}
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		close(fildes[0]);
		close(fildes[1]);
		return 1;
	}
	if (pid == 0) {
		close(fildes[0]);
		dup2(fildes[1], STDOUT_FILENO);
		// dup2(fildes[1], STDERR_FILENO);
		close(fildes[1]);
		execvp(argv[0], argv);
		perror("execvp error:");
		_exit(127);
	}
	// if (output == NULL) { return 1; }
	// if (output_capacity == 0) { return 1; }
	// if (output_size == NULL) { return 1; }
	close(fildes[1]);
	if (output != NULL && output_capacity != 0 && output_size != NULL) {
		*output_size = copy_from_fd(fildes[0], output, output_capacity);
	}
	close(fildes[0]);
	if (waitpid(pid , &status, 0) < 0) {
		fprintf(stderr, "Failed to waitpid(%d)\n", pid);
		return 1;
	}
	if (WIFEXITED(status)) {
		*exit_code = WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		// It seems that 128 + WTERMSIG is a shell convention, not sure about that
		*exit_code = 128 + WTERMSIG(status);
	} else {
		*exit_code = -1;
	}
	return 0;
}

static int copy_from_fd(int fd, char *output, size_t output_capacity) {
	char buffer[1024];
	size_t used = 0;
	ssize_t n;
	while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
		size_t available;
		if (used >= output_capacity - 1) { break; }
		available = output_capacity - 1 - used;
		size_t to_copy = (size_t) n > available ? available : (size_t) n;
		memcpy(output + used, buffer, to_copy);
		used += to_copy;
		if (to_copy < (size_t) n) {
			break;
		}
	}
	output[used] = '\0';
	return used;
}
