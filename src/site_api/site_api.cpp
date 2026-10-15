#include "pch.hpp"
#include "logs.hpp"
#include "../config/config.hpp"
#include "site_api.hpp"

#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

//
//// HELPER FUNCTIONS
//

static net::Response get_timed(const std::string& url)
{
	return net::get(url, 4500);
}

// Runs "ffmpeg -i input -map 0 -c copy -movflags +faststart -f ipod output". This only repackages
// the audio (no re-encoding, nothing is lost). The format is given explicitly because the file
// names used during a download don't end in ".m4a". Spawned directly instead of through a shell so
// file names never need quoting. Returns false if ffmpeg is missing or exits with an error.
static bool remux_with_ffmpeg(const std::string& input, const std::string& output)
{
	const char* const args[] =
	{
		"ffmpeg", "-nostdin", "-v", "error", "-y",
		"-i", input.c_str(),
		"-map", "0", "-c", "copy", "-movflags", "+faststart", "-f", "ipod",
		output.c_str(),
		nullptr
	};

	pid_t pid = 0;
	if (posix_spawnp(&pid, "ffmpeg", nullptr, nullptr, const_cast<char* const*>(args), environ) != 0)
	{
		return false; // ffmpeg not found (or could not be started)
	}

	int status = 0;
	if (waitpid(pid, &status, 0) < 0)
	{
		return false;
	}

	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// SoundCloud's AAC streams are HLS fragments. Joining them gives a "fragmented MP4" whose
// header says the duration is 0, so players such as Spotify show no track length. Remuxing
// writes a regular MP4 with the real duration. If anything goes wrong the original file is
// left exactly as it was, so a failed repair never costs you the download.
static void repair_fragmented_m4a(const std::string& path)
{
	namespace fs = std::filesystem;

	const fs::path repaired = path + ".repair";

	std::error_code ec;

	if (!remux_with_ffmpeg(path, repaired.string()))
	{
		err::warn("could not repair \"{}\" (is ffmpeg installed?). The track will play, but some players will not show its length", path);
		fs::remove(repaired, ec);
		return;
	}

	const std::uintmax_t size = fs::file_size(repaired, ec);
	if (ec || size == 0)
	{
		err::warn("ffmpeg produced an empty file while repairing \"{}\", keeping the original", path);
		fs::remove(repaired, ec);
		return;
	}

	fs::rename(repaired, path, ec); // replaces the original

	if (ec)
	{
		err::warn("could not replace \"{}\" with the repaired file: {}", path, ec.message());
		fs::remove(repaired, ec);
	}
}

// Splits "Artist - Song" into its two halves at the FIRST separator, which is a hyphen, en dash
// or em dash with a space on each side (so "Mid-Air" or "AC/DC" are never split). Everything after
// the separator is kept, e.g. "A - B - Remix" -> "A" and "B - Remix". Returns false (and leaves
// both strings untouched) if there is no separator or either half would be empty.
static bool split_artist_and_title(const std::string& full, std::string& artist, std::string& title)
{
	const std::string_view separators[] = { " - ", " \xE2\x80\x93 ", " \xE2\x80\x94 " };

	size_t sep_pos = std::string::npos;
	size_t sep_len = 0;

	for (const std::string_view sep : separators)
	{
		const size_t pos = full.find(sep);

		if (pos < sep_pos)
		{
			sep_pos = pos;
			sep_len = sep.size();
		}
	}

	if (sep_pos == std::string::npos)
	{
		return false;
	}

	auto trim = [](const std::string& text)
		{
			constexpr const char* whitespace = " \t\r\n";
			const size_t first = text.find_first_not_of(whitespace);
			if (first == std::string::npos) return std::string{};
			return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
		};

	const std::string left  = trim(full.substr(0, sep_pos));
	const std::string right = trim(full.substr(sep_pos + sep_len));

	if (left.empty() || right.empty())
	{
		return false;
	}

	artist = left;
	title  = right;
	return true;
}

bool get_client_id(void)
{
	auto handle_parsing_err = [](void)
		{
			if (cfg::client_id.empty())
			{
				err::log("failed to automatically resolve client ID, please provide one using the -cid command");
				return false;
			}
			else
			{
				err::warn("failed to resolve client ID, falling back to user provided ID");
				return true;
			}
		};

	const net::Response r = get_timed("https://soundcloud.com/");
	if (r.status_code != 200)
	{
		err::log_net(r);
		return false;
	}

	constexpr char id_string[] = "\"data\":{\"id\":\"";
	const size_t id_pos = r.text.find(id_string);
	if (id_pos == std::string::npos || r.text.find(id_string, id_pos + 1) != std::string::npos)
	{
		return handle_parsing_err();
	}

	const size_t offset = (id_pos + std::size(id_string)) - 1;
	const size_t end_pos = r.text.find('\"', offset);
	if (end_pos == std::string::npos)
	{
		return handle_parsing_err();
	}

	cfg::client_id = r.text.substr(offset, end_pos - offset);
	cfg::f.save_cid = 1; // CID is automatically saved to cfg.json to minimize the amount of requests that must be made in the future
	return true;
}

static void fix_artwork_url(std::string& url)
{
	// SoundCloud will typically store multiple versions of a song's artwork on their CDN. 
	// Interestingly, they even store uncropped high resolution versions of images, which 
	// is what we're adjusting the URL for.

	const size_t pos = url.find("large.");

	if (pos != std::string::npos)
	{
		url.replace(pos, std::size("large") - 1, "original");
	}
	else
	{
		err::warn("failed to locate original cover art, falling back to default artwork URL");
	}
}

std::string sanitize_filename(const std::string& name)
{
	return std::regex_replace(name, std::regex("[<>:\"/\\\\|?*]"), "_");
}

static bool read_cover_file(TagLib::File& target_file, TagLib::ByteVector& buffer)
{
	std::ifstream file(cfg::image_src, std::ios::binary | std::ios::ate);
	if (file.fail())
	{
		err::log("failed to open file \"{}\"", cfg::image_src);
		target_file.save();
		return false;
	}

	const size_t sz = file.tellg();
	buffer.resize(static_cast<unsigned int>(sz));

	file.seekg(0, std::ios::beg);

	file.read(buffer.data(), sz);
	file.close();
	return true;
}

static bool request_failed(net::Response& r)
{
	// This function will return false is the request succeeded. Otherwise it will attempt 
	// to get a fresh CID and retry the request with it. For that reason, this function 
	// should only be used on requests that included a CID.

	if (r.status_code == 200)
	{
		return false;
	}

	if (cfg::f.save_cid || !get_client_id())
	{
		err::log_net(r);
		return true;
	}

	std::string url = r.url;

	constexpr std::string_view cid_str = "client_id=";
	const size_t pos = url.find(cid_str);
	if (pos == std::string::npos)
	{
		err::log_net(r);
		return true;
	}

	url.erase(pos + cid_str.size());
	url += cfg::client_id;

	r = net::get(url, 4500);
	if (r.status_code != 200)
	{
		err::log_net(r);
		return true;
	}
	
	return false;
}

static bool resolve_post(std::string& url, Json& buffer)
{
	// Erasing tracking data from link

	const size_t pos = url.find('?');
	if (pos != std::string::npos) url.erase(pos);
	
	// Resolving post

	net::Response r = get_timed("https://api-v2.soundcloud.com/resolve?url=" + url + "&client_id=" + cfg::client_id);
	if (request_failed(r)) return false;

	buffer = Json::parse(r.text);
	return true;
}

//
//// TRACK/COVER DOWNLOADING
//

bool sc_upload::get_cover_art(net::Response& buffer) const
{
	std::string url;

	if (cfg::f.cover_src_sc_link)
	{
		Json post_data;
		if (!resolve_post(cfg::image_src, post_data))
		{
			return false;
		}

		url = post_data.value("artwork_url");
		if (url.empty())
		{
			return false;
		}

		fix_artwork_url(url);
	}
	else
	{
		url = this->art_src;
	}

	buffer = get_timed(url);
	if (cfg::f.cover_src_sc_link && request_failed(buffer))
	{
		return false;
	}

	if (buffer.status_code != 200)
	{
		err::log_net(buffer);
		return false;
	}

	if (buffer.text.size() > ~0U) // art size fields for id3v2/mp4 tags are 32-bits
	{
		err::warn("Cover art is larger than the max of ~4.29gb. This is likely unintended, please verify cover source.");
		return false;
	}

	return true;
}

static bool is_jpeg(const TagLib::ByteVector& image)
{
	return image.startsWith(TagLib::ByteVector("\xFF\xD8\xFF", 3));
}

void sc_upload::store_basic_tag_data(TagLib::Tag* tag) const
{
	// TagLib::String assumes Latin-1 for std::string, all of our strings are UTF-8

	tag->setAlbum(TagLib::String(this->album, TagLib::String::UTF8));

	tag->setTitle(TagLib::String(this->title, TagLib::String::UTF8));

	tag->setArtist(TagLib::String(this->artist, TagLib::String::UTF8));

	tag->setGenre(TagLib::String(this->genre, TagLib::String::UTF8));

	tag->setComment(TagLib::String(this->description, TagLib::String::UTF8));

	tag->setYear(this->year);

	if (this->num)
	{
		tag->setTrack(this->num);
	}
}

void sc_upload::add_m4a_tag(const std::string& path) const
{
	// Saving basic metadata

	TagLib::MP4::File file(path.c_str());
	TagLib::MP4::Tag* const tag = file.tag();

	this->store_basic_tag_data(tag);

	if (!this->album_artist.empty())
	{
		tag->setItem("aART", TagLib::StringList(TagLib::String(this->album_artist, TagLib::String::UTF8)));
	}

	// Getting cover art

	TagLib::ByteVector raw_image;

	if (cfg::f.cover_src_path)
	{
		if (!read_cover_file(file, raw_image))
		{
			return;
		}
	}
	else
	{
		net::Response r;
		if (!this->get_cover_art(r))
		{
			file.save();
			return;
		}

		raw_image = { r.text.data(), static_cast<unsigned int>(r.text.size()) };
	}

	TagLib::MP4::CoverArtList cover_list;
	cover_list.append({ is_jpeg(raw_image) ? TagLib::MP4::CoverArt::JPEG : TagLib::MP4::CoverArt::PNG, raw_image });
	tag->setItem("covr", TagLib::MP4::Item(cover_list));

	if (!file.save())
	{
		err::warn("failed to save MP4 tag");
	}
}

bool sc_upload::get_track_ids(void)
{
	const auto tracks = this->post_data.value("tracks", Json{});
	if (tracks.empty())
	{
		err::log("failed to parse track ID's");
		return false;
	}

	for (size_t i = tracks.size() - 1; i; --i)
	{
		this->track_ids.push_back(tracks[i].value("id", 0));
	}

	return true;
}

bool sc_upload::parse_manifest(const std::string& raw_data, std::vector<std::shared_future<net::Response>>& buffer)
{
	// This is used to download .m3u8 files, which contain an array of links that lead
	// to file segments, which you must append to eachother in the order of which the
	// links are provided. The first link (#EXT-X-MAP) is the MP4 header.

	std::istringstream iss(raw_data);
	std::string line;

	for (uint8_t active_threads = 0; std::getline(iss, line);)
	{
		if (line.starts_with("#EXT-X-MAP:URI="))
		{
			line.erase(0, line.find_first_of('\"') + 1);
			line.pop_back();
		}
		else if (line.starts_with("#EXTINF"))
		{
			std::getline(iss, line);
		}
		else continue;

		// We don't want more than 20 threads running concurrently

		if (active_threads >= 20)
		{
			// Check for completed threads

			for (auto& future : buffer)
			{
				if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
				{
					--active_threads;
				}
			}

			// If no threads have completed, wait on the first for 4500ms. If timed out, download fails.

			if (active_threads >= 20 && buffer[0].wait_for(std::chrono::milliseconds(4500)) != std::future_status::ready)
			{
				err::log("Base thread timed out with thread limit reached. Please open an issue on the repository and provide the track link (use the \"-?\" arg for link).");
				return false;
			}
		}

		buffer.emplace_back(net::get_async(line, 4500));
		++active_threads;
	}

	iss.clear();
	line.clear();

	// Give threads 5 seconds to return, sleep for 50ms between checks. 500ms 
	// higher than the 4500ms timeout that requests are given to account for 
	// potentially slow thread scheduling.

	bool active_thread = false;

	for (uint8_t i = 0; i < 100; std::this_thread::sleep_for(std::chrono::milliseconds(50)), ++i, active_thread = false)
	{
		for (auto& future : buffer)
		{
			const bool thread_waiting = future.wait_for(std::chrono::seconds(0)) != std::future_status::ready;

			if (!thread_waiting)
			{
				if (!future.valid())
				{
					throw std::future_error(std::future_errc::no_state);
				}

				const net::Response& r = future.get();

				if (r.status_code != 200)
				{
					err::log("request failed (code = {}, url = {})", r.status_code, r.url);
					return false;
				}
			}
			else 
			{
				active_thread = true;
			}
		}

		if (!active_thread) break;
	}

	if (active_thread)
	{
		err::log("threads timed out while attempting asynchronous HLS downloads (4500ms max), aborting download");
		return false;
	}

	return true;
}

bool sc_upload::get_streaming_url(void)
{
	// SoundCloud only serves AAC (as HLS) now. aac_96k is only used without aac_160k.

	auto list = this->post_data.value("media", Json{});
	if (list.empty())
	{
		err::log("'media' property not found in metadata");
		return false;
	}

	list = list.value("transcodings", Json{});
	if (list.empty())
	{
		err::log("'transcodings' property not found in metadata");
		return false;
	}

	for (const char* preset : { "aac_160k", "aac_96k" })
	{
		for (const auto& transcoding : list)
		{
			if (transcoding["preset"] == preset)
			{
				if (std::string_view(preset) == "aac_96k")
				{
					err::warn("only a 96 kbps AAC stream is available for this track");
				}

				this->streaming_url = transcoding["url"].get<std::string>() + "?client_id=" + cfg::client_id;
				return true;
			}
		}
	}

	err::log("failed to locate valid media transcoding");
	return false;
}

bool sc_upload::download_track(void)
{
	// Downloading track

	net::Response r = get_timed(this->streaming_url);
	if (request_failed(r)) return false;

	r = get_timed(Json::parse(r.text)["url"].get<std::string>());
	if (r.status_code != 200) return false;

	this->streaming_url.clear();
	
	// Finalizing download

	std::vector<std::shared_future<net::Response>> hls_parts;

	if (!this->parse_manifest(r.text, hls_parts))
	{
		return false;
	}

	// Writing track to disk

	const std::string& file_name = cfg::g_data.audio_name.empty() ? this->file_title : cfg::g_data.audio_name;
	const std::string name = sanitize_filename(file_name) + ".m4a";
	const std::string path = cfg::audio_dir + name;

	// The track is written, repaired and tagged under a hidden temporary name, then renamed into
	// place. Music players that watch the folder (Spotify does) read a file as soon as it shows
	// up and cache what they find, so they must never see it before its tags are written.
	const std::string temp_path = cfg::audio_dir + "." + name + ".part";

	std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
	if (file.fail())
	{
		err::log("failed to create file at \"{}\"", temp_path);
		return false;
	}

	bool failed = false;

	for (auto& future : hls_parts)
	{
		if (!future.valid())
		{
			throw std::future_error(std::future_errc::no_state);
		}

		const net::Response& r = future.get();
		file.write(r.text.c_str(), r.text.size());

		if (file.fail())
		{
			failed = true;
			break;
		}
	}

	file.close();
	hls_parts.clear();

	std::error_code ec;

	if (failed)
	{
		err::log("failed to write to file \"{}\"", temp_path);
		std::filesystem::remove(temp_path, ec);
		return false;
	}

	// Adding tag

	// Must happen before tagging: the repair rewrites the file, the tags are added to the result
	repair_fragmented_m4a(temp_path);
	this->add_m4a_tag(temp_path);

	std::filesystem::rename(temp_path, path, ec); // replaces an existing download of the track

	if (ec)
	{
		err::log("failed to move \"{}\" to \"{}\": {}", temp_path, path, ec.message());
		std::filesystem::remove(temp_path, ec);
		return false;
	}

	return true;
}

bool sc_upload::download_album(void)
{
	std::cout << "ERROR: album/playlist downloads arent actually implemented yet despite what the readme says cuz its not something i care abt lol. open an issue on the repo and ill add it for u!!\n";
	return false;
}

bool sc_upload::download_cover(void) const
{
	net::Response r = get_timed(this->artwork_url);
	if (r.status_code != 200) return false;

	const std::string& file_name = cfg::g_data.image_name.empty() ? this->file_title : cfg::g_data.image_name;
	const std::string path = cfg::image_dir + sanitize_filename(file_name) + ".jpg";

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (file.fail())
	{
		err::log("failed to open/create \"{}\"", path);
		return false;
	}

	file.write(r.text.data(), r.text.size());
	file.close();

	return true;
}

//
//// METADATA PARSING
//

void sc_upload::get_misc_metadata(void)
{
	// Getting genre

	if (cfg::g_data.genre.empty())
	{
		this->genre = post_data.value("genre");
	}
	else
	{
		this->genre = cfg::g_data.genre;
	}

	// Getting year

	if (cfg::g_data.year.empty())
	{
		try
		{
			this->year = std::stoul(post_data["created_at"].get<std::string>());
		}
		catch (...)
		{
			err::warn("failed to get upload year");
		}
	}
	else
	{
		try 
		{ 
			this->year = std::stoul(cfg::g_data.year); 
		}
		catch (...) 
		{
			err::log("invalid year provided");
			throw;
		}
	}

	// Getting track number and album artist

	if (!cfg::g_data.number.empty())
	{
		try
		{
			this->num = std::stoul(cfg::g_data.number);
		}
		catch (...)
		{
			err::log("invalid track number provided");
			this->f.error_occured = true;
		}
	}

	this->album_artist = cfg::g_data.album_artists;
}

void sc_upload::create_comments(void)
{
	// User requested comments

	this->description = cfg::g_data.comments;

	// Extra comments

	std::string temp;

	auto add_comment = [this, &temp](const char* label, const char* value)
		{
			temp = this->post_data.value(value);

			if (!temp.empty())
			{
				if (!this->description.empty())
				{
					this->description += "\n\n";
				}

				this->description += label + temp;

				temp.clear();
			}
		};

	add_comment("Upload date: ",          "created_at");
	add_comment("Original title: ",       "title");
	add_comment("Original description: ", "description");
	add_comment("Original tags: ",        "tag_list");
}

void sc_upload::get_album(void)
{
	// Ordering: cfg::g_track_data.album -> first existing album via SoundCloud API -> title

	if (cfg::g_data.album.empty())
	{
		net::Response r = get_timed("https://api-v2.soundcloud.com/tracks/" + std::to_string(post_data["id"].get<int>()) + "/albums?client_id=" + cfg::client_id);
		if (request_failed(r))
		{
			std::cout << "ignoring failure, continuing download\n";
		}
		else
		{
			const Json album_data = Json::parse(r.text)["collection"];

			if (!album_data.empty())
			{
				this->album = album_data[0].value("title");
			}
		}

		if (this->album.empty())
		{
			this->album = this->title;
		}
	}
	else
	{
		this->album = cfg::g_data.album;
	}
}

void sc_upload::get_art_and_title(void)
{
	// Getting title

	if (cfg::g_data.title.empty())
	{
		this->title = post_data.value("title");
	}
	else
	{
		this->title = cfg::g_data.title;
	}

	// File names are derived from the full track title, so "-parse" can shorten the tag
	// title without two songs called "Intro" ending up with the same file name.

	this->file_title = this->title;

	// Parsing "Artist - Title" (-parse). Anything given explicitly with -title / -artists wins.

	if (cfg::f.parse_title)
	{
		std::string parsed_artist;
		std::string parsed_title;

		if (!split_artist_and_title(post_data.value("title"), parsed_artist, parsed_title))
		{
			err::warn("-parse: no \"Artist - Title\" pattern in \"{}\", keeping the original title and artist", post_data.value("title"));
		}
		else
		{
			if (cfg::g_data.title.empty())
			{
				this->title = parsed_title;
			}

			if (cfg::g_data.contrib_artists.empty())
			{
				this->artist = parsed_artist;
			}

			std::cout << "parsed artist \"" << this->artist << "\", title \"" << this->title << "\"\n";
		}
	}

	// Getting artwork URL

	this->artwork_url = post_data.value("artwork_url", post_data.value("avatar_url"));
	if (!this->artwork_url.empty())
	{
		fix_artwork_url(this->artwork_url);
	}

	// Getting embedded artwork source

	if (cfg::image_src.empty())
	{
		this->art_src = this->artwork_url;
	}
	else
	{
		this->art_src = cfg::image_src;
	}
}

bool sc_upload::get_artist(void)
{
	if (cfg::g_data.contrib_artists.empty())
	{
		std::string mb_artist;

		if ((mb_artist = post_data.value("publisher_metadata", Json{}).value("artist")).empty()
			&& (mb_artist = post_data.value("user", Json{}).value("username")).empty())
		{
			err::log("failed to get publisher_metadata.artist or user.username");
			this->f.error_occured = true;
			return false;
		}

		this->artist = mb_artist;
	}
	else
	{
		this->artist = cfg::g_data.contrib_artists;
	}

	return true;
}

bool sc_upload::get_resolution_data(void)
{
	// Getting upload type, track ID(s), and streaming url (if applicable)

	const std::string kind = this->post_data.value("kind");
	if (kind.empty())
	{
		err::log("failed to get 'kind' field from metadata");
		this->f.error_occured = true;
		return false;
	}

	if (kind[0] == 't')
	{
		this->id = this->post_data.value("id", 0);
		this->f.is_track = true;

		if (!cfg::cover_art_only() && !this->get_streaming_url()) // cover art only downloads don't need a stream
		{
			this->f.error_occured = true;
		}
	}
	else
	{
		this->f.is_album = true;

		if (!this->get_track_ids())
		{
			this->f.error_occured = true;
		}
	}

	return true;
}

sc_upload::sc_upload(std::string url)
{
	if (!resolve_post(url, this->post_data))
	{
		this->f.error_occured = true;
		return;
	}

	if (!this->get_resolution_data() || !this->get_artist())
	{
		return;
	}

	this->get_art_and_title();

	if (cfg::cover_art_only()) 
	{
		return;
	}

	this->get_album();
	this->create_comments();
	this->get_misc_metadata();
}