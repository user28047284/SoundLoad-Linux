#include "pch.hpp"
#include "logs.hpp"
#include "../config/config.hpp"
#include "../site_api/site_api.hpp"

// Some distributions build TagLib with debug output enabled, which prints messages such as
// "TagLib: Frame::checkEncoding() -- Rendering using UTF8." for every non-Latin-1 tag
struct silent_taglib_listener : TagLib::DebugListener
{
	void printMessage(const TagLib::String&) override {}
};

static void fix_path(std::string& path)
{
	if (!path.empty())
	{
		if (path.back() != '/') path.push_back('/');
	}
}

static bool save_config(cfg_format& cfg_data_raw)
{
	if (cfg::f.save_cfg || cfg::f.save_cid)
	{
		if (cfg::f.save_cfg)
		{
			cfg::save_config(cfg_data_raw);
		}
		else
		{
			cfg_data_raw.cid = cfg::client_id;
		}

		std::fstream cfg_file(cfg::path_cache, std::ios::out | std::ios::trunc);
		if (cfg_file.fail())
		{
			err::log("failed to open/create cfg.json");
			return false;
		}

		try
		{
			cfg_file << Json(cfg_data_raw).dump();
		}
		catch (...)
		{
			cfg_file.close();
			err::log("failed to save config");
			return false;
		}

		cfg_file.close();
	}

	return true;
}

static bool read_config(cfg_format& cfg_data_raw)
{
	if (std::filesystem::exists(cfg::path_cache))
	{
		std::fstream cfg_file(cfg::path_cache, std::ios::in);
		if (cfg_file.fail())
		{
			err::log("failed to open cfg.json");
			return false;
		}

		try
		{
			Json cfg_data_json;
			cfg_file >> cfg_data_json;
			cfg_data_raw = cfg_data_json.get<cfg_format>();
		}
		catch (...)
		{
			cfg_file.close();
			err::log("failed to read config");
			return false;
		}

		cfg_file.close();
		cfg::read_config(cfg_data_raw);
	}
	else
	{
		cfg::f.fresh_cfg = true;
	}

	return true;
}

static bool init_program(int argc, char* argv[], cfg_format& raw_cfg)
{
	if (!cfg::parse_arguments(argc, argv))
	{
		return false;
	}

	const std::filesystem::path program_dir = cfg::path_cache;
	cfg::path_cache = cfg::config_file_path();

	if (!read_config(raw_cfg))
	{
		return false;
	}

	if (!cfg::f.no_link_provided && cfg::client_id.empty() && !get_client_id())
	{
		err::log("no client ID provided/resolved");
		return false;
	}

	if (cfg::f.add_to_path || cfg::f.fresh_cfg)
	{
		cfg::add_to_path(program_dir); // failure is ignored as its non-vital to primary functionality
	}

	if (cfg::f.no_link_provided)
	{
		if (save_config(raw_cfg))
		{
			std::cout << "\n[!] INPUT HANDLED\n";
		}

		return false;
	}

	return true;
}

int main(int argc, char* argv[])
{
	static silent_taglib_listener taglib_listener;
	TagLib::setDebugListener(&taglib_listener);

	const net::global_init curl_init;

	cfg_format raw_cfg = {};
	if (!init_program(argc, argv, raw_cfg))
	{
		return 1;
	}

	fix_path(cfg::audio_dir);
	fix_path(cfg::image_dir);

	sc_upload post(argv[1]);
	if (post.f.error_occured || !post.download())
	{
		return 1;
	}

	std::cout << "\n[!] DOWNLOAD(s) COMPLETE\n";

	save_config(raw_cfg); // this is done after downloads complete incase request_failed had to get a new CID
	return 0;
}
