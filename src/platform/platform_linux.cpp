#include "pch.hpp"
#include "logs.hpp"
#include "util.hpp"
#include "platform.hpp"

#include <poll.h>
#include <spawn.h>
#include <termios.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace platform
{
	namespace fs = std::filesystem;

	std::vector<std::string> utf8_arguments(const int argc, char** argv)
	{
		return { argv, argv + argc };
	}

	void init_console(void)
	{
	}

	fs::path executable_dir(void)
	{
		std::error_code ec;
		const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
		return ec ? fs::path{} : exe.parent_path();
	}

	// $<env_var>/soundload if that is an absolute path, otherwise $HOME/<home_fallback>/soundload
	static fs::path xdg_dir(const char* env_var, const char* home_fallback)
	{
		const char* const value = std::getenv(env_var);
		if (value && *value && fs::path(value).is_absolute())
		{
			return fs::path(value) / "soundload";
		}

		const char* const home = std::getenv("HOME");
		if (home && *home)
		{
			return fs::path(home) / home_fallback / "soundload";
		}

		return {};
	}

	fs::path config_dir(void)
	{
		return xdg_dir("XDG_CONFIG_HOME", ".config");
	}

	fs::path state_dir(void)
	{
		return xdg_dir("XDG_STATE_HOME", ".local/state");
	}

	std::string_view config_dir_hint(void)
	{
		return "~/.config/soundload";
	}

	bool stdin_is_terminal(void)
	{
		return isatty(STDIN_FILENO);
	}

	bool read_line_or_escape(const std::string_view prompt, std::string& line)
	{
		std::cout << prompt << std::flush;

		termios saved{};
		if (tcgetattr(STDIN_FILENO, &saved) != 0)
		{
			return false;
		}

		termios raw = saved;
		raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO | ISIG); // Ctrl+C arrives as a key below
		raw.c_cc[VMIN]  = 1;
		raw.c_cc[VTIME] = 0;
		tcsetattr(STDIN_FILENO, TCSANOW, &raw);

		auto read_byte = [](unsigned char& byte) { return read(STDIN_FILENO, &byte, 1) == 1; };
		auto echo      = [](const std::string_view text) { return write(STDOUT_FILENO, text.data(), text.size()); };

		std::string text;
		bool entered = false;

		for (unsigned char c; read_byte(c);)
		{
			if (c == 27) // Esc on its own, or the start of an arrow or function key sequence
			{
				pollfd next{ STDIN_FILENO, POLLIN, 0 };
				if (poll(&next, 1, 30) <= 0)
				{
					break;
				}

				unsigned char b = 0;
				if (read_byte(b) && (b == '[' || b == 'O'))
				{
					while (read_byte(b) && b < 0x40) {}
				}
			}
			else if (c == '\n' || c == '\r')
			{
				entered = true;
				break;
			}
			else if (c == 3 || (c == 4 && text.empty())) // Ctrl+C, Ctrl+D
			{
				break;
			}
			else if (c == 127 || c == 8)
			{
				if (!text.empty())
				{
					while (!text.empty())
					{
						const unsigned char last = static_cast<unsigned char>(text.back());
						text.pop_back();
						if ((last & 0xC0) != 0x80) break; // that was the first byte of the character
					}

					echo("\b \b");
				}
			}
			else if (c >= 32)
			{
				text += static_cast<char>(c);
				echo(std::string_view(reinterpret_cast<const char*>(&c), 1));
			}
		}

		tcsetattr(STDIN_FILENO, TCSANOW, &saved);
		echo("\n");

		if (entered) line = text;
		return entered;
	}

	run_result run_program(const std::vector<std::string>& args)
	{
		std::vector<char*> argv;

		for (const std::string& arg : args)
		{
			argv.push_back(const_cast<char*>(arg.c_str()));
		}

		argv.push_back(nullptr);

		pid_t pid = 0;
		if (posix_spawnp(&pid, argv[0], nullptr, nullptr, argv.data(), environ) != 0)
		{
			return run_result::missing;
		}

		int status = 0;
		if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
		{
			return run_result::failed;
		}

		return run_result::ok;
	}

	void add_to_path(const fs::path& program_dir)
	{
		const std::string dir = program_dir.string();

		// Already in PATH?

		std::string_view list = std::getenv("PATH") ? std::getenv("PATH") : "";

		while (true)
		{
			const size_t pos = list.find(':');
			const std::string_view entry = list.substr(0, pos);

			if (entry == dir || entry == dir + '/')
			{
				return;
			}

			if (pos == std::string_view::npos) break;
			list.remove_prefix(pos + 1);
		}

		// The line and profile for the user's shell

		const char* const shell_env = std::getenv("SHELL");
		const char* const home_env  = std::getenv("HOME");
		const std::string shell     = shell_env ? fs::path(shell_env).filename().string() : "";
		const fs::path    home      = home_env ? home_env : "";

		std::string line    = "export PATH='" + dir + "':\"$PATH\"";
		fs::path    profile = home / ".bashrc";

		if (shell == "fish")
		{
			line    = "fish_add_path '" + dir + "'";
			profile = home / ".config/fish/config.fish";
		}
		else if (shell == "zsh")
		{
			profile = home / ".zshrc";
		}

		std::cout << "[*] To use sl from any directory, add this line to " << profile.string() << ":\n    " << line << '\n';

		if (!home_env || !util::ask_yes_no("Add it now?"))
		{
			return;
		}

		std::ofstream file(profile, std::ios::app);
		file << '\n' << line << '\n';

		if (file.fail())
		{
			err::warn("failed to write to {}", profile.string());
			return;
		}

		std::cout << "[!] Added sl directory to PATH, restart your shell for it to take effect\n";
	}

	void pause_if_double_clicked(void)
	{
	}
}
