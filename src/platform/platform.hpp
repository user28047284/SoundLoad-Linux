#pragma once

// Everything that differs between Linux and Windows. CMake compiles platform_linux.cpp or
// platform_windows.cpp. All strings are UTF-8, paths are converted with util::to_path.

namespace platform
{
	// The command line as UTF-8 (Windows passes argv in the ANSI code page)
	std::vector<std::string> utf8_arguments(int argc, char** argv);

	// Makes the console display UTF-8 output
	void init_console(void);

	// The folder sl is in, empty if it can't be determined
	std::filesystem::path executable_dir(void);

	// Per-user folders for cfg.json (with -xdg) and the -undo history, empty if they can't be determined
	std::filesystem::path config_dir(void);
	std::filesystem::path state_dir(void);

	// How config_dir() is shown in the help, e.g. "~/.config/soundload"
	std::string_view config_dir_hint(void);

	// True if stdin is a terminal, i.e. there is someone to ask
	bool stdin_is_terminal(void);

	enum class run_result
	{
		ok,
		missing, // the program is not installed (or could not be started)
		failed   // it ran but exited with an error
	};

	// Runs a program found through PATH, without a shell, and waits for it. args[0] is the program.
	run_result run_program(const std::vector<std::string>& args);

	// Shows how to add dir to PATH and offers to do it (asks first)
	void add_to_path(const std::filesystem::path& dir);
}
