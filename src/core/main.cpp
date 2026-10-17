#include "pch.hpp"
#include "logs.hpp"
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
		exit_code = run(platform::utf8_arguments(argc, argv));
	}
	catch (const std::exception& e)
	{
		history::save();
		err::log("unexpected error: {}", e.what());
	}

	platform::pause_if_double_clicked();
	return exit_code;
}
