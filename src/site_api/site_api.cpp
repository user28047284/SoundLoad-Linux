#include "pch.hpp"
#include "logs.hpp"
#include "../config/config.hpp"
#include "site_api.hpp"

//
//// HELPER FUNCTIONS
//

static net::Response get_timed(const std::string& url)
{
	return net::get(url, 4500);
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

void sc_upload::add_mp3_tag(const std::string& path) const
{
	// Creating tag & saving basic metadata

	TagLib::MPEG::File file(path.c_str());
	TagLib::ID3v2::Tag* const tag = file.ID3v2Tag(true);

	this->store_basic_tag_data(tag);

	if (!this->album_artist.empty())
	{
		auto frame = new TagLib::ID3v2::TextIdentificationFrame("TPE2", TagLib::String::UTF8); // taglib takes ownership
		frame->setText(TagLib::String(this->album_artist, TagLib::String::UTF8));
		tag->addFrame(frame);
	}

	// Setting cover art

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
	
	auto cover = new TagLib::ID3v2::AttachedPictureFrame; // not using std::make_unique because taglib takes ownership of the object
	cover->setMimeType(is_jpeg(raw_image) ? "image/jpeg" : "image/png");
	cover->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
	cover->setPicture(raw_image);
	tag->addFrame(cover);

	if (!file.save())
	{
		err::warn("failed to save ID3v2 tag");
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
	// This is used to download .m3u/.m3u8 files. Both file types are similar, 
	// with each containing an array of links that lead to file segments, which 
	// you must append to eachother in the order of which the links are provided.
	// All HLS transcodings I've seen lead to these, which is why we want progressive.

	std::istringstream iss(raw_data);
	std::string line;

	for (uint8_t active_threads = 0; std::getline(iss, line);)
	{
		if (this->f.is_m4a_media && line.starts_with("#EXT-X-MAP:URI="))
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
	// We prefer progressive transcodings over HLS because progressive transcodings only require 
	// one request to download the entire track, but HLS requires 10+ requests and forces you to 
	// parse a manifest to get each link. Multi-threading significantly improves this process, but 
	// even if it took the exact same amount of time as progressive downloads, we will want to 
	// minimize requests to reduce the chances of rate limiting.

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

	bool found = false, progressive_found = false;

	for (const auto& transcoding : list)
	{
		// AAC transcodings are highest quality, and provide .m4a files rather than .mp3.
		// You can't play these files Spotify or Apple Music so don't use for local files.

		if (transcoding["preset"] != "aac_160k")
		{
			const auto format = transcoding["format"];
			const auto protocol = format["protocol"];

			if (protocol == "progressive" || format["mime_type"] == "audio/mpeg")
			{
				this->streaming_url = transcoding["url"].get<std::string>() + "?client_id=" + cfg::client_id;
				found = true;

				if (protocol != "hls")
				{
					progressive_found = true;
					break;
				}
			}
		}
		else if (cfg::f.use_aac)
		{
			this->streaming_url  = transcoding["url"].get<std::string>() + "?client_id=" + cfg::client_id;
			this->f.is_m4a_media = true;

			found = true;
			break;
		}
	}

	// SoundCloud no longer provides MPEG transcodings for many tracks. AAC is only used instead
	// if requested. aac_96k is only used without aac_160k, it's lower quality than MPEG (128k).

	if (!found)
	{
		for (const char* preset : { "aac_160k", "aac_96k" })
		{
			for (const auto& transcoding : list)
			{
				if (transcoding["preset"] == preset)
				{
					this->streaming_url  = transcoding["url"].get<std::string>() + "?client_id=" + cfg::client_id;
					this->f.is_m4a_media = true;
					found = true;
					break;
				}
			}

			if (!found) continue;

			if (!cfg::f.use_aac && !cfg::f.use_aac_fallback)
			{
				err::log("no MP3 stream is available for this track, use -aac to download it as .m4a (or -aac-fallback to do this automatically)");
				return false;
			}

			if (!cfg::f.use_aac)
			{
				err::warn("no MP3 stream is available for this track, downloading AAC (.m4a) instead");
			}

			if (std::string_view(preset) == "aac_96k")
			{
				err::warn("only a 96 kbps AAC stream is available for this track");
			}

			break;
		}
	}

	if (!found)
	{
		err::log("failed to locate valid media transcoding");
		return false;
	}

	this->f.is_hls_mpeg = !progressive_found;
	return true;
}

bool sc_upload::download_track(void)
{
	// Downloading track (MP3 or M4A)

	net::Response r = get_timed(this->streaming_url);
	if (request_failed(r)) return false;

	r = get_timed(Json::parse(r.text)["url"].get<std::string>());
	if (r.status_code != 200) return false;

	this->streaming_url.clear();
	
	// Finalizing download

	std::vector<std::shared_future<net::Response>> hls_parts;
	const char* raw_audio  = nullptr;
	size_t      audio_size = 0;
	const bool  use_parts  = this->f.is_hls_mpeg || this->f.is_m4a_media;

	if (!use_parts)
	{
		raw_audio  = r.text.data();
		audio_size = r.text.size();
	}
	else if (!this->parse_manifest(r.text, hls_parts))
	{
		return false;
	}

	// Writing track to disk

	const std::string& file_name = cfg::g_data.audio_name.empty() ? this->title : cfg::g_data.audio_name;
	const std::string path = cfg::audio_dir + sanitize_filename(file_name) + (this->f.is_m4a_media ? ".m4a" : ".mp3");

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (file.fail())
	{
		err::log("failed to create file at \"{}\"", path);
		return false;
	}

	bool failed = false;

	if (use_parts)
	{
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
	}
	else
	{
		file.write(raw_audio, audio_size);
		failed = file.fail();
	}

	file.close();
	hls_parts.clear();

	if (failed)
	{
		err::log("failed to write to file \"{}\"", path);
		return false;
	}

	// Adding tag

	if (this->f.is_m4a_media)
	{
		this->add_m4a_tag(path);
	}
	else
	{
		this->add_mp3_tag(path);
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

	const std::string& file_name = cfg::g_data.image_name.empty() ? this->title : cfg::g_data.image_name;
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

	// Getting artwork URL

	this->artwork_url = post_data.value("artwork_url", post_data.value("avatar_url"));
	if (!this->artwork_url.empty())
	{
		fix_artwork_url(this->artwork_url);
	}

	// Getting MP3 artwork source

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