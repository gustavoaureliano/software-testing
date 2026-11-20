
#include <provider/llamacpp.h>
#include <tools/registry.h>
#include <orchestrator/orchestrator.h>
#include <tools/dispatch.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ORCHESTRATOR_MAX_TURNS 128
#define ORCHESTRATOR_MAX_INITIAL_MESSAGES 16
#define ORCHESTRATOR_MAX_TOOL_CALLS_PER_TURN LLM_MAX_TOOL_CALLS
#define ORCHESTRATOR_MAX_MESSAGES \
	(ORCHESTRATOR_MAX_INITIAL_MESSAGES + ORCHESTRATOR_MAX_TURNS * (2 + ORCHESTRATOR_MAX_TOOL_CALLS_PER_TURN) + 1)
#define ORCHESTRATOR_TOOL_RESULT_CAPACITY 8192
#define ORCHESTRATOR_RAW_RESPONSE_CAPACITY 102400

static const char tool_call_limit_notice[] = 
	"The previous assistant response requested more tool calls than the " 
	"orchestrator can execute in one turn. "
	"Only a bounded subsed of the requested tool calls was executed. "
	"Reissue any remaining calls that are still needed.";

static const char turn_limit_notice[] =
	"The tool-turn budget is exhausted. Do not request further tools. "
	"Provide a concise final response describing completed work, "
	"remaining work, and relevant tool errors. "
	"Do not claim success unless it was verified.";

struct orchestrator_conversation {
	struct llm_message messages[ORCHESTRATOR_MAX_MESSAGES];
	struct llm_tool_call assistant_tool_calls [ORCHESTRATOR_MAX_TURNS][ORCHESTRATOR_MAX_TOOL_CALLS_PER_TURN];
	char tool_result_contents [ORCHESTRATOR_MAX_TURNS]
							  [ORCHESTRATOR_MAX_TOOL_CALLS_PER_TURN]
							  [ORCHESTRATOR_TOOL_RESULT_CAPACITY];
	size_t message_count;
};

static int append_initial_messages(struct orchestrator_conversation *conversation, struct llm_message_list initial_messages);
static int append_assistant_tool_calls(struct orchestrator_conversation *conversation, size_t turn_index, const struct llm_response *response);
static int append_tool_result(struct orchestrator_conversation *conversation, size_t turn_index, size_t tool_call_index, const struct tool_result *result);
static int append_notice(struct orchestrator_conversation *conversation, const char *role, const char *content);

int orchestrator_run( struct orchestrator_config config, struct llm_message_list initial_messages, struct tool_registry registry, struct tool_context context, struct orchestrator_result *out) {
	int ret = 1;
	struct orchestrator_conversation *conversation = NULL;
	char raw_response[ORCHESTRATOR_RAW_RESPONSE_CAPACITY];
	if (out == NULL) { return 1; }
	memset(out, 0, sizeof(*out));
	out->stop_reason = ORCHESTRATOR_STOP_INTERNAL_ERROR;
	if (config.base_url == NULL) { goto cleanup; }
	if (config.model == NULL) { goto cleanup; }
	if (config.max_turns == 0) { goto cleanup; }
	if (config.max_turns > ORCHESTRATOR_MAX_TURNS) { goto cleanup; }
	switch (config.initial_tool_choice) {
		case LLM_TOOL_CHOICE_NONE:
		case LLM_TOOL_CHOICE_AUTO:
		case LLM_TOOL_CHOICE_REQUIRED:
			break;
		default:
			goto cleanup;
	}
	conversation = calloc(1, sizeof(*conversation));
	if (conversation == NULL) { goto cleanup; }
	if (append_initial_messages(conversation, initial_messages) != 0) { goto cleanup; }
	for (size_t turn = 0; turn < config.max_turns; turn++) {
		struct llm_message_list messages = {
			.items = conversation->messages,
			.count = conversation->message_count
		};
		struct llm_response response;
		enum llm_tool_choice_mode tool_choice = turn == 0 ? config.initial_tool_choice : LLM_TOOL_CHOICE_AUTO;
		if (llm_chat(
			config.base_url,
			config.model,
			messages,
			registry,
			tool_choice,
			raw_response,
			sizeof(raw_response),
			&response) != 0
		   ) {
			out->stop_reason = ORCHESTRATOR_STOP_PROVIDER_ERROR;
			goto cleanup;
		}
		out->turns_completed++;
		if (response.tool_call_count == 0) {
			if (!response.has_content) { goto cleanup; }
			memcpy(out->content, response.content, sizeof(out->content));
			out->content[sizeof(out->content) - 1] = '\0';
			out->has_content = true;
			out->stop_reason = ORCHESTRATOR_STOP_COMPLETED;
			ret = 0;
			goto cleanup;
		}
		if (append_assistant_tool_calls(conversation, turn, &response) != 0) { goto cleanup; }
		for (size_t i = 0; i < response.tool_call_count; i++) {
			struct tool_result tool_result = {0};
			if (dispatch_tool_call(registry, context, &conversation->assistant_tool_calls[turn][i], &tool_result) != 0) {
				tool_result_set_error(&tool_result, "Internal error while dispatching tool call.");
			}
			if (append_tool_result(conversation, turn, i, &tool_result) != 0) { goto cleanup; }
		}
		if (response.tool_calls_truncated) {
			if (append_notice(conversation, "user", tool_call_limit_notice) != 0) { goto cleanup; }
		}
	}
	if (append_notice(conversation, "user", turn_limit_notice) != 0) { goto cleanup; }
	struct llm_message_list messages = {
		.items = conversation->messages,
		.count = conversation->message_count
	};
	struct llm_response response;
	if (llm_chat(
		config.base_url,
		config.model,
		messages,
		registry,
		LLM_TOOL_CHOICE_NONE,
		raw_response,
		sizeof(raw_response),
		&response) != 0
	   ) {
		out->stop_reason = ORCHESTRATOR_STOP_PROVIDER_ERROR;
		goto cleanup;
	}
	out->turns_completed++;
	if (!response.has_content || response.tool_call_count != 0) {
		out->stop_reason = ORCHESTRATOR_STOP_PROVIDER_ERROR;
		goto cleanup;
	}
	memcpy(out->content, response.content, sizeof(out->content));
	out->content[sizeof(out->content) - 1] = '\0';
	out->has_content = true;
	out->stop_reason = ORCHESTRATOR_STOP_TURN_LIMIT;
	ret = 0;
cleanup:
	free(conversation);
	return ret;
}

static int append_notice(struct orchestrator_conversation *conversation, const char *role, const char *content) {
	if (conversation == NULL) { return 1; }
	if (content == NULL) { return 1; }
	if (conversation->message_count >= ORCHESTRATOR_MAX_MESSAGES) { return 1; }
	conversation->messages[conversation->message_count] = (struct llm_message) {
		.role = role,
		.content = content,
		.tool_call_id = NULL,
		.tool_calls = NULL,
		.tool_call_count = 0
	};
	conversation->message_count++;
	return 0;
}

static int append_tool_result(struct orchestrator_conversation *conversation, size_t turn_index, size_t tool_call_index, const struct tool_result *result) {
	char *stored_content = NULL;
	if (conversation == NULL) { return 1; }
	if (result == NULL) { return 1; }
	if (turn_index >= ORCHESTRATOR_MAX_TURNS) { return 1; }
	if (tool_call_index >= ORCHESTRATOR_MAX_TOOL_CALLS_PER_TURN) { return 1; }
	if (conversation->message_count >= ORCHESTRATOR_MAX_MESSAGES) { return 1; }
	stored_content = conversation->tool_result_contents[turn_index][tool_call_index];
	memcpy(stored_content, result->content, sizeof(result->content));
	stored_content[sizeof(result->content) - 1] = '\0';
	conversation->messages[conversation->message_count] = (struct llm_message) {
		.role = "tool",
		.content = stored_content,
		.tool_call_id = conversation->assistant_tool_calls[turn_index][tool_call_index].id,
		.tool_calls = NULL,
		.tool_call_count = 0
	};
	conversation->message_count++;
	return 0;
}

static int append_assistant_tool_calls(struct orchestrator_conversation *conversation, size_t turn_index, const struct llm_response *response) {
	struct llm_tool_call *stored_calls = NULL;
	if (conversation == NULL) { return 1; }
	if (response == NULL) { return 1; }
	if (turn_index >= ORCHESTRATOR_MAX_TURNS) { return 1; }
	if (response->tool_call_count == 0) { return 1; }
	if (response->tool_call_count > ORCHESTRATOR_MAX_TOOL_CALLS_PER_TURN) { return 1; }
	if (conversation->message_count >= ORCHESTRATOR_MAX_MESSAGES) { return 1; }
	stored_calls = conversation->assistant_tool_calls[turn_index];
	memcpy(stored_calls, response->tool_calls, response->tool_call_count * sizeof(response->tool_calls[0]));
	conversation->messages[conversation->message_count] = (struct llm_message) {
		.role = "assistant",
		.content = NULL,
		.tool_call_id = NULL,
		.tool_calls = stored_calls,
		.tool_call_count = response->tool_call_count
	};
	conversation->message_count++;
	return 0;
}

static int append_initial_messages(struct orchestrator_conversation *conversation, struct llm_message_list initial_messages){
	if (conversation == NULL) { return 1; }
	if (initial_messages.items == NULL) { return 1; }
	if (initial_messages.count == 0) { return 1; }
	for (size_t i = 0; i < initial_messages.count; i++) {
		if (conversation->message_count >= ORCHESTRATOR_MAX_MESSAGES) {
			return 1;
		}
		conversation->messages[conversation->message_count] = initial_messages.items[i];
		conversation->message_count++;
	}
	return 0;
}
