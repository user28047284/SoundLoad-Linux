#include "pch.hpp"
#include "m4a.hpp"

#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

namespace media
{
	enum class run_result { ok, missing, failed };

	// Runs "ffmpeg -i input -map 0 -c copy -movflags +faststart -f ipod output". The format is
	// given explicitly because the file names used during a download don't end in ".m4a".
	// Spawned directly instead of through a shell so file names never need quoting.
	static run_result remux(const std::string& input, const std::string& output)
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
			return run_result::missing;
		}

		int status = 0;
		if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
		{
			return run_result::failed;
		}

		return run_result::ok;
	}

	repair_result repair_fragmented_m4a(const std::string& path)
	{
		namespace fs = std::filesystem;

		const std::string repaired = path + ".repair";
		std::error_code ec;

		switch (remux(path, repaired))
		{
		case run_result::missing:
			return repair_result::ffmpeg_missing;

		case run_result::failed:
			fs::remove(repaired, ec);
			return repair_result::invalid;

		case run_result::ok:
			break;
		}

		if (fs::file_size(repaired, ec) == 0 || ec)
		{
			fs::remove(repaired, ec);
			return repair_result::invalid; // no audio in it
		}

		fs::rename(repaired, path, ec);

		if (ec)
		{
			fs::remove(repaired, ec);
			return repair_result::not_repaired;
		}

		return repair_result::repaired;
	}
}
