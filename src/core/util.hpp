#pragma once

namespace util
{
	// Lowercases ASCII letters only, so the byte length (and any UTF-8) is unchanged
	std::string lowercase(std::string text);

	// Replaces < > : " / \ | ? * with underscores
	std::string sanitize_filename(std::string name);

	// $<env_var>/soundload if that is an absolute path, otherwise $HOME/<home_fallback>/soundload.
	// Empty if neither is set.
	std::filesystem::path xdg_dir(const char* env_var, const char* home_fallback);

	// True if stdin is a terminal, i.e. there is someone to ask
	bool interactive(void);

	// Prints the question and returns the line typed. Returns "" without asking when not interactive.
	std::string ask(std::string_view question);

	// Asks "<question> [y/N] ". True only for y or Y, so always false when not interactive.
	bool ask_yes_no(std::string_view question);
}
