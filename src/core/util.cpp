#include "pch.hpp"
#include "util.hpp"

#include <unistd.h>

namespace util
{
	std::string lowercase(std::string text)
	{
		for (char& ch : text)
		{
			if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
		}

		return text;
	}

	std::string sanitize_filename(std::string name)
	{
		for (char& ch : name)
		{
			if (std::strchr("<>:\"/\\|?*", ch)) ch = '_';
		}

		return name;
	}

	std::filesystem::path xdg_dir(const char* env_var, const char* home_fallback)
	{
		const char* const value = std::getenv(env_var);
		if (value && *value && std::filesystem::path(value).is_absolute())
		{
			return std::filesystem::path(value) / "soundload";
		}

		const char* const home = std::getenv("HOME");
		if (home && *home)
		{
			return std::filesystem::path(home) / home_fallback / "soundload";
		}

		return {};
	}

	bool interactive(void)
	{
		return isatty(STDIN_FILENO);
	}

	std::string ask(const std::string_view question)
	{
		std::string answer;

		if (interactive())
		{
			std::cout << question << std::flush;
			std::getline(std::cin, answer);
		}

		return answer;
	}

	bool ask_yes_no(const std::string_view question)
	{
		const std::string answer = ask(std::format("{} [y/N] ", question));
		return answer == "y" || answer == "Y";
	}
}
