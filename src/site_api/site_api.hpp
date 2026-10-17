#pragma once

#include "../net/http.hpp"

// One SoundCloud track: the constructor reads its metadata, download() saves it.
class sc_upload
{
public:
	explicit sc_upload(const std::string& url);

	// False if reading the track failed (the reason has been logged)
	bool valid(void) const { return this->is_valid; }

	// Saves the audio (tagged .m4a) and/or the cover art, depending on the settings
	bool download(void);

private:
	Json post_data;
	bool is_valid = false;

	std::string stream_url; // api-v2 URL that returns the HLS playlist
	std::string artwork_url;
	mutable std::optional<std::string> artwork_cache; // fetched once, used by the cover file and the tag

	// Tag values
	std::string  title;
	std::string  file_title; // default file name, the full title even when -parse shortens the tag
	std::string  artist;
	std::string  album;
	std::string  album_artist;
	std::string  genre;
	std::string  comment;
	unsigned int year   = 0;
	unsigned int number = 0;

	bool load(const std::string& url);

	bool find_stream(void);
	bool read_artist(void);
	void read_title(void);
	void replace_artist(void);
	void read_album(void);
	void build_comment(void);
	bool read_misc(void);

	bool download_track(void);
	bool download_cover(void) const;
	bool download_segments(std::string& audio) const;

	const std::string* artwork_data(void) const;
	std::optional<TagLib::ByteVector> get_cover_art(void) const;
	void write_tags(const std::string& path, const std::string& name) const;
};

// Requests a fresh client ID from SoundCloud
bool get_client_id(void);
