#pragma once

// TYPES

struct cfg_format
{
	std::string cid;
	std::string art_out_dir;
	std::string track_out_dir;
	bool get_track_audio     = true;
	bool get_track_art       = false;
	bool get_aac_transcoding = false;
	bool aac_fallback        = false;
};

// Missing keys keep their default values so config files from older versions still load
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(cfg_format, cid, art_out_dir, track_out_dir, get_track_audio, get_track_art, get_aac_transcoding, aac_fallback)

// CONFIG DATA

namespace cfg
{
	//
	//// GLOBAL CONFIG VARIABLES
	//

	struct cfg_flags_t
	{
		// 16/16 bits used

		uint16_t add_to_path       : 1; // Adds program to PATH variables
		uint16_t save_cfg          : 1; // Saves applicable arguments to config (set when -save is used)
		uint16_t save_cid          : 1; // Saves the client ID to config. Set when a client ID is automatically resolved.
		uint16_t seperate_art      : 1; // [saveable] downloads track's cover art to its own file
		uint16_t no_seperate_art   : 1; // [saveable] [default] disables the seperate_art flag if it was saved to config
		uint16_t download_audio    : 1; // [saveable] [default] reenables audio downloads if disabled by no_audio_download in config
		uint16_t no_audio_download : 1; // [saveable] disables audio downloads
		uint16_t use_aac           : 1; // [saveable] Downloads lossless m4a media transcoding
		uint16_t no_aac            : 1; // [saveable] [default] Disables lessless m4a media transcoding downloads
		uint16_t no_link_provided  : 1; // Indicates that no link was provided - used when modifying config without downloading anything
		uint16_t cover_src_path    : 1; // Provided source for audio cover art is an image path on the computer
		uint16_t cover_src_sc_link : 1; // Provided source for audio cover art is a SoundCloud link (uses cover art from song/profile)
		uint16_t fresh_cfg         : 1; // cfg.json was just created
		uint16_t use_aac_fallback  : 1; // [saveable] Downloads AAC when a track has no MPEG transcoding
		uint16_t no_aac_fallback   : 1; // [saveable] [default] Fails when a track has no MPEG transcoding
		uint16_t use_xdg           : 1; // Stores cfg.json in the XDG config directory
	} inline f = {};

	struct track_data_t
	{
		// Output data

		std::string audio_name; // Name for downloaded audio. Derived from track title by default.
		std::string image_name; // Name for downloaded cover art. Derived from track title by default.

		// Audio metadata

		std::string title;
		std::string comments;
		std::string contrib_artists; // contributing artists
		std::string album_artists;   // album artist
		std::string album;
		std::string genre;
		std::string number; // track number (for albums)
		std::string year;
	} inline g_data = {};

	// All strings are UTF-8

	inline std::string client_id; // SoundCloud client ID
	inline std::string audio_dir; // Audio download directory
	inline std::string image_dir; // Cover art download directory
	inline std::string image_src; // Audio cover art source (file path, soundcloud track link, or image link)

	inline std::filesystem::path path_cache; // Stores the directory of sl or the path of cfg.json

	//
	//// FORWARD DECLARATIONS
	//

	bool parse_arguments(int argc, const char* const argv[]);

	void read_config(const cfg_format& data);

	void save_config(cfg_format& data);

	// cfg.json in the program directory (path_cache), or in the XDG config directory with -xdg or if it already exists there
	std::filesystem::path config_file_path(void);

	void add_to_path(const std::filesystem::path& program_dir);

	//
	//// HELPERS
	//

	inline bool audio_flags_set(void)
	{
		return cfg::f.download_audio || cfg::f.no_audio_download;
	}

	inline bool art_flags_set(void)
	{
		return cfg::f.seperate_art || cfg::f.no_seperate_art;
	}

	inline bool aac_flags_set(void)
	{
		return cfg::f.use_aac || cfg::f.no_aac;
	}

	inline bool aac_fallback_flags_set(void)
	{
		return cfg::f.use_aac_fallback || cfg::f.no_aac_fallback;
	}

	inline bool cover_art_only(void)
	{
		return cfg::f.seperate_art && cfg::f.no_audio_download;
	}
}
