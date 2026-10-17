#include "pch.hpp"
#include "util.hpp"
#include "../platform/platform.hpp"
#include "m4a.hpp"

namespace media
{
	repair_result repair_fragmented_m4a(const std::string& path)
	{
		namespace fs = std::filesystem;

		const std::string repaired = path + ".repair";
		std::error_code ec;

		// The format is explicit because the temp file name doesn't end in .m4a; -c copy doesn't re-encode

		const platform::run_result result = platform::run_program(
			{
				"ffmpeg", "-nostdin", "-v", "error", "-y",
				"-i", path,
				"-map", "0", "-c", "copy", "-movflags", "+faststart", "-f", "ipod",
				repaired
			});

		switch (result)
		{
		case platform::run_result::missing:
			return repair_result::ffmpeg_missing;

		case platform::run_result::failed:
			fs::remove(util::to_path(repaired), ec);
			return repair_result::invalid;

		case platform::run_result::ok:
			break;
		}

		if (fs::file_size(util::to_path(repaired), ec) == 0 || ec)
		{
			fs::remove(util::to_path(repaired), ec);
			return repair_result::invalid; // no audio in it
		}

		fs::rename(util::to_path(repaired), util::to_path(path), ec);

		if (ec)
		{
			fs::remove(util::to_path(repaired), ec);
			return repair_result::not_repaired;
		}

		return repair_result::repaired;
	}
}
