#include "pch.hpp"
#include "logs.hpp"
#include "util.hpp"
#include "config.hpp"

#include <strings.h>

namespace cfg
{
	namespace fs = std::filesystem;

	static cfg_format            saved;                // contents of cfg.json
	static fs::path              program_dir;          // directory sl is in
	static fs::path              config_path;          // cfg.json
	static std::vector<std::string> removed_replacements; // -n-replace

	//
	//// ARGUMENTS
	//

	// Arguments without a value set a flag, arguments with one store it in a string

	struct flag_arg  { const char* name; const char* alias; bool* target; };
	struct value_arg { const char* name; const char* alias; std::string* target; };

	static const flag_arg flag_args[] =
	{
		{ "save",    "s", &f.save_cfg          },
		{ "art",     "",  &f.separate_art      },
		{ "n-art",   "",  &f.no_separate_art   },
		{ "audio",   "",  &f.download_audio    },
		{ "n-audio", "",  &f.no_audio_download },
		{ "pvars",   "",  &f.add_to_path       },
		{ "xdg",     "",  &f.use_xdg           },
		{ "undo",    "",  &f.undo              },
		{ "parse",   "p", &f.parse_title       },
	};

	static const value_arg value_args[] =
	{
		{ "audio-name", "aname", &overrides.audio_name   },
		{ "img-name",   "iname", &overrides.image_name   },
		{ "audio-dst",  "adst",  &audio_dir              },
		{ "img-dst",    "idst",  &image_dir              },
		{ "img-src",    "isrc",  &image_src              },
		{ "title",      "t",     &overrides.title        },
		{ "comment",    "c",     &overrides.comment      },
		{ "artists",    "a",     &overrides.artists      },
		{ "a-artist",   "aa",    &overrides.album_artist },
		{ "album",      "al",    &overrides.album        },
		{ "genre",      "g",     &overrides.genre        },
		{ "num",        "n",     &overrides.number       },
		{ "year",       "y",     &overrides.year         },
		{ "cid",        "",      &client_id              },
	};

	static bool matches(const std::string_view name, const char* arg_name, const char* alias)
	{
		auto equals = [name](const char* candidate)
			{
				return *candidate && std::strlen(candidate) == name.size() && strncasecmp(candidate, name.data(), name.size()) == 0;
			};

		return equals(arg_name) || equals(alias);
	}

	void show_usage(void)
	{
		std::cout <<
R"([*] SoundLoad (sl) - downloads SoundCloud tracks with their metadata and cover art
Usage: sl <track link> [options]   download a track
       sl [options]                only change saved settings (with -s)
Quote links and values with spaces or shell characters (? & * ; < > |) in them.
Options ignore case. [S] = saved to cfg.json when -s is given.
Documentation: https://github.com/user28047284/SoundLoad-Linux

[+] GENERAL
  -s, -save               save the [S] options of this command
  -audio / -n-audio       [S] download the audio (default) / don't
  -art / -n-art           [S] also save the cover art as an image file / don't (default)
  -cid <id>               [S] SoundCloud client ID (normally found automatically)
  -undo                   list your last 10 downloads and delete the ones you pick
  -pvars                  show how to add sl to PATH (asks before changing anything)
  -xdg                    keep cfg.json in ~/.config/soundload instead of next to sl
  -?                      show this help

[+] FILES
  -adst, -audio-dst <dir>     [S] folder for audio files
  -idst, -img-dst <dir>       [S] folder for cover art images
  -aname, -audio-name <name>  audio file name (default: track title)
  -iname, -img-name <name>    cover art file name (default: track title)
  -isrc, -img-src <source>    cover art to embed: image file, image link or SoundCloud track link

[+] TAGS
  -t, -title <title>          title (default: track title)
  -a, -artists <artists>      artist (default: the track's artist, or its uploader)
  -aa, -a-artist <artist>     album artist
  -al, -album <album>         album (default: the track's album, or its title)
  -g, -genre <genre>          genre (default: the track's genre)
  -n, -num <number>           track number
  -y, -year <year>            year (default: upload year)
  -c, -comment <text>         comment (upload date, title, description and tags are added after it)
  -p, -parse                  split "Artist - Title" track names into artist and title
  -replace <from> <to>        [S] replace an artist name in the artist tag (whole names, ignores case)
  -n-replace <from>           [S] remove a saved -replace
)";
	}

	bool parse_arguments(const int argc, const char* const argv[])
	{
		if (argc < 2)
		{
			err::log("no arguments given, use '-?' for program usage");
			return false;
		}

		int i = 1;

		if (strncasecmp(argv[1], "https://soundcloud.com/", 23) == 0)
		{
			link = argv[1];
			i = 2;
		}

		for (; i < argc; ++i)
		{
			const std::string_view arg = argv[i];

			auto has_values = [&](const int count)
				{
					if (i + count < argc) return true;

					if (count == 1) err::log("no value provided for argument \"{}\"", arg);
					else            err::log("argument \"{}\" needs {} values", arg, count);

					return false;
				};

			if (arg == "-?")
			{
				f.show_help = true;
				continue;
			}

			if (arg.size() < 2 || arg[0] != '-')
			{
				err::log("invalid argument \"{}\"", arg);
				return false;
			}

			const std::string_view name = arg.substr(1);

			if (const auto it = std::ranges::find_if(flag_args, [name](const flag_arg& a) { return matches(name, a.name, a.alias); });
				it != std::end(flag_args))
			{
				*it->target = true;
				continue;
			}

			if (const auto it = std::ranges::find_if(value_args, [name](const value_arg& a) { return matches(name, a.name, a.alias); });
				it != std::end(value_args))
			{
				if (!has_values(1)) return false;
				*it->target = argv[++i];
				continue;
			}

			if (matches(name, "replace", ""))
			{
				if (!has_values(2)) return false;

				const std::string from = util::lowercase(argv[i + 1]);
				if (from.empty())
				{
					err::log("-replace: the artist name to replace can't be empty");
					return false;
				}

				artist_replacements[from] = argv[i + 2];
				i += 2;
				continue;
			}

			if (matches(name, "n-replace", ""))
			{
				if (!has_values(1)) return false;

				const std::string from = util::lowercase(argv[++i]);
				removed_replacements.push_back(from);
				artist_replacements.erase(from);
				continue;
			}

			err::log("invalid argument \"{}\"", arg);
			return false;
		}

		// Cover art source type

		if (!image_src.empty())
		{
			f.cover_src_path    = fs::exists(image_src);
			f.cover_src_sc_link = !f.cover_src_path && image_src.starts_with("https://soundcloud.com/");
		}

		std::error_code ec;
		program_dir = fs::read_symlink("/proc/self/exe", ec).parent_path();

		if (ec)
		{
			err::log("failed to get sl path ({})", ec.message());
			return false;
		}

		return true;
	}

	//
	//// CFG.JSON
	//

	// cfg.json next to sl, or in the XDG config directory with -xdg or if it already exists there
	static fs::path config_file_path(void)
	{
		const fs::path dir = util::xdg_dir("XDG_CONFIG_HOME", ".config");

		if (!dir.empty())
		{
			if (fs::exists(dir / "cfg.json"))
			{
				return dir / "cfg.json";
			}

			std::error_code ec;
			if (f.use_xdg && (fs::create_directories(dir, ec), !ec))
			{
				return dir / "cfg.json";
			}
		}

		return program_dir / "cfg.json";
	}

	static void add_trailing_slash(std::string& dir)
	{
		if (!dir.empty() && dir.back() != '/')
		{
			dir.push_back('/');
		}
	}

	bool load_config(void)
	{
		config_path = config_file_path();

		if (fs::exists(config_path))
		{
			try
			{
				std::ifstream file(config_path);
				saved = Json::parse(file).get<cfg_format>();
			}
			catch (...)
			{
				err::log("failed to read \"{}\"", config_path.string());
				return false;
			}

			// Command line arguments win over saved values

			if (client_id.empty()) client_id = saved.cid;
			if (image_dir.empty()) image_dir = saved.art_out_dir;
			if (audio_dir.empty()) audio_dir = saved.track_out_dir;

			if (!f.download_audio && !f.no_audio_download && !saved.get_track_audio)
			{
				f.no_audio_download = true;
			}

			if (!f.separate_art && !f.no_separate_art && saved.get_track_art)
			{
				f.separate_art = true;
			}

			for (const auto& [from, to] : saved.artist_replacements)
			{
				const std::string key = util::lowercase(from);

				if (std::ranges::find(removed_replacements, key) == removed_replacements.end())
				{
					artist_replacements.try_emplace(key, to);
				}
			}
		}
		else
		{
			f.fresh_cfg = true;
		}

		add_trailing_slash(audio_dir);
		add_trailing_slash(image_dir);
		return true;
	}

	bool save_config(void)
	{
		if (f.save_cfg)
		{
			if (!client_id.empty()) saved.cid           = client_id;
			if (!image_dir.empty()) saved.art_out_dir   = image_dir;
			if (!audio_dir.empty()) saved.track_out_dir = audio_dir;

			if (f.download_audio || f.no_audio_download) saved.get_track_audio = f.download_audio;
			if (f.separate_art || f.no_separate_art)     saved.get_track_art   = f.separate_art;

			saved.artist_replacements = artist_replacements; // already merged, minus -n-replace
		}
		else if (f.save_cid)
		{
			saved.cid = client_id;
		}
		else
		{
			return true;
		}

		std::ofstream file(config_path, std::ios::trunc);
		file << Json(saved).dump();

		if (file.fail())
		{
			err::log("failed to save \"{}\"", config_path.string());
			return false;
		}

		return true;
	}

	//
	//// PATH
	//

	void add_to_path(void)
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
}
