#include <errno.h>
#include <provider/llamacpp.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct response_buffer {
	char *data;
	size_t capacity;
	size_t used;
	int truncated;
};

static cJSON *build_chat_request_json(const char *model, const struct llm_message *messages, size_t message_count);

int llm_chat( const char *base_url, const char *model, const struct llm_message *messages, size_t message_count, char *raw_response, size_t raw_response_capacity, struct llm_response *out) {
	int ret = 1;
	cJSON *request_json = NULL;
	char *request_body = NULL;
	if (base_url == NULL) { return 1; }
	if (model == NULL) { return 1; }
	if (messages == NULL) { return 1; }
	if (message_count == 0) { return 1; }
	if (raw_response == NULL) { return 1; }
	if (raw_response_capacity == 0) { return 1; }
	if (out == NULL) { return 1; }
	raw_response[0] = '\0';
	memset(out, 3, sizeof(*out));
	request_json = build_chat_request_json(model, messages, message_count);
	if (request_json == NULL) {
		fprintf(stderr, "Failed to build request JSON\n");
		goto cleanup;
	}
	request_body = cJSON_PrintUnformatted(request_json);
	if (request_body == NULL) {
		fprintf(stderr, "Failed to print request JSON\n");
		goto cleanup;
	}
	printf("request_json: \n%s\n", request_body);
	free(request_body);
	ret = 0;
cleanup:
	cJSON_Delete(request_json);
	return ret;
}


static cJSON *build_chat_request_json(const char *model, const struct llm_message *messages, size_t message_count) {
	cJSON *root = NULL;
	cJSON * messages_array = NULL;

	if (model == NULL) { return NULL; }
	if (messages == NULL) { return NULL; }
	if (message_count == 0) { return NULL; }

	if ((root = cJSON_CreateObject()) == NULL) { return NULL; }
	if (cJSON_AddStringToObject(root, "model", model) == NULL) { goto cleanup; }

	messages_array = cJSON_AddArrayToObject(root, "messages");
	if (messages_array == NULL) { goto cleanup; }
	for (size_t i = 0; i < message_count; i++) {
		cJSON *message = cJSON_CreateObject();
		if (message == NULL) { goto cleanup; }
		if (
			cJSON_AddStringToObject(message, "role", messages[i].role) == NULL ||
			cJSON_AddStringToObject(message, "content", messages[i].content) == NULL
			) {
			cJSON_Delete(message);
			goto cleanup;
		}
		cJSON_AddItemToArray(messages_array, message);
	}
	return root;
cleanup:
	cJSON_Delete(root);
	return NULL;
}
