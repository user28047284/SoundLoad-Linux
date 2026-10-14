#pragma once

#include "../net/http.hpp"

namespace err
{
	inline std::mutex log_mutex;

	template <typename... t> void warn(std::format_string<t...> fmt, t&&... args)
	{
		const std::lock_guard<std::mutex> lock(log_mutex);
		std::cerr << "WARNING: " << std::format(fmt, std::forward<t>(args)...) << '\n';
	}

	template <typename... t> void warn_ex(std::format_string<t...> fmt, t&&... args)
	{
		const int error = errno;
		const std::lock_guard<std::mutex> lock(log_mutex);
		std::cerr << "WARNING: " << std::format(fmt, std::forward<t>(args)...) << "\nError code: " << error << " (" << std::strerror(error) << ")\n";
	}

	template <typename... t> void log(std::format_string<t...> fmt, t&&... args)
	{
		const std::lock_guard<std::mutex> lock(log_mutex);
		std::cerr << "ERROR: " << std::format(fmt, std::forward<t>(args)...) << '\n';
	}

	template <typename... t> void log_ex(std::format_string<t...> fmt, t&&... args)
	{
		const int error = errno;
		const std::lock_guard<std::mutex> lock(log_mutex);
		std::cerr << "ERROR: " << std::format(fmt, std::forward<t>(args)...) << "\nError code: " << error << " (" << std::strerror(error) << ")\n";
	}

	inline void log_net(const net::Response& r)
	{
		const std::lock_guard<std::mutex> lock(log_mutex);
		std::cerr << "ERROR: request to \"" << r.url << "\" failed with code " << r.status_code << '\n';
	}
}
