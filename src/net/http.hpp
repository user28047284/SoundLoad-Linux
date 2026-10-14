#pragma once

#include <future>
#include <string>

// Minimal libcurl wrapper: blocking and asynchronous GET requests. Session defaults:
// "curl/<version>" user agent, redirects followed (max 50), in-memory cookie engine,
// TCP keepalive and all built-in content encodings accepted.

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

	// Performs a GET request on a separate thread
	std::shared_future<Response> get_async(const std::string& url, long timeout_ms = 4500);

	// RAII wrapper around curl_global_init/curl_global_cleanup, create one in main()
	struct global_init
	{
		global_init();
		~global_init();

		global_init(const global_init&) = delete;
		global_init& operator=(const global_init&) = delete;
	};
}
