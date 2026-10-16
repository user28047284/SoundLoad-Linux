#include "pch.hpp"
#include "logs.hpp"
#include "util.hpp"
#include "history.hpp"
#include "../platform/platform.hpp"

struct downloaded_file
{
	std::string path;             // absolute
	bool        replaced = false; // an earlier copy existed and was overwritten
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(downloaded_file, path, replaced)

// All files written by one run of sl: the song and, with -art, its cover
struct download_entry
{
	std::vector<downloaded_file> files;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(download_entry, files)

namespace history
{
	namespace fs = std::filesystem;

	constexpr size_t max_entries = 10;

	static std::vector<downloaded_file> this_run;

	static fs::path file_path(void)
	{
		const fs::path dir = platform::state_dir();
		return dir.empty() ? fs::path{} : dir / "history.json";
	}

	// Newest first. Files that no longer exist are left out, and so are entries with none left.
	static std::vector<download_entry> load(void)
	{
		std::ifstream file(file_path());
		if (file.fail())
		{
			return {};
		}

		std::vector<download_entry> entries;

		try
		{
			entries = Json::parse(file).get<std::vector<download_entry>>();
		}
		catch (...)
		{
			err::warn("could not read the download history, starting a new one");
			return {};
		}

		for (auto& entry : entries)
		{
			std::erase_if(entry.files, [](const downloaded_file& f) { return !fs::exists(util::to_path(f.path)); });
		}

		std::erase_if(entries, [](const download_entry& entry) { return entry.files.empty(); });
		return entries;
	}

	static void store(const std::vector<download_entry>& entries)
	{
		const fs::path path = file_path();
		if (path.empty())
		{
			return;
		}

		std::error_code ec;
		fs::create_directories(path.parent_path(), ec);

		std::ofstream file(path, std::ios::trunc);
		file << Json(entries).dump();

		if (file.fail())
		{
			err::warn("could not save the download history to \"{}\"", util::to_utf8(path));
		}
	}

	void add(const std::string& path, const bool replaced)
	{
		std::error_code ec;
		this_run.push_back({ util::to_utf8(fs::absolute(util::to_path(path), ec)), replaced });
	}

	void save(void)
	{
		if (this_run.empty())
		{
			return;
		}

		std::vector<download_entry> entries = load();

		// A song downloaded again is only listed under its newest download

		auto written_now = [](const downloaded_file& old)
			{
				return std::ranges::any_of(this_run, [&old](const downloaded_file& f) { return f.path == old.path; });
			};

		for (auto& entry : entries)
		{
			std::erase_if(entry.files, written_now);
		}

		std::erase_if(entries, [](const download_entry& entry) { return entry.files.empty(); });

		// The song is listed first, then its cover

		download_entry entry{ std::move(this_run) };
		this_run.clear();
		std::ranges::stable_partition(entry.files, [](const downloaded_file& f) { return f.path.ends_with(".m4a"); });

		entries.insert(entries.begin(), std::move(entry));

		if (entries.size() > max_entries)
		{
			entries.resize(max_entries);
		}

		store(entries);
	}

	// Parses "1", "1 3", "1-3", "2,4-5" into sorted 0-based indices. False on anything invalid.
	static bool parse_selection(std::string input, const size_t count, std::vector<size_t>& out)
	{
		std::ranges::replace(input, ',', ' ');

		auto to_number = [count](const std::string_view text, size_t& number)
			{
				const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), number);
				return ec == std::errc{} && end == text.data() + text.size() && number >= 1 && number <= count;
			};

		std::istringstream iss(input);

		for (std::string token; iss >> token;)
		{
			const size_t dash = token.find('-');
			size_t first = 0, last = 0;

			if (dash == std::string::npos)
			{
				if (!to_number(token, first)) return false;
				last = first;
			}
			else if (!to_number(std::string_view(token).substr(0, dash), first)
				|| !to_number(std::string_view(token).substr(dash + 1), last) || first > last)
			{
				return false;
			}

			for (size_t i = first; i <= last; ++i)
			{
				out.push_back(i - 1);
			}
		}

		std::ranges::sort(out);
		out.erase(std::ranges::unique(out).begin(), out.end());
		return !out.empty();
	}

	static std::string plural(const size_t count, const std::string_view word)
	{
		return std::format("{} {}{}", count, word, count == 1 ? "" : "s");
	}

	void undo(void)
	{
		std::vector<download_entry> entries = load();

		if (entries.empty())
		{
			std::cout << "Nothing to undo.\n";
			store(entries); // drops entries whose files were deleted by hand
			return;
		}

		auto file_name = [](const downloaded_file& f) { return util::to_utf8(util::to_path(f.path).filename()); };

		std::cout << "Recent downloads (newest first):\n\n";

		for (size_t i = 0; i < entries.size(); ++i)
		{
			const auto& files = entries[i].files;
			std::cout << "  " << std::setw(2) << (i + 1) << "  " << file_name(files[0]) << '\n';

			// Extra files and notes line up under the first file name
			for (size_t j = 1; j < files.size(); ++j)
			{
				std::cout << "      + " << file_name(files[j]) << '\n';
			}

			if (std::ranges::any_of(files, &downloaded_file::replaced))
			{
				std::cout << "      (replaced an older copy, which can't be restored)\n";
			}
		}

		std::cout << '\n';

		if (!platform::stdin_is_terminal())
		{
			std::cout << "Not run from a terminal, nothing was deleted.\n";
			return;
		}

		const std::string answer = util::ask("Delete which? e.g. 1, 1 3 or 1-3 (Enter to cancel): ");

		if (answer.find_first_not_of(" \t") == std::string::npos)
		{
			std::cout << "Cancelled.\n";
			return;
		}

		std::vector<size_t> picked;
		if (!parse_selection(answer, entries.size(), picked))
		{
			err::log("invalid selection \"{}\", use numbers from 1 to {} (e.g. 1, 1 3, 1-3)", answer, entries.size());
			return;
		}

		size_t file_count = 0;
		for (const size_t i : picked) file_count += entries[i].files.size();

		if (!util::ask_yes_no(std::format("Delete {} ({})?", plural(picked.size(), "download"), plural(file_count, "file"))))
		{
			std::cout << "Nothing was deleted.\n";
			return;
		}

		for (const size_t i : picked)
		{
			for (const auto& f : entries[i].files)
			{
				std::error_code ec;
				fs::remove(util::to_path(f.path), ec);

				if (ec)
				{
					err::warn("could not delete \"{}\": {}", f.path, ec.message());
				}
			}
		}

		// picked is sorted, so erasing from the back keeps the other indices valid

		for (auto it = picked.rbegin(); it != picked.rend(); ++it)
		{
			entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(*it));
		}

		store(entries);
		std::cout << "Deleted " << plural(picked.size(), "download") << ".\n";
	}
}
