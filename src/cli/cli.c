#include "tools/handlers.h"
#include "tools/registry.h"
#include <cli/cli.h>
#include <cli/commands.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <provider/llamacpp.h>
#include <orchestrator/orchestrator.h>
#include <tools/schema.h>
#include <unistd.h>

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

	if (strcmp(cmd, "chat") == 0) {
		struct llm_message messages[] = {
			{
				.role = "system",
				.content = "You are a tool-using Developer specialist assistant. Do not ask follow-up questions. If a suitable tools is available, you must call it.",
				.tool_call_id = NULL // not really necessary here
			},
			{
				.role = "user",
				.content = argc > 2 ? argv[2] : "Add a very funny joke at the end of the readme file.",
				.tool_call_id = NULL
			}
		};
		struct llm_message_list message_list = {
			.items = messages,
			.count = sizeof(messages)/sizeof(messages[0])
		};

		struct llm_tool_schema *read_file_parameters = llm_tool_schema_create_object();
		if (read_file_parameters == NULL) {
			fprintf(stderr, "Failed to create read_file parameters object!\n");
			return 1;
		}
		if (
				llm_tool_schema_add_string_property(read_file_parameters, "path", "Path to the file to read", true) != 0 ||
				llm_tool_schema_add_integer_property(read_file_parameters, "start_line", "Optional first line to read, starting at 1", false) != 0 ||
				llm_tool_schema_add_integer_property(read_file_parameters, "end_line", "Optional last line to read, inclusive", false) != 0
				) {
			fprintf(stderr, "Failed to add read_file schema properties!\n");
			llm_tool_schema_destroy(read_file_parameters);
			return 1;
		}

		struct llm_tool_schema *list_files_parameters = llm_tool_schema_create_object();
		if (list_files_parameters == NULL) {
			fprintf(stderr, "Failed to create list_files parameters object!\n");
			llm_tool_schema_destroy(read_file_parameters);
			return 1;
		}
		if (llm_tool_schema_add_string_property(list_files_parameters, "path", "Optional directory path to list", false) != 0) {
			fprintf(stderr, "Failed to add list_files schema properties!\n");
			llm_tool_schema_destroy(list_files_parameters);
			llm_tool_schema_destroy(read_file_parameters);
			return 1;
		}
		struct llm_tool_schema *grep_files_parameters = llm_tool_schema_create_object();
		if (grep_files_parameters == NULL) {
			fprintf(stderr, "Failed to create grep_files parameters object!\n");
			llm_tool_schema_destroy(list_files_parameters);
			llm_tool_schema_destroy(read_file_parameters);
			return 1;
		}
		if (llm_tool_schema_add_string_property(grep_files_parameters, "path", "Optional workspace files or directory to search. Defaults to the workspace root", false) != 0) {
			fprintf(stderr, "Failed to add grep_files schema properties!\n");
			llm_tool_schema_destroy(grep_files_parameters);
			llm_tool_schema_destroy(list_files_parameters);
			llm_tool_schema_destroy(read_file_parameters);
			return 1;
		}
		if (llm_tool_schema_add_string_property(grep_files_parameters, "pattern", "grep basic regular expression to search for.", true) != 0) {
			fprintf(stderr, "Failed to add grep_files schema properties!\n");
			llm_tool_schema_destroy(grep_files_parameters);
			llm_tool_schema_destroy(list_files_parameters);
			llm_tool_schema_destroy(read_file_parameters);
			return 1;
		}
		struct llm_tool_schema *edit_file_parameters = llm_tool_schema_create_object();
		if (edit_file_parameters == NULL) {
			fprintf(stderr, "Failed to create edit_file parameters object!\n");
			llm_tool_schema_destroy(grep_files_parameters);
			llm_tool_schema_destroy(list_files_parameters);
			llm_tool_schema_destroy(read_file_parameters);
			return 1;
		}
		if (
			llm_tool_schema_add_string_property(edit_file_parameters, "path", "Existing workspace file to edit (not a symlink).", true) != 0 ||
			llm_tool_schema_add_string_property(edit_file_parameters, "old_text", "Exact text to replace; must occur exactly once. Copy text without read_file line numbers.", true) != 0 ||
			llm_tool_schema_add_string_property(edit_file_parameters, "new_text", "Replacement text. May be empty to delete old_text.", true) != 0
		   ) {
			fprintf(stderr, "Failed to add edit_file schema properties!\n");
			llm_tool_schema_destroy(edit_file_parameters);
			llm_tool_schema_destroy(grep_files_parameters);
			llm_tool_schema_destroy(list_files_parameters);
			llm_tool_schema_destroy(read_file_parameters);
			return 1;
		}
		struct llm_tool_schema *create_file_parameters = llm_tool_schema_create_object();
		if (create_file_parameters == NULL) {
			fprintf(stderr, "Failed to create create_file parameters object!\n");
			llm_tool_schema_destroy(edit_file_parameters);
			llm_tool_schema_destroy(grep_files_parameters);
			llm_tool_schema_destroy(list_files_parameters);
			llm_tool_schema_destroy(read_file_parameters);
			return 1;
		}
		if (
			llm_tool_schema_add_string_property(create_file_parameters, "path", "Path to a new file inside an existing workspace directory. Must not already exist.", true) != 0 ||
			llm_tool_schema_add_string_property(create_file_parameters, "content", "Complete content of the new file (may be empty; at most 3500 bytes). If a trailing newline is requested, include \\n at the end of content. Use edit_file for existing files.", true) != 0
		   ) {
			fprintf(stderr, "Failed to add create_file schema properties!\n");
			llm_tool_schema_destroy(create_file_parameters);
			llm_tool_schema_destroy(edit_file_parameters);
			llm_tool_schema_destroy(grep_files_parameters);
			llm_tool_schema_destroy(list_files_parameters);
			llm_tool_schema_destroy(read_file_parameters);
			return 1;
		}
		struct llm_tool_definition tools[] = {
			{
				.name = "read_file",
				.description = "Read up to 200 lines from a file in the workspace",
				.parameters = read_file_parameters
			},
			{
				.name = "list_files",
				.description = "List entries in a workspace directory",
				.parameters = list_files_parameters
			},
			{
				.name = "grep_files",
				.description = "Search workspace files for matching lines. Returns file names and line numbers. Use a narrow path or pattern when possible",
				.parameters = grep_files_parameters
			},
			{
				.name = "edit_file",
				.description = "Edit one existing workspace file by replacing one unique, exact old_text with new_text. Read the file first; include context if the text occurs more than once.",
				.parameters = edit_file_parameters
			},
			{
				.name = "create_file",
				.description = "Create a new workspace file with the given content. Never overwrites existing files or symlinks; the parent directory must already exist.",
				.parameters = create_file_parameters
			}
		};
		struct tool_registry_entry entries[] = {
			{
			.definition = tools[0],
			.handler = read_file
			},
			{
			.definition = tools[1],
			.handler = list_files
			},
			{
			.definition = tools[2],
			.handler = grep_files
			},
			{
			.definition = tools[3],
			.handler = edit_file
			},
			{
			.definition = tools[4],
			.handler = create_file
			}
		};
		struct tool_registry registry = {
			.items = entries,
			.count = sizeof(entries)/sizeof(entries[0])
		};
		char *workspace_root = getcwd(NULL, 0); // should only work in GNU
		if (workspace_root == NULL) {
			fprintf(stderr, "Failed to get current working directory!\n");
			llm_tool_schema_destroy(create_file_parameters);
			llm_tool_schema_destroy(edit_file_parameters);
			llm_tool_schema_destroy(grep_files_parameters);
			llm_tool_schema_destroy(list_files_parameters);
			llm_tool_schema_destroy(read_file_parameters);
			return 1;
		}
		struct tool_context context = {
			.workspace_root = workspace_root
		};
		struct orchestrator_config config = {
			// .base_url = "http://100.72.37.73:8080",
			.base_url = "http://100.113.151.90:8080",
			.model = "qwen35-9b",
			.max_turns = 128,
			.initial_tool_choice = LLM_TOOL_CHOICE_REQUIRED
		};
		struct orchestrator_result result;
		int ret = orchestrator_run(
				config,
				message_list,
				registry,
				context,
				&result
				);
		if (ret != 0) {
			fprintf(stderr, "Orchestrator failed!\n");
			free(workspace_root);
			llm_tool_schema_destroy(create_file_parameters);
			llm_tool_schema_destroy(edit_file_parameters);
			llm_tool_schema_destroy(grep_files_parameters);
			llm_tool_schema_destroy(list_files_parameters);
			llm_tool_schema_destroy(read_file_parameters);
			return ret;
		}
		if (result.has_content) {
			printf("final content:\n%s\n", result.content);
		}
		printf("turns completed: %zu\n", result.turns_completed);
		if (result.stop_reason == ORCHESTRATOR_STOP_TURN_LIMIT) {
			printf("Stop reason: turn limit!\n");
		}
		free(workspace_root);
		llm_tool_schema_destroy(create_file_parameters);
		llm_tool_schema_destroy(edit_file_parameters);
		llm_tool_schema_destroy(grep_files_parameters);
		llm_tool_schema_destroy(list_files_parameters);
		llm_tool_schema_destroy(read_file_parameters);
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
