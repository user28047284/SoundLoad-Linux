#include "http.hpp"

#include <curl/curl.h>

namespace net
{
	static size_t write_callback(char* data, size_t size, size_t count, void* user)
	{
		static_cast<std::string*>(user)->append(data, size * count);
		return size * count;
	}

	Response get(const std::string& url, long timeout_ms)
	{
		Response r;
		r.url = url;

		CURL* const curl = curl_easy_init();
		if (!curl)
		{
			return r;
		}

		static const std::string user_agent = std::string("curl/") + curl_version_info(CURLVERSION_NOW)->version;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // required for multi-threaded use
		curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
		curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
		curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &r.text);

		const CURLcode code = curl_easy_perform(curl);

		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &r.status_code);

		const char* effective_url = nullptr;
		if (curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url) == CURLE_OK && effective_url)
		{
			r.url = effective_url;
		}

		if (code != CURLE_OK)
		{
			r.status_code = 0;
		}

		curl_easy_cleanup(curl);
		return r;
	}

	std::shared_future<Response> get_async(const std::string& url, long timeout_ms)
	{
		return std::async(std::launch::async, [url, timeout_ms](void) { return get(url, timeout_ms); }).share();
	}

	global_init::global_init()
	{
		curl_global_init(CURL_GLOBAL_DEFAULT);
	}

	global_init::~global_init()
	{
		curl_global_cleanup();
	}
}
