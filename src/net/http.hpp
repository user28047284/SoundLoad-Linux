#pragma once

#include <string>
#include <vector>

// Minimal libcurl wrapper: curl user agent, redirects, cookies, keepalive and compression

namespace net
{
	struct Response
	{
		long        status_code = 0; // HTTP status, 0 if the transfer itself failed
		std::string text;            // response body
		std::string url;             // effective URL (after redirects)
	};

	// Performs a blocking GET request
	Response get(const std::string& url, long timeout_ms = 4500);

	// Requests all URLs, at most max_parallel at a time. Responses are in the same order as urls.
	std::vector<Response> get_all(const std::vector<std::string>& urls, size_t max_parallel, long timeout_ms = 4500);

	// RAII wrapper around curl_global_init/curl_global_cleanup, create one in main()
	struct global_init
	{
		global_init();
		~global_init();

		global_init(const global_init&) = delete;
		global_init& operator=(const global_init&) = delete;
	};
}
