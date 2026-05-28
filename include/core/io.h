#ifndef CORE_IO_H
#define CORE_IO_H

#include <stddef.h>

int run_program_capture(char *argv[], char *output, size_t output_capacity, size_t *output_size, int *exit_code);

#endif
