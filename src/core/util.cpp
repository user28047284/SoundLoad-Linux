#include "pch.hpp"
#include "util.hpp"
#include "../platform/platform.hpp"

namespace util
{
	static char to_lower(const char ch)
	{
		return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : ch;
	}

	std::string lowercase(std::string text)
	{
		std::ranges::transform(text, text.begin(), to_lower);
		return text;
	}

	bool iequals(const std::string_view a, const std::string_view b)
	{
		return std::ranges::equal(a, b, [](const char x, const char y) { return to_lower(x) == to_lower(y); });
	}

	bool istarts_with(const std::string_view text, const std::string_view prefix)
	{
		return text.size() >= prefix.size() && iequals(text.substr(0, prefix.size()), prefix);
	}

	std::string sanitize_filename(std::string name)
	{
		for (char& ch : name)
		{
			if (static_cast<unsigned char>(ch) < 0x20 || std::strchr("<>:\"/\\|?*", ch)) ch = '_';
		}

		// Windows reserves these names, with any extension ("con.m4a" can't be created either)

		constexpr std::string_view reserved[] = { "CON", "PRN", "AUX", "NUL",
			"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
			"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };

		if (std::ranges::any_of(reserved, [&name](const std::string_view r) { return iequals(name, r); }))
		{
			name += '_';
		}

		return name;
	}

	std::filesystem::path to_path(const std::string_view utf8)
	{
		return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
	}

	std::string to_utf8(const std::filesystem::path& path)
	{
		const std::u8string text = path.u8string();
		return { text.begin(), text.end() };
	}

	std::string ask(const std::string_view question)
	{
		std::string answer;

		if (platform::stdin_is_terminal())
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
