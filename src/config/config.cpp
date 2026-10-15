#include "pch.hpp"
#include "logs.hpp"
#include "config.hpp"

#include <strings.h>
#include <unistd.h>

namespace cfg
{
	//
	//// ARGUMENT PARSING
	//

	static void show_usage(void)
	{
		std::cout << "[*] ABOUT THIS PROGRAM\n";
		std::cout << "All valid arguments are listed below. You MUST surround any arguments with shell operators (e.g. < | & ? * ;) or spaces in quotes. THIS INCLUDES LINKS WITH '?' IN THEM!\n";
		std::cout << "Some features provide short and long versions of the same argument. The short version is in parenthesis at the start of the line, if it exists.\n";
		std::cout << "For detailed usage instructions: https://github.com/user28047284/SoundLoad-Linux\n";

		std::cout << "\n[+] GENERAL CONFIGURATION\n";
		std::cout << "-audio     ; [SAVEABLE, DEFAULT] enables the download of audio files\n";
		std::cout << "-n-audio   ; [SAVEABLE] disables the download of audio files\n";
		std::cout << "-art       ; [SAVEABLE] enables cover art downloads\n";
		std::cout << "-n-art     ; [SAVEABLE, DEFAULT] disables cover art downloads\n";
		std::cout << "-pvars     ; shows how to add the program to PATH (asks before changing any file)\n";
		std::cout << "-cid <cid> ; [SAVEABLE] sets the client ID (use only if you know what you're doing)\n";
		std::cout << "-xdg       ; stores cfg.json in ~/.config/soundload instead of the program directory (used automatically once it exists there)\n";
		std::cout << "(-s) -save ; saves applicable arguments to cfg.json\n";

		std::cout << "\n[+] FILE INFO\n";
		std::cout << "(-adst)  -audio-dst <dir>   ; [SAVEABLE] sets the download directory for audio files\n";
		std::cout << "(-idst)  -img-dst <dir>     ; [SAVEABLE] sets the download directory for images\n";
		std::cout << "(-aname) -audio-name <name> ; the name to be given to the downloaded audio file\n";
		std::cout << "(-iname) -img-name <name>   ; the name to be given to the downloaded cover art\n";
		std::cout << "(-isrc)  -img-src <source>  ; the cover art that will be added to the audio (soundcloud track link, image link, or image path)\n";

		std::cout << "\n[+] METADATA\n";
		std::cout << "(-t)  -title <title       ; title property of audio tag (also used for the file name if one is not specified)\n";
		std::cout << "(-c)  -comment <comment>  ; comment property of the audio tag (automatically adds the upload date, upload description, and tags)\n";
		std::cout << "(-a)  -artists <artists>  ; contributing artists property of audio tag (defaults to artist metadata or uploader)\n";
		std::cout << "(-aa) -a-artist <artist>  ; album artist property of audio tag (defaults to album uploader if in an album)\n";
		std::cout << "(-al) -album <album>      ; album property of audio tag (defaults to track name or, if track is in an album, the album name)\n";
		std::cout << "(-g)  -genre <genre>      ; genre property of audio tag (defaults to parsed genre from track)\n";
		std::cout << "(-n)  -num <track number> ; track number property of audio tag (defaults to track number in album if track is in an album)\n";
		std::cout << "(-y)  -year <year>        ; year property of audio tag (defaults to year of upload date)\n";
		std::cout << "(-p)  -parse              ; splits a track name like \"Artist - Song\" into artist + title (uses the first \" - \"; -title and -artists still win)\n";
	}

	bool parse_arguments(int argc, const char* const argv[])
	{
		// Checking arg count

		if (argc < 2)
		{
			err::log("insufficient arguments, use '-?' for program usage");
			return false;
		}

		// Checking if first argument (link required for any downloads)

		if (strncasecmp(argv[1], "https://soundcloud.com/", 23) != 0)
		{
			cfg::f.no_link_provided = true;

			if (!strcmp(argv[1], "-?"))
			{
				show_usage();
				return false;
			}
		}

		// Parsing arguments (starts at index 2 if first arg is a link)

		for (int i = 1 + ((cfg::f.no_link_provided) == false); i < argc; ++i)
		{
			const char* const arg = argv[i];
			auto log_bad_arg = [&arg](void)
				{
					err::log("invalid argument \"{}\"", arg);
				};

			if (arg[0] != '-' || !arg[1])
			{
				log_bad_arg();
				return false;
			}

			const uint32_t arg_hash = hash_rt(&arg[1]);
			bool handled = true;

			switch (arg_hash)
			{
			case hash("s"):       { [[fallthrough]];                             }
			case hash("save"):    { cfg::f.save_cfg          = true; break; }
			case hash("art"):     { cfg::f.seperate_art      = true; break; }
			case hash("n-art"):   { cfg::f.no_seperate_art   = true; break; }
			case hash("audio"):   { cfg::f.download_audio    = true; break; }
			case hash("n-audio"): { cfg::f.no_audio_download = true; break; }
			case hash("pvars"):   { cfg::f.add_to_path       = true; break; }
			case hash("xdg"):            { cfg::f.use_xdg          = true; break; }
			case hash("p"):              { [[fallthrough]]; }
			case hash("parse"):          { cfg::f.parse_title      = true; break; }

			default: handled = false;
			}

			if (!handled)
			{
				++i;

				if (i >= argc)
				{
					err::log("no value provided for argument \"{}\"", arg);
					return false;
				}

				const char* const next_arg = argv[i];
				if (!next_arg)
				{
					err::log("argv/argc mismatch occured, retry or recompile the program");
					return false;
				}

				switch (arg_hash)
				{
					// Config data arguments

				case hash("iname"): [[fallthrough]];
				case hash("img-name"):   { cfg::g_data.image_name = next_arg; break; }

				case hash("aname"): [[fallthrough]];
				case hash("audio-name"): { cfg::g_data.audio_name = next_arg; break; }

				case hash("idst"):  [[fallthrough]];
				case hash("img-dst"):    { cfg::image_dir         = next_arg; break; }

				case hash("adst"):  [[fallthrough]];
				case hash("audio-dst"):  { cfg::audio_dir         = next_arg; break; }

				case hash("isrc"):  [[fallthrough]];
				case hash("img-src"):    { cfg::image_src         = next_arg; break; }

					// Audio tag arguments

				case hash("t"):  [[fallthrough]];
				case hash("title"):    { cfg::g_data.title           = next_arg; break; }

				case hash("c"):  [[fallthrough]];
				case hash("comment"):  { cfg::g_data.comments        = next_arg; break; }

				case hash("a"):  [[fallthrough]];
				case hash("artists"):  { cfg::g_data.contrib_artists = next_arg; break; }

				case hash("aa"): [[fallthrough]];
				case hash("a-artist"): { cfg::g_data.album_artists   = next_arg; break; }

				case hash("al"): [[fallthrough]];
				case hash("album"):    { cfg::g_data.album           = next_arg; break; }

				case hash("g"):  [[fallthrough]];
				case hash("genre"):    { cfg::g_data.genre           = next_arg; break; }

				case hash("n"):  [[fallthrough]];
				case hash("num"):      { cfg::g_data.number          = next_arg; break; }

				case hash("y"):  [[fallthrough]];
				case hash("year"):     { cfg::g_data.year            = next_arg;   break; }

					// Extra

				case hash("cid"):      { cfg::client_id              = next_arg; break; }

					// Invalid arguments

				default:
				{
					log_bad_arg();
					return false;
				}
				}
			}
		}

		// Getting cover art source type

		if (!cfg::image_src.empty())
		{
			if (std::filesystem::exists(cfg::image_src))
			{
				cfg::f.cover_src_path = true;
			}
			else if (cfg::image_src.starts_with("https://soundcloud.com/"))
			{
				cfg::f.cover_src_sc_link = true;
			}
		}

		// Getting process directory

		std::error_code ec;
		const std::filesystem::path program_path = std::filesystem::read_symlink("/proc/self/exe", ec);
		if (ec)
		{
			err::log("failed to get sl path ({})", ec.message());
			return false;
		}

		cfg::path_cache = program_path.parent_path();

		return true;
	}

	//
	//// CONFIG HANDLING
	//

	void read_config(const cfg_format& data)
	{
		if (!data.cid.empty() && cfg::client_id.empty())
		{
			cfg::client_id = data.cid;
		}

		if (!data.art_out_dir.empty() && cfg::image_dir.empty())
		{
			cfg::image_dir = data.art_out_dir;
		}

		if (!data.track_out_dir.empty() && cfg::audio_dir.empty())
		{
			cfg::audio_dir = data.track_out_dir;
		}

		if (!cfg::audio_flags_set() && !data.get_track_audio)
		{
			cfg::f.no_audio_download = true;
		}

		if (!cfg::art_flags_set() && data.get_track_art)
		{
			cfg::f.seperate_art = true;
		}
	}

	void save_config(cfg_format& data)
	{
		if (!cfg::client_id.empty())
		{
			data.cid = cfg::client_id;
		}

		if (!cfg::image_dir.empty())
		{
			data.art_out_dir = cfg::image_dir;
		}

		if (!cfg::audio_dir.empty())
		{
			data.track_out_dir = cfg::audio_dir;
		}

		if (cfg::audio_flags_set())
		{
			data.get_track_audio = cfg::f.download_audio;
		}

		if (cfg::art_flags_set())
		{
			data.get_track_art = cfg::f.seperate_art;
		}
	}

	//
	//// PATH variable
	//

	static std::optional<std::filesystem::path> xdg_config_dir(const char* xdg_config_home, const char* home)
	{
		if (xdg_config_home && *xdg_config_home && std::filesystem::path(xdg_config_home).is_absolute())
		{
			return std::filesystem::path(xdg_config_home) / "soundload";
		}

		if (home && *home)
		{
			return std::filesystem::path(home) / ".config" / "soundload";
		}

		return std::nullopt;
	}

	std::filesystem::path config_file_path(void)
	{
		// The XDG config directory is used with -xdg, or if a config was already saved there

		if (const auto dir = xdg_config_dir(std::getenv("XDG_CONFIG_HOME"), std::getenv("HOME")))
		{
			if (std::filesystem::exists(*dir / "cfg.json"))
			{
				return *dir / "cfg.json";
			}

			std::error_code ec;
			if (cfg::f.use_xdg && (std::filesystem::create_directories(*dir, ec), !ec))
			{
				return *dir / "cfg.json";
			}
		}

		return cfg::path_cache / "cfg.json";
	}

	void add_to_path(const std::filesystem::path& program_dir)
	{
		const std::string dir = program_dir.string();

		// Checking if the program directory is already in PATH

		const char* const path_env = std::getenv("PATH");
		std::string_view list = path_env ? path_env : "";

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

		// Getting the line and profile for the user's shell

		const char* const shell_env = std::getenv("SHELL");
		const char* const home      = std::getenv("HOME");
		const std::string shell     = shell_env ? std::filesystem::path(shell_env).filename().string() : "";

		std::string line = "export PATH='" + dir + "':\"$PATH\"";
		std::filesystem::path profile = std::filesystem::path(home ? home : "") / ".bashrc";

		if (shell == "fish")
		{
			line = "fish_add_path '" + dir + "'";
			profile = std::filesystem::path(home ? home : "") / ".config/fish/config.fish";
		}
		else if (shell == "zsh")
		{
			profile = std::filesystem::path(home ? home : "") / ".zshrc";
		}

		// Asking before changing anything

		std::cout << "[*] To use sl from any directory, add this line to " << profile.string() << ":\n    " << line << '\n';

		if (!home || !isatty(STDIN_FILENO))
		{
			return;
		}

		std::cout << "Add it now? [y/N] " << std::flush;

		std::string answer;
		std::getline(std::cin, answer);

		if (answer != "y" && answer != "Y")
		{
			return;
		}

		std::ofstream file(profile, std::ios::app);
		file << '\n' << line << '\n';

		if (file.fail())
		{
			err::warn_ex("failed to write to {}", profile.string());
			return;
		}

		std::cout << "[!] Added sl directory to PATH, restart your shell for it to take effect\n";
	}
}
