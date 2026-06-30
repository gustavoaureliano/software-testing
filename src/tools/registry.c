#include <stddef.h>
#include <string.h>
#include <tools/registry.h>

const struct tool_registry_entry *tool_registry_find(struct tool_registry registry, const char *name) {
	if (name == NULL) { return NULL; }
	if (strlen(name) == 0) { return NULL; }
	if (registry.items == NULL) { return NULL; }
	if (registry.count == 0) { return NULL; }
	for (size_t i = 0; i < registry.count; i++) {
		const char *tool_name = registry.items[i].definition.name;
		if (strcmp(tool_name, name) == 0) {
			return &registry.items[i];
		}
	}
	return NULL;
}

