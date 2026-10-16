#pragma once

#include "../net/http.hpp"
#include "../config/config.hpp"

struct sc_upload
{
private:

	//
	//// PRIVATE MEMBERS
	//

	Json post_data;

	std::string      streaming_url;
	std::vector<int> track_ids;

	std::string  art_src;
	std::string  artwork_url;
	std::string  artist_pfp_url;

	std::string  description;
	std::string  genre;
	std::string  album;
	std::string  artist;
	std::string  album_artist;
	std::string  title;
	std::string  file_title; // base for default file names, set before -parse changes the tag title
	unsigned int year = 0u;
	unsigned int num  = 0u;

	int id = 0;

	//
	//// PRIVATE METHODS
	//

	bool download_track(void);

	bool download_album(void);

	bool download_cover(void) const;

	bool parse_manifest(const std::string& raw_data, std::vector<std::shared_future<net::Response>>& buffer);

	bool get_streaming_url(void);

	bool get_track_ids(void);

	bool get_cover_art(net::Response& buffer) const;

	void store_basic_tag_data(TagLib::Tag* tag) const;

	void add_m4a_tag(const std::string& path) const;

	void get_misc_metadata(void);

	void create_comments(void);

	void get_album(void);

	void get_art_and_title(void);

	bool get_artist(void);

	void replace_artist(void);

	bool get_resolution_data(void);

public:

	//
	//// PUBLIC MEMBERS
	//

	struct
	{
		// 3/8 bits used

		uint8_t error_occured : 1; // An error occured in the constructor
		uint8_t is_track      : 1; // Object represents a singular track
		uint8_t is_album      : 1; // Object represents an album or playlist
	} f = {};

	//
	//// PUBLIC METHODS
	//

	sc_upload(std::string url);

	bool download(void)
	{
		if (cfg::f.seperate_art)
		{
			if (!this->download_cover())
			{
				return false;
			}

			if (cfg::f.no_audio_download)
			{
				std::cout << "WARNING: only downloaded cover art\n";
			}
		}

		if (cfg::f.no_audio_download)
		{
			return true;
		}

		return this->f.is_track ? this->download_track() : this->download_album();
	}
};

// Replaces < > : " / \ | ? * with underscores
std::string sanitize_filename(const std::string& name);

// Requests a fresh client ID from SoundCloud
bool get_client_id(void);