#include <provider/llamacpp.h>
#include <tools/schema.h>
#include <tools/registry.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <cjson/cJSON.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LLAMA_ENDPOINT_SUFFIX "/v1/chat/completions"

struct response_buffer {
	char *data;
	size_t capacity;
	size_t used;
	bool truncated;
};

struct llm_tool_schema {
	cJSON *root;
	cJSON *properties;
	cJSON *required;
};

static cJSON *build_chat_request_json(const char *model, const struct llm_message_list messages, const struct tool_registry registry, enum llm_tool_choice_mode tool_choice);
static size_t write_response_callback(char *contents, size_t size, size_t nmemb, void *userp);
static int parse_chat_response_json(const char *raw_response, struct llm_response *out);
static int copy_json_string(const cJSON  *node, char *dest, size_t dest_capacity);
static int parse_tool_calls(const cJSON *tool_calls, struct  llm_response *out);
static cJSON *build_tools_json(const struct tool_registry registry);
static const char *tool_choice_mode_to_string(enum llm_tool_choice_mode mode);

int llm_chat(const char *base_url, const char *model, const struct llm_message_list messages, const struct tool_registry registry, enum llm_tool_choice_mode tool_choice, char *raw_response, size_t raw_response_capacity, struct llm_response *out) {
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
	if (messages.items == NULL) { goto cleanup; }
	if (messages.count == 0) { goto cleanup; }
	if (raw_response == NULL) { goto cleanup; }
	if (raw_response_capacity == 0) { goto cleanup; }
	if (out == NULL) { goto cleanup; }
	for (size_t i = 0; i < messages.count; i++) {
		if (messages.items[i].role == NULL) { goto cleanup; }
		if (messages.items[i].content == NULL) { goto cleanup; }
	}
	if (registry.count > 0 && registry.items == NULL) { goto cleanup; }
	if (tool_choice != LLM_TOOL_CHOICE_NONE && registry.count == 0) { goto cleanup; }
	for (size_t i = 0; i < registry.count; i++) {
		if (registry.items[i].definition.name == NULL) { goto cleanup; }
		if (registry.items[i].definition.description == NULL) { goto cleanup; }
		if (registry.items[i].definition.parameters == NULL) { goto cleanup; }
	}
	raw_response[0] = '\0';
	memset(out, 0, sizeof(*out));
	request_json = build_chat_request_json(model, messages, registry, tool_choice);
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
	if ((curl_result = curl_easy_setopt(curl, CURLOPT_URL, url)) != CURLE_OK) {
		fprintf(stderr, "Failed to set CURLOPT_URL: %s\n", curl_easy_strerror(curl_result));
		goto cleanup;
	}
	if ((curl_result = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers)) != CURLE_OK) {
		fprintf(stderr, "Failed to set CURLOPT_HTTPHEADER: %s\n", curl_easy_strerror(curl_result));
		goto cleanup;
	}
	// curl_easy_setopt(curl, CURLOPT_POST, 1L);
	if ((curl_result = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body)) != CURLE_OK) {
		fprintf(stderr, "Failed to set CURLOPT_POSTFIELDS: %s\n", curl_easy_strerror(curl_result));
		goto cleanup;
	}
	if ((curl_result = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback)) != CURLE_OK) {
		fprintf(stderr, "Failed to set CURLOPT_WRITEFUNCTION: %s\n", curl_easy_strerror(curl_result));
		goto cleanup;
	}
	if ((curl_result = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response_state)) != CURLE_OK) {
		fprintf(stderr, "Failed to set CURLOPT_WRITEDATA: %s\n", curl_easy_strerror(curl_result));
		goto cleanup;
	}
	if ((curl_result = curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L)) != CURLE_OK) {
		fprintf(stderr, "Failed to set CURLOPT_TIMEOUT: %s\n", curl_easy_strerror(curl_result));
		goto cleanup;
	}
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
	if (parse_chat_response_json(response_state.data, out) != 0) {
		fprintf(stderr, "Failed to parse response json\n");
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

static const char *tool_choice_mode_to_string(enum llm_tool_choice_mode mode) {
	switch (mode) {
		case LLM_TOOL_CHOICE_NONE:
			return NULL;
		case LLM_TOOL_CHOICE_AUTO:
			return "auto";
		case LLM_TOOL_CHOICE_REQUIRED:
			return "required";
		default:
			return NULL;
	}
}

static cJSON *build_tools_json(const struct tool_registry registry) {
	cJSON *tools_array = NULL;
	if (registry.items == NULL) { return NULL; }
	if (registry.count == 0) { return NULL; }
	tools_array = cJSON_CreateArray();
	if (tools_array == NULL) { return NULL; }
	for (size_t i = 0; i < registry.count; i++) {
		cJSON *tool = NULL;
		cJSON *function = NULL;
		cJSON *parameters = NULL;
		if (registry.items[i].definition.name == NULL) { goto cleanup; }
		if (registry.items[i].definition.description == NULL) { goto cleanup; }
		if (registry.items[i].definition.parameters == NULL) { goto cleanup; }
		if ((tool = cJSON_CreateObject()) == NULL) { goto cleanup; }
		if (cJSON_AddStringToObject(tool, "type", "function") == NULL) {
			goto cleanup_tool;
		}
		if ((function = cJSON_AddObjectToObject(tool, "function")) == NULL) {
			goto cleanup_tool;
		}
		if (cJSON_AddStringToObject(function, "name", registry.items[i].definition.name) == NULL) {
			goto cleanup_tool;
		}
		if (cJSON_AddStringToObject(function, "description", registry.items[i].definition.description) == NULL) {
			goto cleanup_tool;
		}
		parameters = registry.items[i].definition.parameters->root;
		// if ((parameters = cJSON_Parse(registry.items[i].definition.parameters[0].root)) == NULL) {
		// 	goto cleanup_tool;
		// }
		if (!cJSON_AddItemToObject(function, "parameters", parameters)) {
			goto cleanup_tool;
		}
		if (!cJSON_AddItemToArray(tools_array, tool)) { goto cleanup_tool; }
		continue;
cleanup_tool:
		cJSON_Delete(tool);
		goto cleanup;
	}
	return tools_array;
cleanup:
	cJSON_Delete(tools_array);
	return NULL;
}

static int parse_tool_calls(const cJSON *tool_calls, struct  llm_response *out) {
	if (out == NULL) { return 1; }
	out->tool_call_count = 0;
	if (tool_calls == NULL) { return 0; }
	if (!cJSON_IsArray(tool_calls)) { return 1; }
	int array_size = cJSON_GetArraySize(tool_calls);
	for (int i = 0; i < array_size; i++) {
		if (out->tool_call_count >= 8) { return 1; }
		const cJSON *tool_call = cJSON_GetArrayItem(tool_calls, i);
		const cJSON *id;
		const cJSON *function;
		const cJSON *name;
		const cJSON *arguments;
		struct llm_tool_call *out_call;
		if (tool_call == NULL) { return 1; }
		if (!cJSON_IsObject(tool_call)) { return 1; }
		id = cJSON_GetObjectItemCaseSensitive(tool_call, "id");
		if (id == NULL) { return 1; }
		function = cJSON_GetObjectItemCaseSensitive(tool_call, "function");
		if (function == NULL) { return 1; }
		if (!cJSON_IsObject(function)) { return 1; }
		name = cJSON_GetObjectItemCaseSensitive(function, "name");
		if (name == NULL) { return 1; }
		arguments = cJSON_GetObjectItemCaseSensitive(function, "arguments");
		if (arguments == NULL) { return 1; }
		out_call = &out->tool_calls[out->tool_call_count];
		if (copy_json_string(id, out_call->id, sizeof(out_call->id)) != 0) {
			return 1;
		}
		if (copy_json_string(name, out_call->name, sizeof(out_call->name)) != 0) {
			return 1;
		}
		if (copy_json_string(arguments, out_call->arguments, sizeof(out_call->arguments)) != 0) {
			return 1;
		}
		out->tool_call_count++;
	}
	return 0;
}

static int copy_json_string(const cJSON  *node, char *dest, size_t dest_capacity) {
	if (node == NULL) { return 1; }
	if (dest == NULL) { return 1; }
	if (dest_capacity == 0) { return 1; }
	if (!cJSON_IsString(node)) { return 1; }
	if (node->valuestring == NULL) { return 1; }
	size_t available = dest_capacity - 1;
	size_t len = strlen(node->valuestring);
	size_t to_copy = len > available ? available : len;
	memcpy(dest, node->valuestring, to_copy);
	dest[to_copy] = '\0';
	return 0;
}

static int parse_chat_response_json(const char *raw_response, struct llm_response *out) {
	int ret = 1;
	cJSON *root;
	cJSON *choices;
	cJSON *choice;
	cJSON *message;
	cJSON *content;
	cJSON *tool_calls;
	// cJSON *role;
	if (raw_response == NULL) { return 1; }
	if (out == NULL) { return 1; }
	memset(out, 0, sizeof(*out));
	if ((root = cJSON_Parse(raw_response)) == NULL) { return 1; }
	choices = cJSON_GetObjectItemCaseSensitive(root, "choices");
	if (choices == NULL) { goto cleanup; }
	if (!cJSON_IsArray(choices)) { goto cleanup; }
	choice = cJSON_GetArrayItem(choices, 0);
	if (choice == NULL) { goto cleanup; }
	if (!cJSON_IsObject(choice)) { goto cleanup; }
	message = cJSON_GetObjectItemCaseSensitive(choice, "message");
	if (message == NULL) { goto cleanup; }
	if (!cJSON_IsObject(message)) { goto cleanup; }
	content = cJSON_GetObjectItemCaseSensitive(message, "content");
	if (content == NULL) {
	} else if (cJSON_IsNull(content)) {
	} else if (cJSON_IsString(content) && content->valuestring != NULL) {
		if (copy_json_string(content, out->content, sizeof(out->content)) != 0) {
			goto cleanup;
		}
		out->has_content = true;
	} else {
		goto cleanup;
	}
	tool_calls = cJSON_GetObjectItemCaseSensitive(message, "tool_calls");
	if (parse_tool_calls(tool_calls, out) != 0) { goto cleanup; }
	if (!out->has_content && out->tool_call_count == 0) { goto cleanup; }
	printf("content: \n%s\n", out->content);
	ret = 0;
cleanup:
	cJSON_Delete(root);
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


static cJSON *build_chat_request_json(const char *model, const struct llm_message_list messages, const struct tool_registry registry, enum llm_tool_choice_mode tool_choice) {
	cJSON *root = NULL;
	cJSON *messages_array = NULL;
	cJSON *tools_array = NULL;
	const char *tool_choice_string = NULL;

	if (model == NULL) { return NULL; }
	if (messages.items == NULL) { return NULL; }
	if (messages.count == 0) { return NULL; }

	if ((root = cJSON_CreateObject()) == NULL) { return NULL; }
	if (cJSON_AddStringToObject(root, "model", model) == NULL) { goto cleanup; }

	messages_array = cJSON_AddArrayToObject(root, "messages");
	if (messages_array == NULL) { goto cleanup; }
	for (size_t i = 0; i < messages.count; i++) {
		cJSON *message = cJSON_CreateObject();
		if (message == NULL) { goto cleanup; }
		if (messages.items[i].role == NULL) { goto cleanup_message; }
		if (messages.items[i].content == NULL) { goto cleanup_message; }
		if (cJSON_AddStringToObject(message, "role", messages.items[i].role) == NULL) {
			goto cleanup_message;
		}
		if (cJSON_AddStringToObject(message, "content", messages.items[i].content) == NULL) {
			goto cleanup_message;
		}
		cJSON_AddItemToArray(messages_array, message);
		continue;
cleanup_message:
		cJSON_Delete(message);
		goto cleanup;
	}
	if (registry.count > 0) {
		tools_array = build_tools_json(registry);
		if (tools_array == NULL) { goto cleanup; }
		if (!cJSON_AddItemToObject(root, "tools", tools_array)) {
			cJSON_Delete(tools_array);
			goto cleanup;
		}
		tools_array = NULL;
		if ((tool_choice_string = tool_choice_mode_to_string(tool_choice)) != NULL) {
			if (cJSON_AddStringToObject(root, "tool_choice", tool_choice_string) == NULL) {
				goto cleanup;
			}
		}
	}
	return root;
cleanup:
	cJSON_Delete(root);
	return NULL;
}
