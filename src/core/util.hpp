#pragma once

namespace util
{
	// Lowercases ASCII letters only, so the byte length (and any UTF-8) is unchanged
	std::string lowercase(std::string text);

	// ASCII case-insensitive comparisons
	bool iequals(std::string_view a, std::string_view b);
	bool istarts_with(std::string_view text, std::string_view prefix);

	// Makes a file name (without extension) valid on Linux and Windows: < > : " / \ | ? * and
	// control characters become underscores, and Windows device names (CON, NUL, COM1...) get one added
	std::string sanitize_filename(std::string name);

	// Converts between UTF-8 strings and paths. Always use these for file access: on Windows a
	// plain std::string is read in the ANSI code page, which breaks names like "💎💎".
	std::filesystem::path to_path(std::string_view utf8);
	std::string           to_utf8(const std::filesystem::path& path);

	// Prints the question and returns the line typed. Returns "" without asking when not interactive.
	std::string ask(std::string_view question);

	// Asks "<question> [y/N] ". True only for y or Y, so always false when not interactive.
	bool ask_yes_no(std::string_view question);
}
