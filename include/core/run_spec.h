#ifndef CORE_RUN_SPEC_H
#define CORE_RUN_SPEC_H

struct run_spec {
	int bug_id;
	const char *project_id;
	const char *runs_root;
	const char *run_dir;
	const char *checkout_dir;
	const char *artifacts_dir;
};

int run_spec_validate(const struct run_spec *spec);

#endif
