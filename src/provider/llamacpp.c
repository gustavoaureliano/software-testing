#include <provider/llamacpp.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/typecheck-gcc.h>
#include <cjson/cJSON.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define LLAMA_ENDPOINT_SUFFIX "/v1/chat/completions"

struct response_buffer {
	char *data;
	size_t capacity;
	size_t used;
	bool truncated;
};

static cJSON *build_chat_request_json(const char *model, const struct llm_message *messages, size_t message_count);
static size_t write_response_callback(char *contents, size_t size, size_t nmemb, void *userp);
// static int copy_json_string(const cJSON  *ndode, char *dest, size_t dest_capacity);
// static int parse_chat_response_json(const char *raw_response, struct llm_response *out);
// static int parse_tool_calls(const cJSON *tool_calls, struct  llm_response *out);

int llm_chat( const char *base_url, const char *model, const struct llm_message *messages, size_t message_count, char *raw_response, size_t raw_response_capacity, struct llm_response *out) {
	int ret = 1;
	cJSON *request_json = NULL;
	char *request_body = NULL;
	char url[MAX_URL_SIZE];
	struct response_buffer response_state;
	CURL *curl = NULL;
	CURLcode curl_result;
	struct curl_slist *headers = NULL;
	long http_status = 0;
	if (base_url == NULL) { goto cleanup; }
	if (model == NULL) { goto cleanup; }
	if (messages == NULL) { goto cleanup; }
	if (message_count == 0) { goto cleanup; }
	if (raw_response == NULL) { goto cleanup; }
	if (raw_response_capacity == 0) { goto cleanup; }
	if (out == NULL) { goto cleanup; }
	for (size_t i = 0; i < message_count; i++) {
		if (messages[i].role == NULL) { goto cleanup; }
		if (messages[i].content == NULL) { goto cleanup; }
	}
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
	if (snprintf(url, MAX_URL_SIZE, "%s%s", base_url, LLAMA_ENDPOINT_SUFFIX) >= MAX_URL_SIZE) {
		fprintf(stderr, "URL too long\n");
		goto cleanup;
	}
	response_state = (struct response_buffer) {
		.data = raw_response,
		.capacity = raw_response_capacity,
		.used = 0,
		.truncated = false
	};
	curl = curl_easy_init();
	if (curl == NULL) {
		fprintf(stderr, "Failed to initialize curl\n");
		goto cleanup;
	}
	headers = curl_slist_append(headers, "Content-Type: application/json");
	if (headers == NULL) {
		fprintf(stderr, "Failed to create curl request headers\n");
		goto cleanup;
	}
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	// curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response_state);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
	curl_result = curl_easy_perform(curl);
	raw_response[response_state.used] = '\0';
	if (curl_result != CURLE_OK) {
		fprintf(stderr, "curl request failed: %s\n", curl_easy_strerror(curl_result));
		goto cleanup;
	}
	printf("curl_response: \n%s\n", response_state.data);
	if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status) != CURLE_OK) {
		fprintf(stderr, "Failed to get HTTP status code\n");
		goto cleanup;
	}
	if (http_status < 200 || http_status >= 300) {
		fprintf(stderr, "Unexpected HTTP status: %ld\n", http_status);
		goto cleanup;
	}
	if (response_state.truncated) {
		fprintf(stderr, "Response buffer too small\n");
		goto cleanup;
	}
	ret = 0;
cleanup:
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	free(request_body);
	cJSON_Delete(request_json);
	return ret;
}

static size_t write_response_callback(char *contents, size_t size, size_t nmemb, void *userp) {
	if (contents == NULL) { return 0; }
	if (size == 0) { return 0; }
	if (nmemb == 0) { return 0; }
	if (userp == NULL) { return 0; }
	size_t realsize = size * nmemb;
	struct response_buffer *buf = (struct response_buffer *)userp;
	if (buf->data == NULL) { return 0; }
	if (buf->capacity == 0) { return 0;}
	if (buf->used >= buf->capacity - 1) {
		buf->truncated = true;
		return realsize;
	}
	size_t available = buf->capacity - 1 - buf->used;
	size_t to_copy = realsize > available ? available : realsize;
	memcpy(buf->data + buf->used, contents, to_copy);
	buf->used += to_copy;
	buf->data[buf->used] = '\0';
	if (to_copy < realsize) {
		buf->truncated = true;
	}
	return realsize;
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
		if (messages[i].role == NULL) { goto cleanup_message; }
		if (messages[i].content == NULL) { goto cleanup_message; }
		if (cJSON_AddStringToObject(message, "role", messages[i].role) == NULL) {
			goto cleanup_message;
		}
		if (cJSON_AddStringToObject(message, "content", messages[i].content) == NULL) {
			goto cleanup_message;
		}
		cJSON_AddItemToArray(messages_array, message);
		continue;
cleanup_message:
		cJSON_Delete(message);
		goto cleanup;
	}
	return root;
cleanup:
	cJSON_Delete(root);
	return NULL;
}
