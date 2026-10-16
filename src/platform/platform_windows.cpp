#include "pch.hpp"
#include "logs.hpp"
#include "util.hpp"
#include "platform.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <io.h>
#include <cstdio>
#include <memory>

namespace platform
{
	namespace fs = std::filesystem;

	static std::string narrow(const std::wstring_view wide)
	{
		if (wide.empty()) return {};

		const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
		std::string utf8(static_cast<size_t>(size), '\0');
		WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), size, nullptr, nullptr);
		return utf8;
	}

	static std::wstring widen(const std::string_view utf8)
	{
		if (utf8.empty()) return {};

		const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
		std::wstring wide(static_cast<size_t>(size), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), size);
		return wide;
	}

	static std::wstring get_env(const wchar_t* name)
	{
		const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
		if (size == 0) return {};

		std::wstring value(size, L'\0');
		value.resize(GetEnvironmentVariableW(name, value.data(), size));
		return value;
	}

	std::vector<std::string> utf8_arguments(const int argc, char** argv)
	{
		int count = 0;
		LPWSTR* const wide_args = CommandLineToArgvW(GetCommandLineW(), &count);

		if (!wide_args)
		{
			return { argv, argv + argc };
		}

		std::vector<std::string> args;

		for (int i = 0; i < count; ++i)
		{
			args.push_back(narrow(wide_args[i]));
		}

		LocalFree(wide_args);
		return args;
	}

	void init_console(void)
	{
		SetConsoleOutputCP(CP_UTF8);
		SetConsoleCP(CP_UTF8);
	}

	fs::path executable_dir(void)
	{
		std::wstring path(MAX_PATH, L'\0');

		while (true)
		{
			const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));

			if (length == 0)
			{
				return {};
			}

			if (length < path.size())
			{
				path.resize(length);
				return fs::path(path).parent_path();
			}

			path.resize(path.size() * 2); // truncated, try a bigger buffer
		}
	}

	static fs::path known_dir(const wchar_t* env_var)
	{
		const std::wstring value = get_env(env_var);
		return value.empty() ? fs::path{} : fs::path(value) / "soundload";
	}

	fs::path config_dir(void)
	{
		return known_dir(L"APPDATA");
	}

	fs::path state_dir(void)
	{
		return known_dir(L"LOCALAPPDATA");
	}

	std::string_view config_dir_hint(void)
	{
		return "%APPDATA%\\soundload";
	}

	bool stdin_is_terminal(void)
	{
		return _isatty(_fileno(stdin));
	}

	// Quotes an argument the way the Microsoft C runtime splits command lines: backslashes are
	// only special before a quote, so those are doubled.
	static void append_argument(std::wstring& command_line, const std::wstring& arg)
	{
		if (!command_line.empty())
		{
			command_line += L' ';
		}

		if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos)
		{
			command_line += arg;
			return;
		}

		command_line += L'"';

		for (auto it = arg.begin();; ++it)
		{
			size_t backslashes = 0;

			while (it != arg.end() && *it == L'\\')
			{
				++it;
				++backslashes;
			}

			if (it == arg.end())
			{
				command_line.append(backslashes * 2, L'\\');
				break;
			}

			if (*it == L'"')
			{
				command_line.append(backslashes * 2 + 1, L'\\');
			}
			else
			{
				command_line.append(backslashes, L'\\');
			}

			command_line += *it;
		}

		command_line += L'"';
	}

	run_result run_program(const std::vector<std::string>& args)
	{
		std::wstring command_line;

		for (const std::string& arg : args)
		{
			append_argument(command_line, widen(arg));
		}

		// Without an application name, CreateProcessW finds args[0] through PATH and adds ".exe"

		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		PROCESS_INFORMATION process{};

		if (!CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process))
		{
			return run_result::missing;
		}

		WaitForSingleObject(process.hProcess, INFINITE);

		DWORD exit_code = 1;
		GetExitCodeProcess(process.hProcess, &exit_code);

		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);

		return exit_code == 0 ? run_result::ok : run_result::failed;
	}

	// True if dir is one of the ;-separated entries (Windows paths ignore case)
	static bool path_list_contains(const std::wstring_view list, const std::wstring& dir)
	{
		auto same = [](std::wstring_view a, std::wstring_view b)
			{
				while (!a.empty() && (a.back() == L'\\' || a.back() == L'/')) a.remove_suffix(1);
				while (!b.empty() && (b.back() == L'\\' || b.back() == L'/')) b.remove_suffix(1);

				return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
			};

		size_t start = 0;

		while (start <= list.size())
		{
			const size_t end = std::min(list.find(L';', start), list.size());

			if (same(list.substr(start, end - start), dir))
			{
				return true;
			}

			start = end + 1;
		}

		return false;
	}

	void add_to_path(const fs::path& program_dir)
	{
		const std::wstring dir = program_dir.wstring();

		if (path_list_contains(get_env(L"PATH"), dir))
		{
			return;
		}

		// The user PATH is HKEY_CURRENT_USER\Environment\Path. It is edited directly instead of
		// with setx, which cuts values off at 1024 characters.

		HKEY key = nullptr;
		if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key) != ERROR_SUCCESS)
		{
			err::warn("could not open the user environment settings");
			return;
		}

		const std::unique_ptr<std::remove_pointer_t<HKEY>, decltype(&RegCloseKey)> key_guard(key, RegCloseKey);

		DWORD type = REG_EXPAND_SZ;
		DWORD size = 0;
		std::wstring user_path;

		const LSTATUS status = RegQueryValueExW(key, L"Path", nullptr, &type, nullptr, &size);

		if (status == ERROR_SUCCESS)
		{
			if (type != REG_SZ && type != REG_EXPAND_SZ)
			{
				err::warn("your user PATH has an unexpected type, add {} to it yourself", narrow(dir));
				return;
			}

			user_path.resize(size / sizeof(wchar_t));
			RegQueryValueExW(key, L"Path", nullptr, &type, reinterpret_cast<BYTE*>(user_path.data()), &size);

			while (!user_path.empty() && user_path.back() == L'\0') user_path.pop_back();
		}
		else if (status != ERROR_FILE_NOT_FOUND)
		{
			err::warn("could not read your user PATH");
			return;
		}

		if (path_list_contains(user_path, dir))
		{
			std::cout << "[*] sl's folder is already in your PATH, open a new terminal to use it\n";
			return;
		}

		std::cout << "[*] To use sl from any directory, its folder has to be added to your user PATH:\n    " << narrow(dir) << '\n';

		if (!user_path.empty())
		{
			std::cout << "    Your current user PATH (keep a copy to undo the change):\n    " << narrow(user_path) << '\n';
		}

		if (!util::ask_yes_no("Add it now?"))
		{
			return;
		}

		if (!user_path.empty() && user_path.back() != L';')
		{
			user_path += L';';
		}

		user_path += dir;

		const DWORD bytes = static_cast<DWORD>((user_path.size() + 1) * sizeof(wchar_t));

		if (RegSetValueExW(key, L"Path", 0, type, reinterpret_cast<const BYTE*>(user_path.c_str()), bytes) != ERROR_SUCCESS)
		{
			err::warn("could not change your user PATH");
			return;
		}

		// Tell running programs (Explorer, new terminals) that the environment changed

		SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"Environment"), SMTO_ABORTIFHUNG, 5000, nullptr);

		std::cout << "[!] Added sl's folder to PATH, open a new terminal for it to take effect\n";
	}
}
