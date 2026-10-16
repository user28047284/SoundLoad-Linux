#include "pch.hpp"
#include "logs.hpp"
#include "util.hpp"
#include "../config/config.hpp"
#include "../history/history.hpp"
#include "../platform/platform.hpp"
#include "../site_api/site_api.hpp"
#include "version.hpp"

// Some TagLib builds print debug messages for every non-Latin-1 tag
struct silent_taglib_listener : TagLib::DebugListener
{
	void printMessage(const TagLib::String&) override {}
};

// Splits a typed line into arguments (double quotes group words) and moves the link to the front
static std::vector<std::string> split_line(const std::string& line)
{
	std::vector<std::string> args;
	std::string token;
	bool in_quotes = false, has_token = false;

	for (const char ch : line + ' ')
	{
		if (ch == '"')
		{
			in_quotes = !in_quotes;
			has_token = true;
		}
		else if (!in_quotes && std::isspace(static_cast<unsigned char>(ch)))
		{
			if (has_token) args.push_back(std::move(token));
			token.clear();
			has_token = false;
		}
		else
		{
			token += ch;
			has_token = true;
		}
	}

	const auto link = std::ranges::find_if(args, [](const std::string& arg) { return util::istarts_with(arg, "https://soundcloud.com/"); });
	if (link != args.end()) std::rotate(args.begin(), link, link + 1);

	return args;
}

// Started without arguments in a terminal (e.g. double-clicked on Windows): download one line after
// another until Esc is pressed. Every line runs in its own sl process so options don't carry over.
static int run_interactive(const std::string& program)
{
	const std::filesystem::path dir = platform::executable_dir();
	const std::string self = dir.empty() ? program : util::to_utf8(dir / util::to_path(program).filename());

	std::cout << "Paste a SoundCloud link, options can follow it (e.g. -p). Press Esc to exit.\n";

	for (std::string line; platform::read_line_or_escape("\n> ", line);)
	{
		std::vector<std::string> args = split_line(line);
		if (args.empty()) continue;

		args.insert(args.begin(), self);

		if (platform::run_program(args) == platform::run_result::missing)
		{
			err::log("failed to start \"{}\"", self);
			return 1;
		}
	}

	return 0;
}

static int run(const std::vector<std::string>& args)
{
	if (!cfg::parse_arguments(args))
	{
		return 1;
	}

	if (cfg::f.show_help)
	{
		cfg::show_usage();
		return 0;
	}

	if (cfg::f.show_version)
	{
		std::cout << "sl " << sl_version;
		if (!sl_commit.empty()) std::cout << " (" << sl_commit << ')';
		std::cout << '\n';
		return 0;
	}

	if (cfg::f.undo)
	{
		history::undo();
		return 0;
	}

	if (!cfg::load_config())
	{
		return 1;
	}

	if (cfg::f.add_to_path || cfg::f.fresh_cfg)
	{
		cfg::add_to_path(); // not needed to download, so failures are ignored
	}

	// Without a link, the arguments only change settings

	if (cfg::link.empty())
	{
		if (!cfg::save_config())
		{
			return 1;
		}

		std::cout << "\n[!] INPUT HANDLED\n";
		return 0;
	}

	if (cfg::client_id.empty() && !get_client_id())
	{
		err::log("no client ID provided/resolved");
		return 1;
	}

	sc_upload upload(cfg::link);
	const bool downloaded = upload.valid() && upload.download();

	history::save(); // also when a later step failed, so files that were written can be undone

	if (!downloaded)
	{
		return 1;
	}

	std::cout << "\n[!] DOWNLOAD(s) COMPLETE\n";

	cfg::save_config(); // after the download, in case a fresh client ID had to be fetched
	return 0;
}

int main(int argc, char* argv[])
{
	static silent_taglib_listener taglib_listener;
	TagLib::setDebugListener(&taglib_listener);

	platform::init_console();
	const net::global_init curl_init;

	int exit_code = 1;

	try
	{
		const std::vector<std::string> args = platform::utf8_arguments(argc, argv);

		if (args.size() == 1 && platform::stdin_is_terminal())
		{
			return run_interactive(args[0]);
		}

		exit_code = run(args);
	}
	catch (const std::exception& e)
	{
		history::save();
		err::log("unexpected error: {}", e.what());
	}

	platform::pause_if_double_clicked();
	return exit_code;
}
