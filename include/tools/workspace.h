#ifndef TOOLS_WORKSPACE_H
#define TOOLS_WORKSPACE_H

#include <limits.h>

int resolve_workspace_existing_path(const char *workspace_root, const char *requested_path, char resolved_path[PATH_MAX]);

int resolve_workspace_write_parent(const char *workspace_root, const char *requested_path, char resolved_parent[PATH_MAX], char filename[NAME_MAX + 1]);

#endif
