#pragma once

#include "../net/http.hpp"

// Errors and warnings go to stderr. The mutex keeps lines from different threads apart.

namespace err
{
	inline std::mutex log_mutex;

	inline void print(const std::string_view prefix, const std::string& message)
	{
		const std::lock_guard lock(log_mutex);
		std::cerr << prefix << message << '\n';
	}

	template <typename... T> void warn(std::format_string<T...> fmt, T&&... args)
	{
		print("WARNING: ", std::format(fmt, std::forward<T>(args)...));
	}

	template <typename... T> void log(std::format_string<T...> fmt, T&&... args)
	{
		print("ERROR: ", std::format(fmt, std::forward<T>(args)...));
	}

	inline void log_net(const net::Response& r)
	{
		log("request to \"{}\" failed with code {}", r.url, r.status_code);
	}
}
