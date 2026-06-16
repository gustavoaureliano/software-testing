#include <cli/cli.h>
#include <curl/curl.h>

int main(int argc, char *argv[]) {
	int result;
	CURLcode curl_result;
	curl_result = curl_global_init(CURL_GLOBAL_ALL);
	if (curl_result != CURLE_OK) {
		return (int)curl_result;
	}
	result = run_subcommand(argc, argv);
	curl_global_cleanup();
	return result;
}
