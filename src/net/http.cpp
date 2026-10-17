#include "http.hpp"

#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>

#include <curl/curl.h>

namespace net
{
	static size_t write_callback(char* data, size_t size, size_t count, void* user)
	{
		static_cast<std::string*>(user)->append(data, size * count);
		return size * count;
	}

	struct curl_deleter
	{
		void operator()(CURL* curl) const { curl_easy_cleanup(curl); }
	};

	using curl_handle = std::unique_ptr<CURL, curl_deleter>;

	// A handle can be reused for several requests, which keeps its connection open
	static Response perform(CURL* const curl, const std::string& url, const long timeout_ms)
	{
		Response r;
		r.url = url;

		static const std::string user_agent = std::string("curl/") + curl_version_info(CURLVERSION_NOW)->version;

		curl_easy_reset(curl);
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

		return r;
	}

	Response get(const std::string& url, const long timeout_ms)
	{
		const curl_handle curl(curl_easy_init());
		return curl ? perform(curl.get(), url, timeout_ms) : Response{ 0, {}, url };
	}

	std::vector<Response> get_all(const std::vector<std::string>& urls, const size_t max_parallel, const long timeout_ms)
	{
		std::vector<Response> responses(urls.size());
		std::atomic<size_t> next = 0;

		{
			std::vector<std::jthread> workers;
			const size_t count = std::min(max_parallel, urls.size());

			for (size_t i = 0; i < count; ++i)
			{
				workers.emplace_back([&](void)
					{
						const curl_handle curl(curl_easy_init());

						for (size_t index; (index = next++) < urls.size();)
						{
							responses[index] = curl ? perform(curl.get(), urls[index], timeout_ms) : Response{ 0, {}, urls[index] };
						}
					});
			}
		} // jthreads join here

		return responses;
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
