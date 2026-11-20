#include <core/io.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <cli/commands.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>

static size_t copy_from_fd(int fd, char *output, size_t output_capacity);

static int monotonic_milliseconds(int64_t *out);

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
	} else {
		copy_from_fd(fildes[0], NULL, 0);
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

static size_t copy_from_fd(int fd, char *output, size_t output_capacity) {
	char buffer[1024];
	size_t used = 0;
	ssize_t n;
	while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
		if (output == NULL || output_capacity == 0) { continue; }
		if (used >= output_capacity - 1) { continue; }
		size_t available;
		available = output_capacity - 1 - used;
		size_t to_copy = (size_t) n > available ? available : (size_t) n;
		memcpy(output + used, buffer, to_copy);
		used += to_copy;
	}
	if (output != NULL && output_capacity > 0) {
		output[used] = '\0';
	}
	return used;
}

int run_program_capture_combined(char *argv[], char *output, size_t output_capacity, size_t *output_size, int timeout_ms, int *exit_code) {
	int fildes[2];
	int status;
	pid_t pid;
	int64_t start_ms;
	int64_t deadline_ms;
	if (argv == NULL) { return 1; }
	if (argv[0] == NULL) { return 1; }
	if (output == NULL) { return 1; }
	if (output_capacity == 0 ) { return 1; }
	if (output_size == NULL) { return 1; }
	if (exit_code == NULL) { return 1; }
	if (timeout_ms <= 0) { return 1; }
	output[0] = '\0';
	*output_size = 0;
	*exit_code = -1;
	if (monotonic_milliseconds(&start_ms) != 0) {
		*output_size = (size_t)snprintf(output, output_capacity, "Failed to read monotonic clock.");
		if (*output_size >= output_capacity) {
			*output_size = output_capacity - 1;
		}
		return 1;
	}
	deadline_ms = start_ms + timeout_ms;
	if (pipe(fildes) == -1) {
		perror("pipe error:");
		return 1;
	}
	pid = fork();
	if (pid < 0) {
		perror("fork");
		close(fildes[0]);
		close(fildes[1]);
		return 1;
	}
	if (pid == 0) {
		close(fildes[0]);
		if (dup2(fildes[1], STDOUT_FILENO) == -1) { _exit(127); }
		if (dup2(fildes[1], STDERR_FILENO) == -1) { _exit(127); }
		close(fildes[1]);
		execvp(argv[0], argv);
		perror("execvp");
		_exit(127);
	}
	close(fildes[1]);
	struct pollfd watched = {
		.fd = fildes[0],
		.events = POLLIN // there is data to read
	};
	char buffer[1024];
	bool child_done = false;
	const char *failure = NULL;
	while (true) {
		if (!child_done) {
			pid_t waited = waitpid(pid, &status, WNOHANG); // WNOHANG: dont block waiting
			if (waited == pid) {
				child_done = true;
			} else if (waited == -1 && errno != EINTR) {
				failure = "Failed to collect child status.";
				goto combined_failure;
			}
		}
		if (child_done && watched.fd == -1) { break; }
		int64_t now_ms;
		if (monotonic_milliseconds(&now_ms) != 0) {
			failure = "Failed to read monotonic clock.";
			goto combined_failure;
		}
		int64_t remaining_ms = deadline_ms - now_ms;
		if (remaining_ms <= 0) {
			failure = "Command timed out.";
			goto combined_failure;
		}
		int wait_ms = remaining_ms > 20 ? 20 : (int)remaining_ms;
		// wait until something happens
		// or time elapsed
		int ready = poll(&watched, 1, wait_ms); // blocking
		if (ready == -1) {
			if (errno == EINTR) {
				continue;
			}
			failure = "Failed to poll command output.";
			goto combined_failure;
		}
		if (ready == 0) { continue; }
		// POLLNVAL: invalid polling request
		if (watched.revents & POLLNVAL) {
			failure = "Invalid command output descriptor.";
			goto combined_failure;
		}
		if (watched.revents & (POLLIN | POLLHUP | POLLERR)) {
			ssize_t n = read(watched.fd, buffer, sizeof(buffer));
			if (n == -1) {
				if (errno == EINTR) { continue; }
				failure = "Failed to read command output.";
				goto combined_failure;
			}
			if (n == 0) {
				close(watched.fd);
				watched.fd = -1;
				continue;
			}
			size_t available = output_capacity - 1 - *output_size;
			size_t to_copy = (size_t)n < available ? (size_t)n : available;
			memcpy(output + *output_size, buffer, to_copy);
			*output_size += to_copy;
			output[*output_size] = '\0';
		}
	}
	if (WIFEXITED(status)) {
		*exit_code = WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		*exit_code = 128 + WTERMSIG(status);
	}
	return 0;
combined_failure:
	if (watched.fd != -1) {
		close(watched.fd);
	}
	if (!child_done) {
		kill(pid, SIGKILL);
		pid_t waited;
		do {
			waited = waitpid(pid, &status, 0);
		} while (waited == -1 && errno == EINTR);
	}
	snprintf(output, output_capacity, "%s", failure);
	*output_size = strlen(output);
	return 1;
}

static int monotonic_milliseconds(int64_t *out) {
	struct timespec now;
	if (out == NULL) { return 1; }
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
		return 1;
	}
	*out = (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
	return 0;
}
