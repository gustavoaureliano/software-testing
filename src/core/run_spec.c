#include <stdio.h>
#include <stdint.h>
#include <core/run_spec.h>

int run_spec_validate(const struct run_spec *spec) {
	printf("pid: %s; bid: %d", spec->project_id, spec->bug_id);
	const struct run_spec new_spec = {
		.bug_id = 32,
		.project_id = "hello"
	};
	if (new_spec.bug_id == spec->bug_id) {
		printf("Hello there");
		return 0;
	}
	return 1;
}
