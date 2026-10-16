#pragma once

// cfg.json. Missing keys keep their defaults and unknown keys (such as the removed AAC options)
// are ignored, so config files from older versions still load.
struct cfg_format
{
	std::string cid;
	std::string art_out_dir;
	std::string track_out_dir;
	bool        get_track_audio = true;
	bool        get_track_art   = false;
	std::map<std::string, std::string> artist_replacements; // -replace, keys are lowercase
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(cfg_format, cid, art_out_dir, track_out_dir, get_track_audio, get_track_art, artist_replacements)

namespace cfg
{
	//
	//// SETTINGS (all strings are UTF-8)
	//

	struct flags_t
	{
		bool save_cfg          = false; // -s: save the saveable arguments to cfg.json
		bool save_cid          = false; // a client ID was resolved automatically, saved even without -s
		bool separate_art      = false; // [saveable] -art: also save the cover art as its own file
		bool no_separate_art   = false; // [saveable] -n-art
		bool download_audio    = false; // [saveable] -audio (the default, overrides a saved -n-audio)
		bool no_audio_download = false; // [saveable] -n-audio
		bool add_to_path       = false; // -pvars
		bool use_xdg           = false; // -xdg: keep cfg.json in the XDG config directory
		bool parse_title       = false; // -p: split "Artist - Title" track names
		bool undo              = false; // -undo
		bool show_help         = false; // -?
		bool show_version      = false; // -version
		bool cover_src_path    = false; // -isrc is an image file
		bool cover_src_sc_link = false; // -isrc is a SoundCloud link (its cover art is used)
		bool fresh_cfg         = false; // there was no cfg.json yet
	} inline f;

	// Values given on the command line that replace what is read from SoundCloud
	struct overrides_t
	{
		std::string audio_name; // file name for the audio (default: track title)
		std::string image_name; // file name for the cover art (default: track title)
		std::string title;
		std::string comment;
		std::string artists;
		std::string album_artist;
		std::string album;
		std::string genre;
		std::string number; // track number
		std::string year;
	} inline overrides;

	inline std::string link;      // the SoundCloud link to download, empty when only changing settings
	inline std::string client_id; // SoundCloud client ID
	inline std::string audio_dir; // audio download directory, ends with '/' unless empty
	inline std::string image_dir; // cover art download directory, ends with '/' unless empty
	inline std::string image_src; // -isrc: image path, image link or SoundCloud link

	// Artist names to replace in the artist tag (-replace). Keys are lowercase, matching ignores case.
	inline std::map<std::string, std::string> artist_replacements;

	//
	//// FUNCTIONS
	//

	// args[0] is the program, all strings are UTF-8
	bool parse_arguments(const std::vector<std::string>& args);

	void show_usage(void);

	// Reads cfg.json (if it exists) and fills in everything not given on the command line
	bool load_config(void);

	// Writes cfg.json with -s, or when a client ID was resolved automatically
	bool save_config(void);

	// Shows how to add sl's directory to PATH and offers to do it
	void add_to_path(void);

	inline bool cover_art_only(void)
	{
		return f.separate_art && f.no_audio_download;
	}
}
