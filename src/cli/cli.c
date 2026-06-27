#include <cli/cli.h>
#include <cli/commands.h>
#include <stdio.h>
#include <string.h>
#include <provider/llamacpp.h>

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
				.content = "You are a tool-using assistant. Do not ask follow-up questions. If a suitable tools is available, you must call it."
			},
			{
				.role = "user",
				.content = "Read the file README.md by calling the available tool now. Do not answer in natural language."
			}
		};
		char raw_response[102400];
		struct llm_tool_definition tools[] = {
			{
				.name = "read_file",
				.description = "Read a file from the workspace",
				.parameters_json =
					"{"
						"\"type\":\"object\","
						"\"properties\":{"
							"\"path\":{"
								"\"type\":\"string\","
								"\"description\":\"Path to the file to read\""
							"}"
						"},"
						"\"required\":[\"path\"]"
					"}"
			}
		};
		struct llm_response llm_response;
		return llm_chat("http://100.72.37.73:8080", "qwen35-9b", messages, sizeof(messages) / sizeof(messages[0]), tools, sizeof(tools)/sizeof(tools[0]), LLM_TOOL_CHOICE_REQUIRED, raw_response, sizeof(raw_response), &llm_response);
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
