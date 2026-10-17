#include "pch.hpp"
#include "logs.hpp"
#include "util.hpp"
#include "../config/config.hpp"
#include "../history/history.hpp"
#include "../media/m4a.hpp"
#include "site_api.hpp"

namespace fs = std::filesystem;

//
//// REQUESTS
//

static net::Response get_timed(const std::string& url)
{
	return net::get(url, 4500);
}

bool get_client_id(void)
{
	// Keeps an already set client ID (-cid or cfg.json) if parsing fails

	auto parsing_failed = [](void)
		{
			if (cfg::client_id.empty())
			{
				err::log("failed to automatically resolve client ID, please provide one using the -cid command");
				return false;
			}

			err::warn("failed to resolve client ID, falling back to user provided ID");
			return true;
		};

	const net::Response r = get_timed("https://soundcloud.com/");
	if (r.status_code != 200)
	{
		err::log_net(r);
		return false;
	}

	constexpr std::string_view marker = "\"data\":{\"id\":\"";
	const size_t marker_pos = r.text.find(marker);

	if (marker_pos == std::string::npos || r.text.find(marker, marker_pos + 1) != std::string::npos)
	{
		return parsing_failed();
	}

	const size_t start = marker_pos + marker.size();
	const size_t end   = r.text.find('"', start);

	if (end == std::string::npos)
	{
		return parsing_failed();
	}

	cfg::client_id  = r.text.substr(start, end - start);
	cfg::f.save_cid = true; // saved to cfg.json so it doesn't have to be fetched every time
	return true;
}

// GETs an api-v2 URL with the client ID. On failure, fetches a fresh client ID (once per run) and retries.
static net::Response api_get(const std::string& url)
{
	auto with_client_id = [&url](void)
		{
			return url + (url.find('?') == std::string::npos ? '?' : '&') + "client_id=" + cfg::client_id;
		};

	net::Response r = get_timed(with_client_id());

	if (r.status_code != 200 && !cfg::f.save_cid && get_client_id())
	{
		r = get_timed(with_client_id());
	}

	if (r.status_code != 200)
	{
		err::log_net(r);
	}

	return r;
}

// api_get and parse the response as JSON. Returns a discarded value on failure.
static Json api_get_json(const std::string& url)
{
	const net::Response r = api_get(url);
	if (r.status_code != 200)
	{
		return Json(Json::value_t::discarded);
	}

	Json json = Json::parse(r.text, nullptr, false);

	if (json.is_discarded())
	{
		err::log("SoundCloud sent an invalid response for \"{}\"", url);
	}

	return json;
}

// Resolves a soundcloud.com link (without tracking parameters) to its API data
static Json resolve_post(std::string url)
{
	url.erase(std::min(url.find('?'), url.size()));
	return api_get_json("https://api-v2.soundcloud.com/resolve?url=" + url);
}

// parent[key] if it is an object, else an empty one (SoundCloud sends some as null, which breaks value())
static Json get_object(const Json& parent, const char* key)
{
	const auto it = parent.find(key);
	return it != parent.end() && it->is_object() ? *it : Json::object();
}

//
//// COVER ART
//

// Artwork URLs point to a small version; tries "-original." first, then smaller ones (not every image has one)
static net::Response get_artwork(const std::string& url)
{
	constexpr std::string_view large = "-large.";
	const size_t pos = url.find(large);

	if (pos == std::string::npos)
	{
		return get_timed(url); // not a SoundCloud artwork URL (e.g. -isrc with an image link)
	}

	net::Response r;

	for (const std::string_view size : { "-original.", "-t1080x1080.", "-t500x500.", "-large." })
	{
		r = get_timed(std::string(url).replace(pos, large.size(), size));

		if (r.status_code != 404)
		{
			break;
		}
	}

	return r;
}

// The track's artwork, or the uploader's avatar if it has none. Empty if neither.
static std::string get_artwork_url(const Json& post_data)
{
	std::string url = post_data.value("artwork_url");
	return url.empty() ? get_object(post_data, "user").value("avatar_url") : url;
}

static bool fits_in_tag(const size_t size)
{
	// TagLib::ByteVector sizes are unsigned int

	if (size > std::numeric_limits<unsigned int>::max())
	{
		err::warn("the cover art is larger than 4 GB, please check the cover source");
		return false;
	}

	return true;
}

std::optional<TagLib::ByteVector> sc_upload::get_cover_art(void) const
{
	if (cfg::f.cover_src_path)
	{
		std::ifstream file(util::to_path(cfg::image_src), std::ios::binary);
		const std::string data{ std::istreambuf_iterator<char>(file), {} };

		if (file.bad() || data.empty())
		{
			err::log("failed to read \"{}\"", cfg::image_src);
			return std::nullopt;
		}

		if (!fits_in_tag(data.size())) return std::nullopt;
		return TagLib::ByteVector(data.data(), static_cast<unsigned int>(data.size()));
	}

	if (cfg::image_src.empty() && this->artwork_url.empty())
	{
		err::warn("no cover art found, saving the track without one");
		return std::nullopt;
	}

	net::Response linked;
	const std::string* data = nullptr;

	if (cfg::image_src.empty())
	{
		data = this->artwork_data();
	}
	else
	{
		linked = get_artwork(cfg::image_src);

		if (linked.status_code == 200) data = &linked.text;
		else                           err::log_net(linked);
	}

	if (!data || !fits_in_tag(data->size())) return std::nullopt;
	return TagLib::ByteVector(data->data(), static_cast<unsigned int>(data->size()));
}

const std::string* sc_upload::artwork_data(void) const
{
	if (!this->artwork_cache)
	{
		const net::Response r = get_artwork(this->artwork_url);

		if (r.status_code != 200)
		{
			err::log_net(r);
			return nullptr;
		}

		this->artwork_cache = r.text;
	}

	return &*this->artwork_cache;
}

//
//// METADATA
//

// Parses a whole string, or with prefix_only just the number at its start ("2019" in "2019-10-24")
static std::optional<unsigned int> to_uint(const std::string_view text, const bool prefix_only = false)
{
	unsigned int value = 0;
	const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);

	if (ec != std::errc{} || (!prefix_only && end != text.data() + text.size()))
	{
		return std::nullopt;
	}

	return value;
}

// Splits "Artist - Song" at the first " - " (hyphen, en or em dash with spaces), so "Mid-Air" stays whole.
// Returns false and leaves both strings untouched if there is no separator or a half is empty.
static bool split_artist_and_title(const std::string& full, std::string& artist, std::string& title)
{
	constexpr std::string_view separators[] = { " - ", " \xE2\x80\x93 ", " \xE2\x80\x94 " };

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

	auto trim = [](const std::string_view text)
		{
			constexpr std::string_view whitespace = " \t\r\n";
			const size_t first = text.find_first_not_of(whitespace);
			return first == std::string_view::npos ? std::string{} : std::string(text.substr(first, text.find_last_not_of(whitespace) - first + 1));
		};

	const std::string left  = trim(std::string_view(full).substr(0, sep_pos));
	const std::string right = trim(std::string_view(full).substr(sep_pos + sep_len));

	if (left.empty() || right.empty())
	{
		return false;
	}

	artist = left;
	title  = right;
	return true;
}

static bool is_word_char(const char ch)
{
	const unsigned char c = static_cast<unsigned char>(ch);
	return c >= 0x80 || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Applies -replace to whole names only, ignoring case: "-replace jay j" turns "4jay x jay" into "4jay x j"
static std::string replace_artist_names(std::string artist)
{
	for (const auto& [from, to] : cfg::artist_replacements)
	{
		std::string lower = util::lowercase(artist); // same byte positions as artist

		for (size_t pos = 0; (pos = lower.find(from, pos)) != std::string::npos;)
		{
			const size_t end = pos + from.size();
			const bool whole_name = (pos == 0 || !is_word_char(lower[pos - 1]))
				&& (end == lower.size() || !is_word_char(lower[end]));

			if (!whole_name)
			{
				++pos;
				continue;
			}

			artist.replace(pos, from.size(), to);
			lower.replace(pos, from.size(), util::lowercase(to));
			pos += to.size();
		}
	}

	return artist;
}

bool sc_upload::find_stream(void)
{
	// AAC over HLS only. aac_96k is the fallback, encrypted streams are skipped.

	const Json transcodings = get_object(this->post_data, "media").value("transcodings", Json::array());

	if (!transcodings.is_array() || transcodings.empty())
	{
		err::log("this track has no streams");
		return false;
	}

	bool encrypted_only = false;

	for (const std::string_view preset : { "aac_160k", "aac_96k" })
	{
		for (const Json& transcoding : transcodings)
		{
			if (transcoding.value("preset") != preset)
			{
				continue;
			}

			if (get_object(transcoding, "format").value("protocol") != "hls")
			{
				encrypted_only = true;
				continue;
			}

			if (preset == "aac_96k")
			{
				err::warn("only a 96 kbps AAC stream is available for this track");
			}

			this->stream_url = transcoding.value("url");
			return !this->stream_url.empty();
		}
	}

	if (encrypted_only)
	{
		err::log("this track is only available as an encrypted (DRM protected) stream and can't be downloaded");
	}
	else
	{
		err::log("failed to locate valid media transcoding");
	}

	return false;
}

bool sc_upload::read_artist(void)
{
	if (!cfg::overrides.artists.empty())
	{
		this->artist = cfg::overrides.artists;
		return true;
	}

	this->artist = get_object(this->post_data, "publisher_metadata").value("artist");

	if (this->artist.empty())
	{
		this->artist = get_object(this->post_data, "user").value("username");
	}

	if (this->artist.empty())
	{
		err::log("failed to get publisher_metadata.artist or user.username");
		return false;
	}

	return true;
}

void sc_upload::read_title(void)
{
	const std::string original = this->post_data.value("title");

	this->title = cfg::overrides.title.empty() ? original : cfg::overrides.title;

	// The file name keeps the full title even when -parse shortens the tag title

	this->file_title = this->title;

	// -parse, but -title and -artists win

	if (!cfg::f.parse_title)
	{
		return;
	}

	std::string parsed_artist, parsed_title;

	if (!split_artist_and_title(original, parsed_artist, parsed_title))
	{
		err::warn("-parse: no \"Artist - Title\" pattern in \"{}\", keeping the original title and artist", original);
		return;
	}

	if (cfg::overrides.title.empty())   this->title  = parsed_title;
	if (cfg::overrides.artists.empty()) this->artist = parsed_artist;

	std::cout << "parsed artist \"" << this->artist << "\", title \"" << this->title << "\"\n";
}

void sc_upload::replace_artist(void)
{
	// After -parse so parsed artists are replaced too; -artists is kept as given

	if (!cfg::overrides.artists.empty())
	{
		return;
	}

	const std::string replaced = replace_artist_names(this->artist);

	if (replaced != this->artist)
	{
		std::cout << "replaced artist \"" << this->artist << "\" with \"" << replaced << "\"\n";
		this->artist = replaced;
	}
}

void sc_upload::read_album(void)
{
	// -album, then the first album the track is in, then the track title

	if (!cfg::overrides.album.empty())
	{
		this->album = cfg::overrides.album;
		return;
	}

	const std::uint64_t id = this->post_data.value("id", std::uint64_t{ 0 });
	const Json albums = api_get_json("https://api-v2.soundcloud.com/tracks/" + std::to_string(id) + "/albums");

	if (albums.is_discarded())
	{
		err::warn("could not look up the track's album, using its title");
	}
	else if (const Json collection = albums.value("collection", Json::array());
		collection.is_array() && !collection.empty() && collection.front().is_object())
	{
		this->album = collection.front().value("title");
	}

	if (this->album.empty())
	{
		this->album = this->title;
	}
}

void sc_upload::build_comment(void)
{
	// -comment first, then details from SoundCloud that would otherwise be lost

	this->comment = cfg::overrides.comment;

	auto add = [this](const std::string_view label, const char* key)
		{
			const std::string value = this->post_data.value(key);

			if (!value.empty())
			{
				if (!this->comment.empty()) this->comment += "\n\n";
				this->comment += std::string(label) + value;
			}
		};

	add("Upload date: ",          "created_at");
	add("Original title: ",       "title");
	add("Original description: ", "description");
	add("Original tags: ",        "tag_list");
}

bool sc_upload::read_misc(void)
{
	this->genre        = cfg::overrides.genre.empty() ? this->post_data.value("genre") : cfg::overrides.genre;
	this->album_artist = cfg::overrides.album_artist;

	if (!cfg::overrides.year.empty())
	{
		const auto year = to_uint(cfg::overrides.year);
		if (!year)
		{
			err::log("invalid year \"{}\"", cfg::overrides.year);
			return false;
		}

		this->year = *year;
	}
	else if (const auto year = to_uint(this->post_data.value("created_at"), true))
	{
		this->year = *year;
	}
	else
	{
		err::warn("failed to get upload year");
	}

	if (!cfg::overrides.number.empty())
	{
		const auto number = to_uint(cfg::overrides.number);
		if (!number)
		{
			err::log("invalid track number \"{}\"", cfg::overrides.number);
			return false;
		}

		this->number = *number;
	}

	return true;
}

sc_upload::sc_upload(const std::string& url)
{
	this->is_valid = this->load(url);
}

bool sc_upload::load(const std::string& url)
{
	this->post_data = resolve_post(url);

	if (this->post_data.is_discarded())
	{
		return false;
	}

	const std::string kind = this->post_data.value("kind");

	if (kind != "track")
	{
		err::log("only links to single tracks are supported{}", kind.empty() ? "" : " (this link is a " + kind + ")");
		return false;
	}

	if (!cfg::cover_art_only() && !this->find_stream()) // cover art only downloads don't need a stream
	{
		return false;
	}

	if (!this->read_artist())
	{
		return false;
	}

	this->read_title();
	this->replace_artist();
	this->artwork_url = get_artwork_url(this->post_data);

	if (cfg::cover_art_only())
	{
		return true;
	}

	this->read_album();
	this->build_comment();
	return this->read_misc();
}

//
//// DOWNLOADING
//

bool sc_upload::download(void)
{
	if (cfg::f.separate_art && !this->download_cover())
	{
		return false;
	}

	if (cfg::f.no_audio_download)
	{
		if (cfg::f.separate_art) err::warn("only downloaded cover art");
		return true;
	}

	return this->download_track();
}

// Asks before replacing an existing file. Always no when there is no terminal to ask.
static bool confirm_overwrite(const std::string& name)
{
	err::warn("\"{}\" already exists in the download folder", name);

	if (!util::ask_yes_no("Download it again and replace it?"))
	{
		std::cout << "not downloading it again\n";
		return false;
	}

	return true;
}

bool sc_upload::download_segments(std::string& audio) const
{
	// The stream URL gives an HLS playlist: the MP4 header (#EXT-X-MAP), then one fragment per #EXTINF.

	const Json stream = api_get_json(this->stream_url);
	const std::string playlist_url = stream.is_discarded() ? "" : stream.value("url");

	if (playlist_url.empty())
	{
		return false;
	}

	const net::Response playlist = get_timed(playlist_url);
	if (playlist.status_code != 200)
	{
		err::log_net(playlist);
		return false;
	}

	std::vector<std::string> urls;
	std::istringstream iss(playlist.text);

	for (std::string line; std::getline(iss, line);)
	{
		if (!line.empty() && line.back() == '\r') line.pop_back();

		if (line.starts_with("#EXT-X-MAP:"))
		{
			const size_t start = line.find("URI=\"");
			const size_t end   = start == std::string::npos ? start : line.find('"', start + 5);

			if (end != std::string::npos)
			{
				urls.push_back(line.substr(start + 5, end - start - 5));
			}
		}
		else if (line.starts_with("#EXTINF") && std::getline(iss, line))
		{
			if (!line.empty() && line.back() == '\r') line.pop_back();
			urls.push_back(line);
		}
	}

	if (urls.empty())
	{
		err::log("the track's playlist has no segments");
		return false;
	}

	const std::vector<net::Response> segments = net::get_all(urls, 20);

	size_t total = 0;

	for (const net::Response& r : segments)
	{
		if (r.status_code != 200)
		{
			err::log_net(r);
			return false;
		}

		total += r.text.size();
	}

	audio.reserve(total);

	for (const net::Response& r : segments)
	{
		audio += r.text;
	}

	return true;
}

void sc_upload::write_tags(const std::string& path, const std::string& name) const
{
	// TagLib assumes Latin-1 for std::string, ours are UTF-8

	auto utf8 = [](const std::string& text) { return TagLib::String(text, TagLib::String::UTF8); };

	const fs::path file_path = util::to_path(path); // TagLib may keep the pointer to its name
	TagLib::MP4::File file(file_path.c_str());
	TagLib::MP4::Tag* const tag = file.tag();

	if (!file.isValid() || !tag)
	{
		err::warn("could not add tags to \"{}\"", name);
		return;
	}

	tag->setTitle(utf8(this->title));
	tag->setArtist(utf8(this->artist));
	tag->setAlbum(utf8(this->album));
	tag->setGenre(utf8(this->genre));
	tag->setComment(utf8(this->comment));
	tag->setYear(this->year);

	if (this->number)
	{
		tag->setTrack(this->number);
	}

	if (!this->album_artist.empty())
	{
		tag->setItem("aART", TagLib::StringList(utf8(this->album_artist)));
	}

	if (const auto cover = this->get_cover_art())
	{
		const bool is_jpeg = cover->startsWith(TagLib::ByteVector("\xFF\xD8\xFF", 3));

		TagLib::MP4::CoverArtList covers;
		covers.append({ is_jpeg ? TagLib::MP4::CoverArt::JPEG : TagLib::MP4::CoverArt::PNG, *cover });
		tag->setItem("covr", TagLib::MP4::Item(covers));
	}

	if (!file.save())
	{
		err::warn("failed to save the tags of \"{}\"", name);
	}
}

bool sc_upload::download_track(void)
{
	const std::string name = util::sanitize_filename(cfg::overrides.audio_name.empty() ? this->file_title : cfg::overrides.audio_name) + ".m4a";
	const std::string path = cfg::audio_dir + name;
	const bool existed = fs::exists(util::to_path(path));

	if (existed && !confirm_overwrite(name))
	{
		return false;
	}

	std::string audio;
	if (!this->download_segments(audio))
	{
		return false;
	}

	// Written under a hidden name and renamed when finished, so players watching the folder
	// (Spotify) never see a file before its tags are written.

	const std::string temp_path = cfg::audio_dir + "." + name + ".part";
	std::error_code ec;

	auto discard = [&temp_path, &ec](void) { fs::remove(util::to_path(temp_path), ec); return false; };

	{
		std::ofstream file(util::to_path(temp_path), std::ios::binary | std::ios::trunc);
		file.write(audio.data(), static_cast<std::streamsize>(audio.size()));

		if (file.fail())
		{
			err::log("failed to write \"{}\"", temp_path);
			return discard();
		}
	}

	audio = {}; // free the memory before ffmpeg and TagLib run

	switch (media::repair_fragmented_m4a(temp_path)) // before tagging, the repair rewrites the file
	{
	case media::repair_result::repaired:
		break;

	case media::repair_result::ffmpeg_missing:
		err::warn("ffmpeg is not installed, \"{}\" will play but some players will not show its length", name);
		break;

	case media::repair_result::not_repaired:
		err::warn("could not repair \"{}\", it will play but some players will not show its length", name);
		break;

	case media::repair_result::invalid:
		err::log("the downloaded data for \"{}\" is not a valid audio file, nothing was saved", name);
		return discard();
	}

	this->write_tags(temp_path, name);

	fs::rename(util::to_path(temp_path), util::to_path(path), ec); // replaces an existing download of the track

	if (ec)
	{
		err::log("failed to move \"{}\" to \"{}\": {}", temp_path, path, ec.message());
		return discard();
	}

	history::add(path, existed);
	return true;
}

bool sc_upload::download_cover(void) const
{
	if (this->artwork_url.empty())
	{
		err::log("no cover art found for this track");
		return false;
	}

	const std::string* const data = this->artwork_data();
	if (!data)
	{
		return false;
	}

	const std::string name = util::sanitize_filename(cfg::overrides.image_name.empty() ? this->file_title : cfg::overrides.image_name) + ".jpg";
	const std::string path = cfg::image_dir + name;
	const bool existed = fs::exists(util::to_path(path));

	std::ofstream file(util::to_path(path), std::ios::binary | std::ios::trunc);
	file.write(data->data(), static_cast<std::streamsize>(data->size()));

	if (file.fail())
	{
		err::log("failed to write \"{}\"", path);
		return false;
	}

	history::add(path, existed);
	return true;
}
